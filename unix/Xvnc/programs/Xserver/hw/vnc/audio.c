#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <sys/un.h>
#include <errno.h>
#include <sys/stat.h>

#include "rfb.h"

#include "h264/common.h"

#define PA_SOCK_FOLDER "/tmp/.xvnc"
#define PA_SOCK_PATH "/tmp/.xvnc/xrdp_chansrv_audio_out_socket_%s"

#define RCV_SOCK_BUF_LEN 131080

int            a_sockfd;

Bool           run_state = FALSE;
int            pa_clients = 0;
int            vnc_clients = 0;
int            stream_state = 1;

CARD32         audioUpdateTime = 10; /* ms */
u_char*        audioBufPtr;
size_t         bufTotalSize, bufFreeSize;
size_t         bufUnsubmittedSize = 0;
size_t         bufSubmittedHead = 0;
size_t         bufUnsubmittedHead = 0;

pthread_t      tid_audio_srv = 0;
pthread_t      tid_audio_client = 0;


static void scheduleAudioUpdate(rfbClientPtr cl);
void AudioNotifyClient(rfbClientPtr cl, int cmd);

/* Audio buffer delivery timer callback */
/* Sends audio buffer content and re-schedules timer again */
static CARD32 audioUpdateCallback(OsTimerPtr timer, CARD32 now, pointer arg)
{
  rfbClientPtr cl = (rfbClientPtr)arg;

  if(vnc_clients == 0)
    return 0;

  if(cl->audioUpdateScheduled) {
    if(bufUnsubmittedSize) rfbAudioSendData(cl);
    if(stream_state != cl->streamState) {
           cl->streamState = stream_state;
           AudioNotifyClient(cl, stream_state);
    }
    cl->audioUpdateScheduled = FALSE;
    scheduleAudioUpdate(cl);
  }

  return 0;
}

/* Schedule audio buffer to the client delivery by timer */
static void scheduleAudioUpdate(rfbClientPtr cl)
{
    if(!cl->audioUpdateScheduled) {
       cl->audioUpdateScheduled = TRUE;
       cl->audioUpdateTimer = TimerSet(cl->audioUpdateTimer, 0,
                                       audioUpdateTime,
                                       audioUpdateCallback, cl);
    }
}

/* Notify client about audio stream to begin/end */
void AudioNotifyClient(rfbClientPtr cl, int cmd)
{
    u_char t_msg[4];

    switch (cmd) {
    case 0:
        t_msg[0] = rfbQEMUServer;
        t_msg[1] = rfbQEMUServerAudio;
        t_msg[2] = (rfbQEMUServerAudioBegin >> 8) & 0xFF;
        t_msg[3] = rfbQEMUServerAudioBegin & 0xFF;
        sendOrQueueData(cl, t_msg, 4, 1);
        break;
    case 1:
        t_msg[0] = rfbQEMUServer;
        t_msg[1] = rfbQEMUServerAudio;
        t_msg[2] = rfbQEMUServerAudioEnd;
        t_msg[3] = rfbQEMUServerAudioEnd;
        sendOrQueueData(cl, t_msg, 4, 1);
        bufUnsubmittedSize = bufSubmittedHead = bufUnsubmittedHead = 0;
        bufFreeSize = cl->audioBufSize * (cl->numberOfChannels << (cl->sampleFormat >> 1));
        break;
    }
}

/* Send audio buffer content to the client */
Bool rfbAudioSendData(rfbClientPtr cl)
{
    u_char t_msg[8];

    t_msg[0] = rfbQEMUServer;
    t_msg[1] = rfbQEMUServerAudio;
    t_msg[2] = (rfbQEMUServerAudioData >> 8) & 0xFF;
    t_msg[3] = rfbQEMUServerAudioData & 0xFF;

    while (bufUnsubmittedSize != 0 && !stream_state) {
        size_t io_bytes = bufUnsubmittedSize;
        if (io_bytes + bufSubmittedHead > bufTotalSize)
            io_bytes = bufTotalSize - bufSubmittedHead;
#ifdef DEBUG
        rfbLog("%s: bufFreeSize=%d bufUnsubmittedHead=%d bufUnsubmittedSize=%d bufSubmittedHead=%d\n", __FUNCTION__, bufFreeSize, bufUnsubmittedHead, bufUnsubmittedSize, bufSubmittedHead );
#endif
        if(!io_bytes)
            break;

        t_msg[4] = (io_bytes >> 24) & 0xFF;
        t_msg[5] = (io_bytes >> 16) & 0xFF;
        t_msg[6] = (io_bytes >> 8) & 0xFF;
        t_msg[7] = io_bytes & 0xFF;

        if(!cl->streamState) {
            sendOrQueueData(cl, t_msg, 8, 0);
            sendOrQueueData(cl, audioBufPtr + bufSubmittedHead, io_bytes, 1);
        }
        bufSubmittedHead = ((bufSubmittedHead + io_bytes) & (bufTotalSize - 1));
        bufUnsubmittedSize -= io_bytes;
        bufFreeSize += io_bytes;
    }

    return TRUE;
}

