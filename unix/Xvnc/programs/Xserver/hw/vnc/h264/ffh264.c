#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>

#include "common.h"

//#define DEBUG 1

static AVCodecContext *av_context = NULL;
static AVBufferRef *hw_device = NULL;
static AVFrame* hwFrame = NULL;
static AVFrame *frame = NULL;
struct SwsContext *sws_context = NULL;

int iw,ih;

Bool hwAcc = FALSE;

char h264conf_path[PATH_MAX];

typedef struct {
      float        Fps;
      int          Bitrate;
      int          GopSize;
      int          MaxBFrames;
      int          RefFrames;
      char         Profile[32];
      char         Preset[32];
      char         Threads[8];
      int          GlobalQ;

      char         Crf[8];
      char         CrfMax[8];
      char         Qp[8];
      char         QpMin[8];
      char         QpMax[8];
      char         aqMode[8];
      char         aqStrength[8];

      Bool         HwAccel;
      char         VAAPIDev[128];
      char         hRcMode[8];
      char         hQuality[8];
      char         hCoder[8];
      char         hProfile[64];
      char         hAsyncDepth[8];
      char         hLevelIdc[8];
} H264Profile;

H264Profile prof[] = {{25.0, 1500000, 120, -1, -1, "baseline", "ultrafast", "4", 0,
                        "-1", "0", "-1", "0", "0", "-1", "0",
                        0, "/dev/dri/renderD128", "VBR", "7", "vlc", "constrained_baseline", "64", "4.1" }};

#define MAX_LINEB 512
#define MAX_ARGB 128
#define MAX_VALB 64

int H264CfgRead() {
    FILE *fn;
    char line[MAX_LINEB];
    char val[MAX_VALB];
    char param[MAX_ARGB];
    int v;

    fn = fopen(h264conf_path,"r");
    if (!fn) {
       rfbLog("No H264 config specified, using defaults\n");
       return -1;
    }
    while (fgets(line, MAX_LINEB, fn) != NULL) {
      if(line[0] == '#' || line[0] == ';') continue;
      if((strlen(line))>=3) {
         char *i = strchr(line,'=');
         if(i) {
           memset(param, 0, MAX_ARGB);
           memset(val, 0, MAX_VALB);
           strncpy(param, line, (i-line));
           strcpy (val, line+(i-line)+1);
           if(!strcmp(param, "Fps")) {
             prof[0].Fps = atof(val);
           }
           else if(!strcmp(param, "Bitrate")) {
             prof[0].Bitrate = atoi(val);
           }
           else if(!strcmp(param, "GopSize")) {
             prof[0].GopSize = atoi(val);
           }
           else if(!strcmp(param, "MaxBFrames")) {
             prof[0].MaxBFrames = atoi(val);
           }
           else if(!strcmp(param, "RefFrames")) {
             prof[0].RefFrames = atoi(val);
           }
           else if(!strcmp(param, "Profile")) {
             strlcpy(prof[0].Profile, val, sizeof(prof[0].Profile)-1);
             prof[0].Profile[strcspn(prof[0].Profile, "\r\n")] = 0;
           }
           else if(!strcmp(param, "Preset")) {
             strlcpy(prof[0].Preset, val, sizeof(prof[0].Preset)-1);
             prof[0].Preset[strcspn(prof[0].Preset, "\r\n")] = 0;
           }
           else if(!strcmp(param, "Threads")) {
             strlcpy(prof[0].Threads, val, sizeof(prof[0].Threads)-1);
             prof[0].Threads[strcspn(prof[0].Threads, "\r\n")] = 0;
           }
           else if(!strcmp(param, "GlobalQ")) {
             prof[0].GlobalQ = atoi(val);
           }
           else if(!strcmp(param, "Crf")) {
             strlcpy(prof[0].Crf, val, sizeof(prof[0].Crf)-1);
             prof[0].Crf[strcspn(prof[0].Crf, "\r\n")] = 0;
           }
           else if(!strcmp(param, "CrfMax")) {
             strlcpy(prof[0].CrfMax, val, sizeof(prof[0].CrfMax)-1);
             prof[0].CrfMax[strcspn(prof[0].CrfMax, "\r\n")] = 0;
           }
           else if(!strcmp(param, "Qp")) {
             strlcpy(prof[0].Qp, val, sizeof(prof[0].Qp)-1);
             prof[0].Qp[strcspn(prof[0].Qp, "\r\n")] = 0;
           }
           else if(!strcmp(param, "QpMin")) {
             strlcpy(prof[0].QpMin, val, sizeof(prof[0].QpMin)-1);
             prof[0].QpMin[strcspn(prof[0].QpMin, "\r\n")] = 0;
           }
           else if(!strcmp(param, "QpMax")) {
             strlcpy(prof[0].QpMax, val, sizeof(prof[0].QpMax)-1);
             prof[0].QpMax[strcspn(prof[0].QpMax, "\r\n")] = 0;
           }
           else if(!strcmp(param, "aqMode")) {
             strlcpy(prof[0].aqMode, val, sizeof(prof[0].aqMode)-1);
             prof[0].aqMode[strcspn(prof[0].aqMode, "\r\n")] = 0;
           }
           else if(!strcmp(param, "aqStrength")) {
             strlcpy(prof[0].aqStrength, val, sizeof(prof[0].aqStrength)-1);
             prof[0].aqStrength[strcspn(prof[0].aqStrength, "\r\n")] = 0;
           }
           else if(!strcmp(param, "HwAccel")) {
             prof[0].HwAccel = atoi(val);
           }
           else if(!strcmp(param, "VAAPIDev")) {
             strlcpy(prof[0].VAAPIDev, val, sizeof(prof[0].VAAPIDev)-1);
             prof[0].VAAPIDev[strcspn(prof[0].VAAPIDev, "\r\n")] = 0;
           }
           else if(!strcmp(param, "hRcMode")) {
             strlcpy(prof[0].hRcMode, val, sizeof(prof[0].hRcMode)-1);
             prof[0].hRcMode[strcspn(prof[0].hRcMode, "\r\n")] = 0;
           }
           else if(!strcmp(param, "hQuality")) {
             strlcpy(prof[0].hQuality, val, sizeof(prof[0].hQuality)-1);
             prof[0].hQuality[strcspn(prof[0].hQuality, "\r\n")] = 0;
           }
           else if(!strcmp(param, "hCoder")) {
             strlcpy(prof[0].hCoder, val, sizeof(prof[0].hCoder)-1);
             prof[0].hCoder[strcspn(prof[0].hCoder, "\r\n")] = 0;
           }
           else if(!strcmp(param, "hProfile")) {
             strlcpy(prof[0].hProfile, val, sizeof(prof[0].hProfile)-1);
             prof[0].hProfile[strcspn(prof[0].hProfile, "\r\n")] = 0;
           }
           else if(!strcmp(param, "hAsyncDepth")) {
             strlcpy(prof[0].hAsyncDepth, val, sizeof(prof[0].hAsyncDepth)-1);
             prof[0].hAsyncDepth[strcspn(prof[0].hAsyncDepth, "\r\n")] = 0;
           }
           else if(!strcmp(param, "hLevelIdc")) {
             strlcpy(prof[0].hLevelIdc, val, sizeof(prof[0].hLevelIdc)-1);
             prof[0].hLevelIdc[strcspn(prof[0].hLevelIdc, "\r\n")] = 0;
           }
         }
      }
    }
    fclose(fn);
    return 0;
}


