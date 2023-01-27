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
static int h264_profile = 0;
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
      Bool         OpenCL;

      Bool         HwAccel;
      char         VAAPIDev[128];
      char         hRcMode[8];
      char         hQuality[8];
      char         hCoder[8];
      char         hProfile[64];
      char         hAsyncDepth[8];
      char         hLevelIdc[8];
} H264Profile;

H264Profile prof[9] = {{25.0, 1500000, 120, -1, -1, "baseline", "ultrafast", "4", 0,
                        "-1", "0", "-1", "0", "0", "-1", "0", 1,
                        0, "/dev/dri/renderD128", "VBR", "7", "vlc", "constrained_baseline", "64", "4.1" },
                        {25.0, 1500000, 120, -1, -1, "baseline", "ultrafast", "4", 0,
                        "-1", "0", "-1", "0", "0", "-1", "0", 1,
                        0, "/dev/dri/renderD128", "VBR", "7", "vlc", "constrained_baseline", "64", "4.1" },
                        {25.0, 1500000, 120, -1, -1, "baseline", "ultrafast", "4", 0,
                        "-1", "0", "-1", "0", "0", "-1", "0", 1,
                        0, "/dev/dri/renderD128", "VBR", "7", "vlc", "constrained_baseline", "64", "4.1" },
                        {25.0, 1500000, 120, -1, -1, "baseline", "ultrafast", "4", 0,
                        "-1", "0", "-1", "0", "0", "-1", "0", 1,
                        0, "/dev/dri/renderD128", "VBR", "7", "vlc", "constrained_baseline", "64", "4.1" },
                        {25.0, 1500000, 120, -1, -1, "baseline", "ultrafast", "4", 0,
                        "-1", "0", "-1", "0", "0", "-1", "0", 1,
                        0, "/dev/dri/renderD128", "VBR", "7", "vlc", "constrained_baseline", "64", "4.1" },
                        {25.0, 1500000, 120, -1, -1, "baseline", "ultrafast", "4", 0,
                        "-1", "0", "-1", "0", "0", "-1", "0", 1,
                        0, "/dev/dri/renderD128", "VBR", "7", "vlc", "constrained_baseline", "64", "4.1" },
                        {25.0, 1500000, 120, -1, -1, "baseline", "ultrafast", "4", 0,
                        "-1", "0", "-1", "0", "0", "-1", "0", 1,
                        0, "/dev/dri/renderD128", "VBR", "7", "vlc", "constrained_baseline", "64", "4.1" },
                        {25.0, 1500000, 120, -1, -1, "baseline", "ultrafast", "4", 0,
                        "-1", "0", "-1", "0", "0", "-1", "0", 1,
                        0, "/dev/dri/renderD128", "VBR", "7", "vlc", "constrained_baseline", "64", "4.1" },
                        {25.0, 1500000, 120, -1, -1, "baseline", "ultrafast", "4", 0,
                        "-1", "0", "-1", "0", "0", "-1", "0", 1,
                        0, "/dev/dri/renderD128", "VBR", "7", "vlc", "constrained_baseline", "64", "4.1" }};

#define MAX_LINEB 512
#define MAX_ARGB 128
#define MAX_VALB 64

