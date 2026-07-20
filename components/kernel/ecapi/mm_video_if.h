#ifndef MM_VIDEO_IF_H
#define MM_VIDEO_IF_H


typedef enum video_color_fmt
{
	VIDEO_COLOR_FMT_YUV420P = 0,
	VIDEO_COLOR_FMT_YUYV,
	VIDEO_COLOR_FMT_RGB565,
	VIDEO_COLOR_FMT_RGBA32,
	VIDEO_COLOR_FMT_MAX
}VIDEO_COLOR_FMT;



typedef struct video_image_buf
{
	VIDEO_COLOR_FMT	eFmt;

	unsigned short	uWidth;
	unsigned short	uHeight;

	void			*pData[3];
}VIDEO_IMAGE_BUF;


typedef struct video_region
{
	unsigned short	uX;
	unsigned short	uY;
	unsigned short	uWidth;
	unsigned short	uHeight;
}VIDEO_REGION;


typedef struct video_effect
{
	unsigned short	uMirror;
	unsigned short	uRotate;
}VIDEO_EFFECT;



/*
*brief      set memory buffer for video module allocation
*
*param_in   pBuf, buffer address
*           uLen, buffer size
*           uDataLen, jpeg data length
*
*return     0, success
*           <0, error
*/
int Video_MemSetBuf(unsigned char *pBuf,unsigned int uLen);


/*
*brief      scale image, rgb565 format
*
*param_in   pInBuf, input image buffer
*           uInWidth, input image width
*           uInHeight, input image height
*param_out pOutBuf, output image buffer
*           uOutWidth, output image width
*           uOutHeight, output image height
*
*return     none
*/
void Video_ScaleImageRgb565(unsigned short *pInBuf,unsigned int uInWidth,unsigned int uInHeight,unsigned short *pOutBuf,unsigned int uOutWidth,unsigned int uOutHeight);


/*
*brief      scale image, yuyv format
*
*param_in   pInBuf, input image buffer
*           uInWidth, input image width
*           uInHeight, input image height
*param_out pOutBuf, output image buffer
*           uOutWidth, output image width
*           uOutHeight, output image height
*
*return     none
*/
void Video_ScaleImageYuyv(unsigned char *pInBuf,unsigned int uInWidth,unsigned int uInHeight,unsigned char *pOutBuf,unsigned int uOutWidth,unsigned int uOutHeight);


/*
*brief      scale image, support only rgb565 format until now
*
*param_in   pInBuf, input image buffer
*           pInRegion, scale region of input image, NULL if scale whole image
*           pEffect, mirror and rorate, NULL if doesn't need effect
*param_out pOutBuf, output image buffer
*
*return     none
*/
void Video_ScaleImage(VIDEO_IMAGE_BUF *pInBuf,VIDEO_REGION *pInRegion,VIDEO_IMAGE_BUF *pOutBuf,VIDEO_EFFECT *pEffect);


/*
*brief      transfrom rgb565 image to yuyv image
*
*param_in   pRgb, rgb image buffer
*           uWidth, image width
*           uHeight, image height
*param_out pYuyv, yuyv image buffer
*
*return     none
*/
void Video_TransRgb565ToYuyv(unsigned short *pRgb,unsigned char *pYuyv,unsigned int uWidth,unsigned int uHeight);


/*
*brief      transfrom yuyv image to rgb565 image
*
*param_in   pYuyv, yuyv image buffer
*           uWidth, image width
*           uHeight, image height
*param_out pRgb, rgb image buffer
*
*return     none
*/
void Video_TransYuyvToRgb565(unsigned char *pYuyv,unsigned short *pRgb,unsigned int uWidth,unsigned int uHeight);





typedef struct png_info
{
	unsigned int    uWidth;
	unsigned int    uHeight;
}PNG_INFO;


/*
*brief      create png decoder handle
*
*param_in   none
*
*return     non-NULL, png handle
*           NULL, error
*/
void *PngD_Create();


/*
*brief      destroy png decoder handle
*
*param_in   hPng, png handle
*
*return     none
*/
void PngD_Destroy(void *hPng);

/*
*brief      set png data, decode png head
*
*param_in   hPng, png handle
*           pData, png data pointer
*           uDataLen, png data length
param_out   pInfo, png image size
*
*return     0, success
*           <0, error
*/
int PngD_DecodeInfo(void *hPng,unsigned char *pData,unsigned int uDataLen,PNG_INFO *pInfo);

/*
*brief      decode png data
*
*param_in   hPng, png handle
*           pIbuf, png image buffer
*
*return     0, success
*           <0, error
*/
int PngD_DecodeImage(void *hPng,VIDEO_IMAGE_BUF *pIbuf);






typedef struct gif_info
{
	unsigned int    uWidth;
	unsigned int    uHeight;
}GIF_INFO;



/*
*brief      create gif decoder handle
*
*param_in   none
*
*return     non-NULL, gif handle
*           NULL, error
*/
void *GifD_Create();


/*
*brief      destroy gif decoder handle
*
*param_in   hGif, gif handle
*
*return     none
*/
void GifD_Destroy(void *hGif);


/*
*brief      set gif data, decode gif head
*
*param_in   hGif, gif handle
*           pData, gif data pointer
*           uDataLen, gif data length
*param_out   pInfo, gif image size
*
*return     0, success
*           <0, error
*/
int GifD_DecodeInfo(void *hGif,unsigned char *pData,unsigned int uDataLen,GIF_INFO *pInfo);


/*
*brief      set gif canvas used in painting, must be set before decode image
*
*param_in   hGif, gif handle
*           pIbuf, gif canvas
*
*return     0, success
*           <0, error
*/
int GifD_SetCanvas(void *hGif,VIDEO_IMAGE_BUF *pIbuf);


/*
*brief      decode gif data
*
*param_in   hGif, gif handle
*           pIbuf, hGif image buffer
*param_out   pDuration, frame duration
*           pEos, animation stop
*
*return     0, success
*           <0, error
*/
int GifD_DecodeImage(void *hGif,VIDEO_IMAGE_BUF *pIbuf,unsigned int *pDuration,unsigned int *pEos);



#endif


