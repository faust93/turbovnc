#ifndef LIBVNCSERVER_COMMON_H
#define LIBVNCSERVER_COMMON_H

#include "rfb.h"

Bool sendFramebufferUpdateMsg(rfbClientPtr cl, int x, int y, int w, int h, u_char *data, size_t size);
Bool sendFramebufferUpdateMsg2(rfbClientPtr cl, int x, int y, int w, int h, u_char *data, size_t size, int forceFlush);
Bool sendOrQueueData(rfbClientPtr cl, u_char* data, int size, int forceFlush);
void fillInputBuffer(char *buffer, int i, int frame_width, int frame_height);
void rgba2Yuv(uint8_t *destination, uint8_t *rgb, size_t width, size_t height);
uint64_t getTimeNowMs();

#endif //LIBVNCSERVER_COMMON_H
