#include <wels/codec_api.h>
#include <wels/codec_ver.h>
#include "common.h"

#define DEBUG 1

ISVCEncoder *encoder = NULL;
static u_char *yuv_buffer = NULL;
int iw, ih;

char h264conf_path[PATH_MAX];

typedef struct {
      int          iRCMode;
      int          iMinQp;
      int          iMaxQp;
      bool         bEnableAdaptiveQuant;
      float        fMaxFrameRate;
      bool         bEnableFrameSkip;
      int          iTemporalLayerNum;
      int          iNumRefFrame;
      int          iTargetBitrate;
      int          iMaxBitrate;
      bool         bEnableLongTermReference;
      unsigned int iLtrMarkPeriod;
      unsigned int uiIntraPeriod;
      int          iComplexityMode;

      bool         bIsLosslessLink;
      bool         bEnableDenoise;
      bool         bEnableSceneChangeDetect;
      bool         bEnableBackgroundDetection;
} H264Profile;

H264Profile prof[] = {{1, 16, 50, true, 60.0, true, 4, 4, 5000000, 15000000, true, 23, 23, 1, false, true, false, false }};

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
           v = atoi(val);
           if(!strcmp(param, "iTargetBitrate")) {
             prof[0].iTargetBitrate = v;
           }
           else if(!strcmp(param, "iMaxBitrate")) {
             prof[0].iMaxBitrate = v;
           }
           else if(!strcmp(param, "iComplexityMode")) {
             prof[0].iComplexityMode = v;
           }
           else if(!strcmp(param, "fMaxFrameRate")) {
             prof[0].fMaxFrameRate = atof(val);
           }
           else if(!strcmp(param, "iRCMode")) {
             prof[0].iRCMode = v;
           }
           else if(!strcmp(param, "iMinQp")) {
             prof[0].iMinQp = v;
           }
           else if(!strcmp(param, "iMaxQp")) {
             prof[0].iMaxQp = v;
           }
           else if(!strcmp(param, "bEnableAdaptiveQuant")) {
             prof[0].bEnableAdaptiveQuant = v;
           }
           else if(!strcmp(param, "bEnableFrameSkip")) {
             prof[0].bEnableFrameSkip = v;
           }
           else if(!strcmp(param, "iTemporalLayerNum")) {
             prof[0].iTemporalLayerNum = v;
           }
           else if(!strcmp(param, "iNumRefFrame")) {
             prof[0].iNumRefFrame = v;
           }
           else if(!strcmp(param, "uiIntraPeriod")) {
             prof[0].uiIntraPeriod = v;
           }
           else if(!strcmp(param, "bEnableLongTermReference")) {
             prof[0].bEnableLongTermReference = v;
           }
           else if(!strcmp(param, "iLtrMarkPeriod")) {
             prof[0].iLtrMarkPeriod = v;
           }
           else if(!strcmp(param, "bIsLosslessLink")) {
             prof[0].bIsLosslessLink = v;
           }
           else if(!strcmp(param, "bEnableDenoise")) {
             prof[0].bEnableDenoise = v;
           }
           else if(!strcmp(param, "bEnableSceneChangeDetect")) {
             prof[0].bEnableSceneChangeDetect = v;
           }
           else if(!strcmp(param, "bEnableBackgroundDetection")) {
             prof[0].bEnableBackgroundDetection = v;
           }

           rfbLog("%s = %d\n",param, v);
         }
      }
    }
    fclose(fn);
    return 0;
}