int H264CfgRead() {
    FILE *fn;
    char line[MAX_LINEB];
    char val[MAX_VALB];
    char param[MAX_ARGB];
    int v, p_idx = 0;

    fn = fopen(h264conf_path,"r");
    if (!fn) {
       rfbLog("No H264 config specified, using defaults\n");
       return -1;
    }
    while (fgets(line, MAX_LINEB, fn) != NULL) {
      if(line[0] == '#' || line[0] == ';') continue;
      if((strlen(line))>=3) {
         if(!strncmp(line, "[profile_", 9)){
          memset(param, 0, MAX_ARGB);
          memset(val, 0, MAX_VALB);
          char *i = strchr(line,'_');
          strncpy(val, i+1, 1);
          p_idx = atoi(val);
          if(p_idx < 0 || p_idx > 9) {
            rfbLog("Invalid h264 profile number, must be in 0-9 range\n");
            p_idx = 0;
          }
          continue;
         }
         char *i = strchr(line,'=');
         if(i) {
           memset(param, 0, MAX_ARGB);
           memset(val, 0, MAX_VALB);
           strncpy(param, line, (i-line));
           strcpy (val, line+(i-line)+1);
           if(!strcmp(param, "Fps")) {
             prof[p_idx].Fps = atof(val);
           }
           else if(!strcmp(param, "Bitrate")) {
             prof[p_idx].Bitrate = atoi(val);
           }
           else if(!strcmp(param, "GopSize")) {
             prof[p_idx].GopSize = atoi(val);
           }
           else if(!strcmp(param, "MaxBFrames")) {
             prof[p_idx].MaxBFrames = atoi(val);
           }
           else if(!strcmp(param, "RefFrames")) {
             prof[p_idx].RefFrames = atoi(val);
           }
           else if(!strcmp(param, "Profile")) {
             strlcpy(prof[p_idx].Profile, val, sizeof(prof[p_idx].Profile)-1);
             prof[p_idx].Profile[strcspn(prof[p_idx].Profile, "\r\n")] = 0;
           }
           else if(!strcmp(param, "Preset")) {
             strlcpy(prof[p_idx].Preset, val, sizeof(prof[p_idx].Preset)-1);
             prof[p_idx].Preset[strcspn(prof[p_idx].Preset, "\r\n")] = 0;
           }
           else if(!strcmp(param, "Threads")) {
             strlcpy(prof[p_idx].Threads, val, sizeof(prof[p_idx].Threads)-1);
             prof[p_idx].Threads[strcspn(prof[p_idx].Threads, "\r\n")] = 0;
           }
           else if(!strcmp(param, "GlobalQ")) {
             prof[p_idx].GlobalQ = atoi(val);
           }
           else if(!strcmp(param, "Crf")) {
             strlcpy(prof[p_idx].Crf, val, sizeof(prof[p_idx].Crf)-1);
             prof[p_idx].Crf[strcspn(prof[p_idx].Crf, "\r\n")] = 0;
           }
           else if(!strcmp(param, "CrfMax")) {
             strlcpy(prof[p_idx].CrfMax, val, sizeof(prof[p_idx].CrfMax)-1);
             prof[p_idx].CrfMax[strcspn(prof[p_idx].CrfMax, "\r\n")] = 0;
           }
           else if(!strcmp(param, "Qp")) {
             strlcpy(prof[p_idx].Qp, val, sizeof(prof[p_idx].Qp)-1);
             prof[p_idx].Qp[strcspn(prof[p_idx].Qp, "\r\n")] = 0;
           }
           else if(!strcmp(param, "QpMin")) {
             strlcpy(prof[p_idx].QpMin, val, sizeof(prof[p_idx].QpMin)-1);
             prof[p_idx].QpMin[strcspn(prof[p_idx].QpMin, "\r\n")] = 0;
           }
           else if(!strcmp(param, "QpMax")) {
             strlcpy(prof[p_idx].QpMax, val, sizeof(prof[p_idx].QpMax)-1);
             prof[p_idx].QpMax[strcspn(prof[p_idx].QpMax, "\r\n")] = 0;
           }
           else if(!strcmp(param, "aqMode")) {
             strlcpy(prof[p_idx].aqMode, val, sizeof(prof[p_idx].aqMode)-1);
             prof[p_idx].aqMode[strcspn(prof[p_idx].aqMode, "\r\n")] = 0;
           }
           else if(!strcmp(param, "aqStrength")) {
             strlcpy(prof[p_idx].aqStrength, val, sizeof(prof[p_idx].aqStrength)-1);
             prof[p_idx].aqStrength[strcspn(prof[p_idx].aqStrength, "\r\n")] = 0;
           }
           else if(!strcmp(param, "OpenCL")) {
             prof[p_idx].OpenCL = atoi(val);
           }
           else if(!strcmp(param, "HwAccel")) {
             prof[p_idx].HwAccel = atoi(val);
           }
           else if(!strcmp(param, "VAAPIDev")) {
             strlcpy(prof[p_idx].VAAPIDev, val, sizeof(prof[p_idx].VAAPIDev)-1);
             prof[p_idx].VAAPIDev[strcspn(prof[p_idx].VAAPIDev, "\r\n")] = 0;
           }
           else if(!strcmp(param, "hRcMode")) {
             strlcpy(prof[p_idx].hRcMode, val, sizeof(prof[p_idx].hRcMode)-1);
             prof[p_idx].hRcMode[strcspn(prof[p_idx].hRcMode, "\r\n")] = 0;
           }
           else if(!strcmp(param, "hQuality")) {
             strlcpy(prof[p_idx].hQuality, val, sizeof(prof[p_idx].hQuality)-1);
             prof[p_idx].hQuality[strcspn(prof[p_idx].hQuality, "\r\n")] = 0;
           }
           else if(!strcmp(param, "hCoder")) {
             strlcpy(prof[p_idx].hCoder, val, sizeof(prof[p_idx].hCoder)-1);
             prof[p_idx].hCoder[strcspn(prof[p_idx].hCoder, "\r\n")] = 0;
           }
           else if(!strcmp(param, "hProfile")) {
             strlcpy(prof[p_idx].hProfile, val, sizeof(prof[p_idx].hProfile)-1);
             prof[p_idx].hProfile[strcspn(prof[p_idx].hProfile, "\r\n")] = 0;
           }
           else if(!strcmp(param, "hAsyncDepth")) {
             strlcpy(prof[p_idx].hAsyncDepth, val, sizeof(prof[p_idx].hAsyncDepth)-1);
             prof[p_idx].hAsyncDepth[strcspn(prof[p_idx].hAsyncDepth, "\r\n")] = 0;
           }
           else if(!strcmp(param, "hLevelIdc")) {
             strlcpy(prof[p_idx].hLevelIdc, val, sizeof(prof[p_idx].hLevelIdc)-1);
             prof[p_idx].hLevelIdc[strcspn(prof[p_idx].hLevelIdc, "\r\n")] = 0;
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

    h264_profile = cl->zlibCompressLevel;
#ifdef DEBUG
    rfbLog("Init H264 encoder instance with profile %d: %dx%d@%d\n", h264_profile, rfbFB.width, rfbFB.height, rfbFB.depth);
#endif
    hwAcc = prof[h264_profile].HwAccel;

    const char* encoderName = hwAcc ? "h264_vaapi": "libx264";
    const AVCodec* videoCodec = avcodec_find_encoder_by_name(encoderName);
    av_context = avcodec_alloc_context3(videoCodec);
    if (!av_context) {
        rfbLogPerror("Could not allocate video codec context\n");
        result = FALSE;
        goto error;
    }

    if(!hwAcc) {
        av_opt_set(av_context->priv_data, "profile", prof[h264_profile].Profile, 0);
        av_opt_set(av_context->priv_data, "preset", prof[h264_profile].Preset, 0);
        av_opt_set(av_context->priv_data, "tune", "zerolatency", 0);
        av_opt_set(av_context->priv_data, "threads", prof[h264_profile].Threads, 0);

        if(prof[h264_profile].OpenCL)
            av_opt_set(av_context->priv_data, "x264opts", "opencl", 0);

//        av_opt_set(av_context->priv_data, "rc-lookahead", "50", 0);

        if(strcmp(prof[h264_profile].Crf, "-1")) {
            av_opt_set(av_context->priv_data, "crf", prof[h264_profile].Crf, 0);
            av_opt_set(av_context->priv_data, "crf-max", prof[h264_profile].CrfMax, 0);
        }
        if(strcmp(prof[h264_profile].Qp, "-1")) {
            av_opt_set(av_context->priv_data, "qp", prof[h264_profile].Qp, 0);
        }
        if(strcmp(prof[h264_profile].QpMin, "-1")) {
            av_opt_set(av_context->priv_data, "qpmin", prof[h264_profile].QpMin, 0);
        }
        if(strcmp(prof[h264_profile].QpMax, "-1")) {
            av_opt_set(av_context->priv_data, "qpmax", prof[h264_profile].QpMax, 0);
        }

        if(strcmp(prof[h264_profile].aqMode, "-1")) {
            av_opt_set(av_context->priv_data, "aq-mode", prof[h264_profile].aqMode, 0);
            av_opt_set(av_context->priv_data, "aq-strength", prof[h264_profile].aqStrength, 0);
        }
    }

    av_context->bit_rate = prof[h264_profile].Bitrate;
    av_context->width = rfbFB.width;
    av_context->height = rfbFB.height;
    av_context->sample_aspect_ratio = (AVRational){1, 1};
    av_context->pix_fmt = AV_PIX_FMT_YUV420P;
    av_context->time_base = (AVRational){1, prof[h264_profile].Fps};
    av_context->framerate = (AVRational){prof[h264_profile].Fps, 1};
    av_context->gop_size = prof[h264_profile].GopSize;
    if(prof[h264_profile].MaxBFrames != -1)
        av_context->max_b_frames = prof[h264_profile].MaxBFrames;
    if(prof[h264_profile].RefFrames != -1)
        av_context->refs = prof[h264_profile].RefFrames;

    if(hwAcc) {
        av_context->pix_fmt = AV_PIX_FMT_VAAPI;
        av_context->get_format = get_vaapi_format;

        ret = av_hwdevice_ctx_create(&hw_device, AV_HWDEVICE_TYPE_VAAPI, prof[h264_profile].VAAPIDev, NULL, 0);
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

        av_opt_set(av_context->priv_data, "rc_mode", prof[h264_profile].hRcMode, 0);
        av_opt_set(av_context->priv_data, "quality", prof[h264_profile].hQuality, 0);
        av_opt_set(av_context->priv_data, "coder", prof[h264_profile].hCoder, 0);
        av_opt_set(av_context->priv_data, "profile", prof[h264_profile].hProfile, 0);
        av_opt_set(av_context->priv_data, "async_depth", prof[h264_profile].hAsyncDepth, 0);
        av_opt_set(av_context->priv_data, "level", prof[h264_profile].hLevelIdc, 0);
//        av_opt_set(av_context->priv_data, "idr_interval", "125", 0);
//        av_opt_set(av_context->priv_data, "b_depth", "2", 0);

        if(prof[h264_profile].GlobalQ != 0)
            av_context->global_quality = prof[h264_profile].GlobalQ;
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

    if(rfbFB.width != iw || rfbFB.height != ih || h264_profile != cl->zlibCompressLevel) {
#ifdef DEBUG
        rfbLog("H264 encoding profile change/resize to: %dx%d@%d\n", rfbFB.width, rfbFB.height, rfbFB.depth);
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
        av_frame_free(&frame);
        if(hwAcc) {
            av_frame_free(&hwFrame);
            av_buffer_unref(&hw_device);
        }
        avcodec_free_context(&av_context);
        av_free(av_context);
        av_context = NULL;
    }
}
