#ifndef APP_DISPLAY_PORT_LVGL_GIF_MM_H
#define APP_DISPLAY_PORT_LVGL_GIF_MM_H

typedef enum {
    VIDEO_COLOR_FMT_YUV420P = 0,
    VIDEO_COLOR_FMT_YUYV,
    VIDEO_COLOR_FMT_RGB565,
    VIDEO_COLOR_FMT_RGBA32,
    VIDEO_COLOR_FMT_MAX
} VIDEO_COLOR_FMT;

typedef struct {
    VIDEO_COLOR_FMT eFmt;
    unsigned short uWidth;
    unsigned short uHeight;
    void *pData[3];
} VIDEO_IMAGE_BUF;

typedef struct {
    unsigned int uWidth;
    unsigned int uHeight;
} GIF_INFO;

void *GifD_Create(void);
void GifD_Destroy(void *hGif);
int GifD_DecodeInfo(void *hGif, unsigned char *pData, unsigned int uDataLen, GIF_INFO *pInfo);
int GifD_SetCanvas(void *hGif, VIDEO_IMAGE_BUF *pIbuf);
int GifD_DecodeImage(void *hGif, VIDEO_IMAGE_BUF *pIbuf, unsigned int *pDuration, unsigned int *pEos);

#endif /* APP_DISPLAY_PORT_LVGL_GIF_MM_H */