static enum AVPixelFormat get_vaapi_format(AVCodecContext*, const enum AVPixelFormat *pix_fmts)
{
    const enum AVPixelFormat *p;
    for (p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
        if (*p == AV_PIX_FMT_VAAPI)
            return *p;
    }
    rfbLog("Failed to get HW surface format.\n");
    return AV_PIX_FMT_NONE;
}

static void yuv_from_bgra(uint8_t *rgb) {
    const int in_linesize[1] = { 4 * av_context->width };
    sws_context = sws_getCachedContext(sws_context,
            av_context->width, av_context->height, AV_PIX_FMT_BGRA,
            av_context->width, av_context->height, AV_PIX_FMT_YUV420P,
            0, 0, 0, 0);
    sws_scale(sws_context, (const uint8_t * const *)&rgb, in_linesize, 0,
            av_context->height, frame->data, frame->linesize);
}

static void hw_from_bgra(uint8_t *rgb) {
    const int in_linesize[1] = { 4 * av_context->width };
    sws_context = sws_getCachedContext(sws_context,
            av_context->width, av_context->height, AV_PIX_FMT_BGRA,
            av_context->width, av_context->height, AV_PIX_FMT_NV12,
            0, 0, 0, 0);
    sws_scale(sws_context, (const uint8_t * const *)&rgb, in_linesize, 0,
            av_context->height, frame->data, frame->linesize);
}