/* Acknowledge client we have audio support & start periodic audio buffer delivery timer */
Bool rfbAudioSendAck(rfbClientPtr cl)
{
    rfbFramebufferUpdateMsg msg;
    rfbFramebufferUpdateRectHeader header;

    msg.type = rfbFramebufferUpdate;
    msg.pad = 0;
    msg.nRects = Swap16IfLE(1);
    if(!sendOrQueueData(cl, (u_char*)&msg, sz_rfbFramebufferUpdateMsg, 0)) {
        rfbLog("%s: error sending framebuffer update\n");
        return FALSE;
    }

    header.r.x = 0;
    header.r.y = 0;
    header.r.w = 0;
    header.r.h = 0;
    header.encoding = Swap32IfLE(rfbEncodingAudio);
    if(!sendOrQueueData(cl, (u_char*)&header, sz_rfbFramebufferUpdateRectHeader, 1)) {
        rfbLog("%s: error sending framebuffer update\n");
        return FALSE;
    }


    return TRUE;
}

/* PA sink receiver thread */
/* Just receives audio chunks and put it to the circular buffer */
void *AudioClientThread(void *arg) {

    unsigned int len;
    int sock_n;

    u_char buff[RCV_SOCK_BUF_LEN];

    if (NULL != arg) {
        sock_n = *((int *)arg);
    } else {
        rfbLog("%s: arg=NULL, return\n", __FUNCTION__);
        return NULL;
    }

    while(len = recv(sock_n, &buff, 8, 0), len > 0 && pa_clients) {

        if(!vnc_clients)
            continue;

        uint32_t as_id = *((uint32_t *) &buff[0]);
        uint32_t as_size = *((uint32_t *) &buff[4]);
        if((as_id & ~3) || (as_size > RCV_SOCK_BUF_LEN) || (as_size < 8))
        {
#ifdef DEBUG
            rfbLog("%s: bad message id %d size %d\n", __FUNCTION__, as_id, as_size);
#endif
          continue;
        }

        if(stream_state != as_id) {
           stream_state = as_id;
#ifdef DEBUG
           rfbLog("%s: set stream state to %d\n", __FUNCTION__, stream_state);
#endif
           continue;
        }

       len = recv(sock_n, &buff, as_size - 8, 0);
       if(len == -1) {
          rfbLog("%s: recv error\n", __FUNCTION__);
          continue;
       }

       if((len != 0) && !stream_state) {

        size_t bytes_left_to_copy = len;
        size_t b_offset = 0;

        while (bytes_left_to_copy != 0) {
            size_t bytes_to_copy = bytes_left_to_copy;

            if (bytes_to_copy > bufFreeSize)
                bytes_to_copy = bufFreeSize;

            if (bytes_to_copy + bufUnsubmittedHead > bufTotalSize)
                bytes_to_copy = bufTotalSize - bufUnsubmittedHead;

            if (bytes_to_copy == 0)
                break;

            memcpy(audioBufPtr + bufUnsubmittedHead, buff + b_offset, bytes_to_copy);
            bufUnsubmittedHead  = ((bufUnsubmittedHead + bytes_to_copy) & (bufTotalSize - 1));
            bufFreeSize        -= bytes_to_copy;
            bufUnsubmittedSize += bytes_to_copy;
            b_offset           += bytes_to_copy;
            bytes_left_to_copy -= bytes_to_copy;
        }
#ifdef DEBUG
        rfbLog("%s: len=%d bufFreeSize=%d bufUnsubmittedHead=%d bufUnsubmittedSize=%d bufSubmittedHead=%d\n", __FUNCTION__, len, bufFreeSize, bufUnsubmittedHead, bufUnsubmittedSize, bufSubmittedHead );
#endif
      }
    }
    rfbLog("%s: exiting client thread..\n", __FUNCTION__);
    close(sock_n);
    return NULL;
}

