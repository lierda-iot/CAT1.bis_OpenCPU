#ifndef MM_JPEG_IF_H
#define MM_JPEG_IF_H


typedef enum jpeg_color_fmt
{
    JPEG_COLOR_FMT_Y = 0,
    JPEG_COLOR_FMT_YUV420P,
    JPEG_COLOR_FMT_YUYV,
    JPEG_COLOR_FMT_YUV444,
    JPEG_COLOR_FMT_RGB565,
    JPEG_COLOR_FMT_MAX
}JPEG_COLOR_FMT;


typedef struct jpeg_info
{
    unsigned short	uWidth;
    unsigned short	uHeight;
    unsigned short	uEdgedWidth;
    unsigned short	uEdgedHeight;	
}JPEG_INFO;


typedef struct jpeg_image_buf
{
    JPEG_COLOR_FMT  eFmt;
    
    unsigned int    uWidth;
    unsigned int    uHeight;

    void           *pData[3];
}JPEG_IMAGE_BUF;


typedef struct jpeg_enc_param
{
    JPEG_COLOR_FMT  eFmt;

    unsigned int    uWidth;
    unsigned int    uHeight;
    unsigned int    uQuality;//1~100

}JPEG_ENC_PARAM;



/*
*brief      create jpeg decoder handle
*
*param_in   none
*
*return     non-NULL, jpeg handle
*           NULL, error
*/
void *JpegD_Create();


/*
*brief      destroy jpeg decoder handle
*
*param_in   hJpeg, jpeg handle
*
*return     none
*/
void JpegD_Destroy(void *hJpeg);


/*
*brief      set jpeg data, decode jpeg head
*
*param_in   hJpeg, jpeg handle
*           pData, jpeg data pointer
*           uDataLen, jpeg data length
param_out   pInfo, jpeg image size
*
*return     0, success
*           <0, error
*/
int JpegD_DecodeInfo(void *hJpeg,unsigned char *pData,unsigned int uDataLen,JPEG_INFO *pInfo);


/*
*brief      decode jpeg data
*
*param_in   hJpeg, jpeg handle
*           pIbuf, jpeg image buffer
*
*return     0, success
*           <0, error
*/
int JpegD_DecodeImage(void *hJpeg,JPEG_IMAGE_BUF *pIbuf);


/*
*brief      decode jpeg slice
*
*param_in   hJpeg, jpeg handle
*           pIbuf, jpeg image buffer, widthx16
*           pPosY, current image slice pos
*
*return     0, success
*           <0, error
*/
int JpegD_DecodeLine(void *hJpeg,JPEG_IMAGE_BUF *pIbuf,unsigned int *pPosY);



/*
*brief      create jpeg encoder handle
*
*param_in   none
*
*return     non-NULL, jpeg handle
*           NULL, error
*/
void *JpegE_Create();


/*
*brief      destroy jpeg encoder handle
*
*param_in   hJpeg, jpeg handle
*
*return     none
*/
void JpegE_Destroy(void *hJpeg);


/*
*brief      optional, set encoder parameter
*
*param_in   hJpeg, jpeg handle
*           pParam, encoder parameter
param_out   pSuggBufLen, suggested data buffer size
*
*return     0, success
*           <0, error
*/
int JpegE_SetParam(void *hJpeg,JPEG_ENC_PARAM *pParam,unsigned int *pSuggBufLen);


/*
*brief      encode jpeg image
*
*param_in   hJpeg, jpeg handle
*           pIbuf, yuv image data
*           pBuf, data buffer pointer
*           pBufLen, data buffer size
param_out   pBufLen, actual data len
*
*return     0, success
*           <0, error
*/
int JpegE_Encode(void *hJpeg,JPEG_IMAGE_BUF *pIbuf,unsigned char *pBuf,unsigned int *pBufLen);


/*
*brief      set log level
*
*param_in   uLevel, log level
*
*return     none
*/
void Jpeg_SetLogLevel(unsigned int uLevel);



#endif