static Bool initH264(rfbClientPtr cl) {
    Bool result = TRUE;
    int ret;

    H264CfgRead();

    if(rfbFB.depth != 32) {
        rfbLogPerror("Screen depth of 32 bits required for h264 encoding.\n");
        result = FALSE;
        goto error;
    }
#ifdef DEBUG
    rfbLog("Init H264 encoder instance: %dx%d@%d\n", rfbFB.width, rfbFB.height, rfbFB.depth);
#endif
    hwAcc = prof[0].HwAccel;

    const char* encoderName = hwAcc ? "h264_vaapi": "libx264";
    const AVCodec* videoCodec = avcodec_find_encoder_by_name(encoderName);
    av_context = avcodec_alloc_context3(videoCodec);
    if (!av_context) {
        rfbLogPerror("Could not allocate video codec context\n");
        result = FALSE;
        goto error;
    }

    if(!hwAcc) {
        av_opt_set(av_context->priv_data, "profile", prof[0].Profile, 0);
        av_opt_set(av_context->priv_data, "preset", prof[0].Preset, 0);
        av_opt_set(av_context->priv_data, "tune", "zerolatency", 0);
        av_opt_set(av_context->priv_data, "threads", prof[0].Threads, 0);

//        av_opt_set(av_context->priv_data, "rc-lookahead", "50", 0);

        if(strcmp(prof[0].Crf, "-1")) {
            av_opt_set(av_context->priv_data, "crf", prof[0].Crf, 0);
            av_opt_set(av_context->priv_data, "crf-max", prof[0].CrfMax, 0);
        }
        if(strcmp(prof[0].Qp, "-1")) {
            av_opt_set(av_context->priv_data, "qp", prof[0].Qp, 0);
        }
        if(strcmp(prof[0].QpMin, "-1")) {
            av_opt_set(av_context->priv_data, "qpmin", prof[0].QpMin, 0);
        }
        if(strcmp(prof[0].QpMax, "-1")) {
            av_opt_set(av_context->priv_data, "qpmax", prof[0].QpMax, 0);
        }

        if(strcmp(prof[0].aqMode, "-1")) {
            av_opt_set(av_context->priv_data, "aq-mode", prof[0].aqMode, 0);
            av_opt_set(av_context->priv_data, "aq-strength", prof[0].aqStrength, 0);
        }
    }

    av_context->bit_rate = prof[0].Bitrate;
    av_context->width = rfbFB.width;
    av_context->height = rfbFB.height;
    av_context->sample_aspect_ratio = (AVRational){1, 1};
    av_context->pix_fmt = AV_PIX_FMT_YUV420P;
    av_context->time_base = (AVRational){1, prof[0].Fps};
    av_context->framerate = (AVRational){prof[0].Fps, 1};
    av_context->gop_size = prof[0].GopSize;
    if(prof[0].MaxBFrames != -1)
        av_context->max_b_frames = prof[0].MaxBFrames;
    if(prof[0].RefFrames != -1)
        av_context->refs = prof[0].RefFrames;

    if(hwAcc) {
        av_context->pix_fmt = AV_PIX_FMT_VAAPI;
        av_context->get_format = get_vaapi_format;

        ret = av_hwdevice_ctx_create(&hw_device, AV_HWDEVICE_TYPE_VAAPI, prof[0].VAAPIDev, NULL, 0);
        if (ret < 0) {
            rfbLog("Could not open VAAPI device: %s\n", av_err2str(ret));
            result = FALSE;
            goto error;
        }

        AVHWDeviceContext* deviceCtx = (AVHWDeviceContext*) hw_device->data;
        if(deviceCtx->type != AV_HWDEVICE_TYPE_VAAPI) {
            rfbLog("device type is not AV_HWDEVICE_TYPE_VAAPI\n");
            result = FALSE;
            goto error;
        }

        AVBufferRef *hw_frames_ref;
        AVHWFramesContext *frames_ctx = NULL;
        hw_frames_ref = av_hwframe_ctx_alloc(hw_device);
        if(hw_frames_ref == NULL) {
            rfbLog("Unable to allocate hw_frames_ref\n");
            result = FALSE;
            goto error;
        }

        frames_ctx = (AVHWFramesContext *)(hw_frames_ref->data);
        frames_ctx->format    = AV_PIX_FMT_VAAPI;
        frames_ctx->sw_format = AV_PIX_FMT_NV12;
        frames_ctx->width     = rfbFB.width;
        frames_ctx->height    = rfbFB.height;
        frames_ctx->initial_pool_size = 20;
        ret = av_hwframe_ctx_init(hw_frames_ref);
        if(ret != 0) {
            rfbLog("Unable to init hw context\n");
            av_buffer_unref(&hw_frames_ref);
            result = FALSE;
            goto error;
        }

        av_context->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
        if(av_context->hw_frames_ctx == NULL) {
            rfbLog("hw_frames_context == NULL\n");
            result = FALSE;
            goto error;
        }

        av_opt_set(av_context->priv_data, "rc_mode", prof[0].hRcMode, 0);
        av_opt_set(av_context->priv_data, "quality", prof[0].hQuality, 0);
        av_opt_set(av_context->priv_data, "coder", prof[0].hCoder, 0);
        av_opt_set(av_context->priv_data, "profile", prof[0].hProfile, 0);
        av_opt_set(av_context->priv_data, "async_depth", prof[0].hAsyncDepth, 0);
        av_opt_set(av_context->priv_data, "level", prof[0].hLevelIdc, 0);
//        av_opt_set(av_context->priv_data, "idr_interval", "125", 0);
//        av_opt_set(av_context->priv_data, "b_depth", "2", 0);

        if(prof[0].GlobalQ != 0)
            av_context->global_quality = prof[0].GlobalQ;
        av_buffer_unref(&hw_frames_ref);
    }

    iw = rfbFB.width;
    ih = rfbFB.height;


    ret = avcodec_open2(av_context, videoCodec, NULL);
    if (ret < 0) {
        rfbLog("Could not open video codec: %s\n", av_err2str(ret));
        result = FALSE;
        goto error;
    }

    frame = av_frame_alloc();
    if (!frame) {
        rfbLogPerror("Could not allocate video frame\n");
        result = FALSE;
        goto error;
    }

    if(!hwAcc) {
        frame->format = av_context->pix_fmt;
        frame->width  = av_context->width;
        frame->height = av_context->height;
        ret = av_image_alloc(frame->data, frame->linesize, av_context->width, av_context->height, av_context->pix_fmt, 32);
        if (ret < 0) {
            rfbLogPerror("Could not allocate raw picture buffer\n");
            goto error;
        }
    } else {
         frame->width  = av_context->width;
         frame->height = av_context->height;
         frame->format = AV_PIX_FMT_NV12;
         if ((ret = av_frame_get_buffer(frame, 32)) < 0) {
              rfbLog("Failed to allocate frame\n");
              result = FALSE;
              goto error;
        }
    }
error:
    return result;
}