void *AudioServerThread(void *) {

    struct sockaddr_un remote;
    int t, err;
    int sock_n = 0;

    pthread_detach(pthread_self());

    while(run_state) {
        memset(&tid_audio_client, 0, sizeof(pthread_t));
        rfbLog("%s: waiting for PA sink connection\n", __FUNCTION__);
        sock_n = accept(a_sockfd, (struct sockaddr *)&remote, &t);
        rfbLog("%s: accept incoming PA sink connection %d\n", __FUNCTION__, sock_n);
        pa_clients++;
        if (sock_n > 0) {
            err = pthread_create(&tid_audio_client, NULL, AudioClientThread, &sock_n);
            if (err != 0) {
                rfbLog("Failed to create client audio thread\n");
                pa_clients--;
            }
        }

        if (tid_audio_client)
            pthread_join(tid_audio_client, NULL);

        rfbLog("%s: PA sink %d has gone..\n", __FUNCTION__, sock_n);

        close(sock_n);
        pa_clients--;
    }
    rfbLog("%s: exit thread\n", __FUNCTION__);
    pthread_exit(0);
    return NULL;
}

Bool rfbAudioInit(rfbClientPtr cl)
{
    struct sockaddr_un local;
    int len;
    char socket_path[128];
    Bool ret = TRUE;

    vnc_clients++;

    rfbLog("%s: client audio sampling freq %d\n", __FUNCTION__, cl->samplingFreq);
    rfbLog("%s: client audio sample format %d\n", __FUNCTION__, cl->sampleFormat);
    rfbLog("%s: client audio channels %d\n", __FUNCTION__, cl->numberOfChannels);

    size_t buf_estim_size = (4 * 1000 * cl->samplingFreq) / 1000;
    cl->audioBufSize = 1;
    while (cl->audioBufSize < buf_estim_size)
        cl->audioBufSize <<= 1;

    scheduleAudioUpdate(cl);

    if(run_state) {
      rfbLog("%s: audio processing thread is already running\n", __FUNCTION__);
      return ret;
    }

    bufTotalSize = cl->audioBufSize * (cl->numberOfChannels << (cl->sampleFormat >> 1));
    bufFreeSize = bufTotalSize;
    audioBufPtr = (u_char*)malloc(bufTotalSize);
    rfbLog("%s: audio buffer size: %lu\n", __FUNCTION__, bufTotalSize);

    snprintf(socket_path, sizeof(socket_path), PA_SOCK_PATH, display);
    rfbLog("%s: audio socket path: %s\n", __FUNCTION__, socket_path);

    mkdir(PA_SOCK_FOLDER, 0);
    chmod(PA_SOCK_FOLDER, 0777);
    remove(socket_path);

    if((a_sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        rfbLog("%s: error creating audio socket %s\n", __FUNCTION__, PA_SOCK_PATH);
        ret = FALSE;
        return ret;
    }

    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, socket_path);

    len = strlen(local.sun_path) + sizeof(local.sun_family);
    if(bind(a_sockfd, (struct sockaddr *)&local, len) == -1)
    {
        rfbLog("%s: audio socket binding error\n", __FUNCTION__);
        ret = FALSE;
        close(a_sockfd);
        return ret;
    }

    if(listen(a_sockfd, 2) == -1)
    {
        rfbLog("%s: audio socket listen error\n", __FUNCTION__);
        ret = FALSE;
        close(a_sockfd);
        return ret;
    }

    run_state = TRUE;

    int err = pthread_create(&tid_audio_srv, NULL, AudioServerThread, NULL);
    if (err != 0) {
        rfbLog("%s: failed to create audio server thread\n", __FUNCTION__);
        ret = FALSE;
        close(a_sockfd);
        run_state = FALSE;
    }

    return ret;
}

void rfbAudioClose(rfbClientPtr cl)
{
    cl->audioUpdateScheduled = FALSE;
    if(tid_audio_client)
      pthread_cancel(tid_audio_client);
    rfbLog("Close audio socket\n");
    vnc_clients--;
}
