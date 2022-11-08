#include "common.h"

void fillInputBuffer(char *buffer, int i, int frame_width, int frame_height) {
    //generate sample image data
    u_int32_t offset = 0;
    u_int32_t stride = frame_width * 4;
    int x, y;
    for(y = 0; y < frame_height; y++) {
        for(x = 0; x < frame_width; x++) {
            buffer[offset+0] = (u_char)(x + (y % 255) % 255);
            buffer[offset+1] = (u_char)((x + i + ((y + i) % 255)) % 255);
            buffer[offset+2] = (u_char)((x + i*2 + ((y + i*2) % 255)) % 255);

            buffer[offset+3] = 255;
            offset += 4;
        }
    }
}

void rgba2Yuv(uint8_t *destination, uint8_t *rgb, size_t width, size_t height)
{
    size_t image_size = width * height;
    size_t upos = image_size;
    size_t vpos = upos + upos / 4;
    size_t i = 0;
    size_t line;
    size_t x;

    for( line = 0; line < height; ++line )
    {
        if( !(line % 2) )
        {
            for( x = 0; x < width; x += 2 )
            {
                uint8_t b = rgb[4 * i];
                uint8_t g = rgb[4 * i + 1];
                uint8_t r = rgb[4 * i + 2];

                destination[i++] = ((66*r + 129*g + 25*b) >> 8) + 16;

                destination[upos++] = ((-38*r + -74*g + 112*b) >> 8) + 128;
                destination[vpos++] = ((112*r + -94*g + -18*b) >> 8) + 128;

                b = rgb[4 * i];
                g = rgb[4 * i + 1];
                r = rgb[4 * i + 2];

                destination[i++] = ((66*r + 129*g + 25*b) >> 8) + 16;
            }
        }
        else
        {
            for( x = 0; x < width; x += 1 )
            {
                uint8_t b = rgb[4 * i];
                uint8_t g = rgb[4 * i + 1];
                uint8_t r = rgb[4 * i + 2];

                destination[i++] = ((66*r + 129*g + 25*b) >> 8) + 16;
            }
        }
    }
}


Bool sendFramebufferUpdateMsg(rfbClientPtr cl, int x, int y, int w, int h, u_char *data, size_t size) {
    Bool result = TRUE;
    rfbFramebufferUpdateMsg msg;
    rfbFramebufferUpdateRectHeader header;
    u_char t_header[8];

    msg.type = rfbFramebufferUpdate;
    msg.pad = 0;
    msg.nRects = Swap16IfLE(1);

    if(!sendOrQueueData(cl, (u_char*)&msg, sz_rfbFramebufferUpdateMsg, 0)) {
        result = FALSE;
        goto error;
    }

    header.r.x = Swap16IfLE(x);
    header.r.y = Swap16IfLE(y);
    header.r.w = Swap16IfLE(w);
    header.r.h = Swap16IfLE(h);
    header.encoding = Swap32IfLE(rfbEncodingH264);

    if(!sendOrQueueData(cl, (u_char*)&header, sz_rfbFramebufferUpdateRectHeader, 0)) {
        result = FALSE;
        goto error;
    }

    memset(&t_header, 0, 8);
    t_header[3] = size;
    t_header[2] = size>>8;
    t_header[1] = size>>16;
    t_header[0] = size>>24;
    t_header[7] = 0;

    if(!sendOrQueueData(cl, t_header, 8, 0)) {
        result = FALSE;
        goto error;
    }

    if(!sendOrQueueData(cl, data, size, 1)) {
        result = FALSE;
        goto error;
    }

    error:
    return result;
}

Bool sendFramebufferUpdateMsg2(rfbClientPtr cl, int x, int y, int w, int h, u_char *data, size_t size, int forceFlush) {
    Bool result = TRUE;
    rfbFramebufferUpdateMsg msg;
    rfbFramebufferUpdateRectHeader header;

    msg.type = rfbFramebufferUpdate;
    msg.pad = 0;
    msg.nRects = Swap16IfLE(1);

    if(!sendOrQueueData(cl, (u_char*)&msg, sz_rfbFramebufferUpdateMsg, 0)) {
        result = FALSE;
        goto error;
    }

    header.r.x = Swap16IfLE(x);
    header.r.y = Swap16IfLE(y);
    header.r.w = Swap16IfLE(w);
    header.r.h = Swap16IfLE(h);
    header.encoding = Swap32IfLE(rfbEncodingH264);

    if(!sendOrQueueData(cl, (u_char*)&header, sz_rfbFramebufferUpdateRectHeader, 0)) {
        result = FALSE;
        goto error;
    }

    if(!sendOrQueueData(cl, data, size, forceFlush)) {
        result = FALSE;
        goto error;
    }

    error:
    return result;
}

Bool sendOrQueueData(rfbClientPtr cl, u_char* data, int size, int forceFlush) {
    Bool result = TRUE;
    if (size > UPDATE_BUF_SIZE) {
        rfbLog("H264: send request size (%d) exhausts UPDATE_BUF_SIZE (%d) -> increase send buffer\n", size, UPDATE_BUF_SIZE);
        result = FALSE;
        goto error;
    }

    if(ublen + size > UPDATE_BUF_SIZE) {
        if(!rfbSendUpdateBuf(cl)) {
            rfbLogPerror("H264: could not send.\n");
            result = FALSE;
            goto error;
        }
    }

    memcpy(&updateBuf[ublen], data, size);
    ublen += size;

    if(forceFlush) {
#ifdef DEBUG
        rfbLog("flush H264 data %d (payloadSize=%d)\n",cl->ublen,cl->ublen - sz_rfbFramebufferUpdateMsg - sz_rfbFramebufferUpdateRectHeader);
#endif
        if(!rfbSendUpdateBuf(cl)) {
            rfbLogPerror("H264: could not send.\n");
            result = FALSE;
        }
    }

    error:
    return result;
}

void rfbH264ContextReset(rfbClientPtr cl) {
#ifdef DEBUG
    rfbLog("Performing H264 contexts reset\n");
#endif
    u_char t_header[8];
    memset(&t_header, 0, 8);
    t_header[7] = 0x2;
    if(!sendFramebufferUpdateMsg2(cl, 0, 0, rfbFB.width, rfbFB.height, t_header, 8, 1)) {
        rfbLogPerror("Error H264 context reset\n");
    }

}

uint64_t getTimeNowMs() {
    struct timespec now = {};
    clock_gettime( CLOCK_MONOTONIC_RAW, &now );
    uint64_t now_ns = (uint64_t)now.tv_sec * UINT64_C(1000) + (uint64_t)now.tv_nsec/1000000;
    return now_ns;
}