Bool rfbSendFrameEncodingH264(rfbClientPtr cl) {
    Bool result = TRUE;
    int rv;
    int frameSize;
    int w = rfbFB.width;
    int h = rfbFB.height;

    int ret;

    if(av_context == NULL) {
        if(!initH264(cl)) {
            rfbLogPerror("Init H264 encoder error");
            result = FALSE;
            goto error;
        }
    }

    if(rfbFB.width != iw || rfbFB.height != ih) {
#ifdef DEBUG
        rfbLog("H264 resize to: %dx%d@%d\n", rfbFB.width, rfbFB.height, rfbFB.depth);
#endif
        rfbH264ContextReset(cl);
        rfbH264Cleanup(cl);
        return 1;
    }

    if(hwAcc) {
        hwFrame = av_frame_alloc();
        av_hwframe_get_buffer(av_context->hw_frames_ctx, hwFrame, 0);
        hw_from_bgra((u_char*)rfbFB.pfbMemory);
        av_hwframe_transfer_data(hwFrame, frame, 0);
        ret = avcodec_send_frame(av_context, hwFrame);
    } else {
        yuv_from_bgra((u_char*)rfbFB.pfbMemory);
        ret = avcodec_send_frame(av_context, frame);
    }
    if (ret < 0) {
        rfbLog("Fail to avcodec_send_frame ! error: %s\n", av_err2str(ret));
        result = FALSE;
        if(hwAcc)
            av_frame_free(&hwFrame);
        goto error;
    }

    AVPacket *pkt = NULL;
    pkt = av_packet_alloc();
    if (pkt) {
        while (ret >= 0) {
            ret = avcodec_receive_packet(av_context, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if(pkt->size) {
                if(!sendFramebufferUpdateMsg(cl, 0, 0, w, h, pkt->data, pkt->size)){
                    av_packet_unref(pkt);
                    result = FALSE;
                    break;
                }
            }
            av_packet_unref(pkt);
        }
        av_packet_free(&pkt);
    }
    if(hwAcc)
        av_frame_free(&hwFrame);
error:
    return result;
}

void rfbH264Cleanup(rfbClientPtr cl) {
#ifdef DEBUG
    rfbLog("H264 cleanup\n");
#endif
    if (av_context) {
        avcodec_free_context(&av_context);
        av_frame_free(&frame);
        if(hwAcc) {
            av_frame_free(&hwFrame);
            av_buffer_unref(&hw_device);
        }
        av_free(av_context);
        av_context = NULL;
    }
}