static Bool initOpenH264(rfbClientPtr cl) {
    Bool result = TRUE;

    if(rfbFB.depth != 32) {
        rfbLogPerror("Screen depth of 32 bits required for h264 encoding.\n");
        result = FALSE;
        goto error;
    }
#ifdef DEBUG
    rfbLog("Init H264 encoder instance: %dx%d@%d\n", rfbFB.width, rfbFB.height, rfbFB.depth);
#endif

    H264CfgRead();

    WelsCreateSVCEncoder(&encoder);

    //int idrIntv;
    //idrIntv = 10;
    //encoder->SetOption(ENCODER_OPTION_IDR_INTERVAL, &idrIntv);

    iw = rfbFB.width;
    ih = rfbFB.height;

    SEncParamExt param;
    memset (&param, 0, sizeof (SEncParamExt));
//    (*encoder)->GetDefaultParams(encoder, &param);
    param.iUsageType = CAMERA_VIDEO_REAL_TIME;
    param.iPicWidth = rfbFB.width;
    param.iPicHeight = rfbFB.height;

    param.iRCMode = prof[0].iRCMode;

    param.iMinQp = prof[0].iMinQp;
    param.iMaxQp = prof[0].iMaxQp;
    param.bEnableAdaptiveQuant = prof[0].bEnableAdaptiveQuant;

    param.fMaxFrameRate = prof[0].fMaxFrameRate;
    param.bEnableFrameSkip = prof[0].bEnableFrameSkip;
    param.iNumRefFrame = prof[0].iNumRefFrame;
    param.iTargetBitrate = prof[0].iTargetBitrate;
    param.iMaxBitrate = prof[0].iMaxBitrate;
    param.bEnableLongTermReference = prof[0].bEnableLongTermReference;
    param.iLtrMarkPeriod = prof[0].iLtrMarkPeriod;
    param.uiIntraPeriod = prof[0].uiIntraPeriod;
    param.iTemporalLayerNum = prof[0].iTemporalLayerNum;
    param.iComplexityMode = prof[0].iComplexityMode;
    param.bIsLosslessLink = prof[0].bIsLosslessLink;
    param.bEnableDenoise = prof[0].bEnableDenoise;
    param.bEnableSceneChangeDetect = prof[0].bEnableSceneChangeDetect;
    param.bEnableBackgroundDetection = prof[0].bEnableBackgroundDetection;

    param.iSpatialLayerNum = 2;

    param.iMultipleThreadIdc = 2;
    param.bUseLoadBalancing = true;

    param.sSpatialLayers[0].sSliceArgument.uiSliceMode = SM_SINGLE_SLICE;
    param.sSpatialLayers[1].sSliceArgument.uiSliceMode = SM_SINGLE_SLICE;

    param.sSpatialLayers[0].iVideoWidth = rfbFB.width;
    param.sSpatialLayers[0].iVideoHeight = rfbFB.height;
    param.sSpatialLayers[0].fFrameRate = param.fMaxFrameRate;
    param.sSpatialLayers[0].iSpatialBitrate = param.iTargetBitrate/2;
    param.sSpatialLayers[0].iMaxSpatialBitrate = param.iTargetBitrate/2;

    param.sSpatialLayers[1].iVideoWidth = rfbFB.width;
    param.sSpatialLayers[1].iVideoHeight = rfbFB.height;
    param.sSpatialLayers[1].fFrameRate = param.fMaxFrameRate;
    param.sSpatialLayers[1].iSpatialBitrate = param.iTargetBitrate/2;
    param.sSpatialLayers[1].iMaxSpatialBitrate = param.iTargetBitrate/2;

//    param.iTargetBitrate *= param.iSpatialLayerNum;
//    param.iTargetBitrate += param.iTargetBitrate/2;
    (*encoder)->InitializeExt(encoder, &param);

error:
    return result;
}

Bool rfbSendFrameEncodingOpenH264(rfbClientPtr cl) {
    Bool result = TRUE;
    int rv;
    int frameSize;
    int w = rfbFB.width;
    int h = rfbFB.height;

    if(encoder == NULL) {
        rfbLogPerror("Init H264 encoder");
        if(!initOpenH264(cl)) {
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

    if(yuv_buffer == NULL) {
        yuv_buffer = (u_char*)malloc(w*h + ((w*h)/2));
    }

    rgba2Yuv(yuv_buffer, (u_char*)rfbFB.pfbMemory, w, h);

    frameSize = w * h * 3 / 2;
    SFrameBSInfo info;
    memset (&info, 0, sizeof (SFrameBSInfo));
    SSourcePicture pic;
    memset (&pic, 0, sizeof (SSourcePicture));
    pic.iPicWidth = w;
    pic.iPicHeight = h;
//    pic.uiTimeStamp = getTimeNowMs();
    pic.iColorFormat = videoFormatI420;
    pic.iStride[0] = pic.iPicWidth;
    pic.iStride[1] = pic.iStride[2] = pic.iPicWidth >> 1;
    pic.pData[0] = yuv_buffer;
    pic.pData[1] = pic.pData[0] + w * h;
    pic.pData[2] = pic.pData[1] + (w * h >> 2);

    //(*encoder)->ForceIntraFrame(encoder, true);
    rv = (*encoder)->EncodeFrame(encoder, &pic, &info);

    if(rv == cmResultSuccess && info.eFrameType != videoFrameTypeSkip) {
        int iLayer;
        for (iLayer=0; iLayer < info.iLayerNum; iLayer++)
        {
            SLayerBSInfo* pLayerBsInfo = &info.sLayerInfo[iLayer];
            if (NULL != pLayerBsInfo) {
            int iLayerSize = 0;
            int iNalIdx = pLayerBsInfo->iNalCount - 1;
            do {
                iLayerSize += pLayerBsInfo->pNalLengthInByte[iNalIdx];
                --iNalIdx;
            } while (iNalIdx >= 0);

            unsigned char *outBuf = pLayerBsInfo->pBsBuf;
            sendFramebufferUpdateMsg(cl, 0, 0, w, h, outBuf, iLayerSize);
           }
        }
    }

error:
    return result;
}

void rfbH264Cleanup(rfbClientPtr cl) {
#ifdef DEBUG
    rfbLog("H264 cleanup\n");
#endif
    if (encoder) {
        (*encoder)->Uninitialize(encoder);
        WelsDestroySVCEncoder(encoder);
        encoder = NULL;
        free(yuv_buffer);
        yuv_buffer = NULL;
    }
}

