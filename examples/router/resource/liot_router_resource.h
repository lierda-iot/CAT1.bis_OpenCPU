/* 自动生成，勿手改。 */
#ifndef __WWW_ASSET_H__
#define __WWW_ASSET_H__

typedef struct {
    const char *path;
    const char *mime;
    const unsigned char *data;
    unsigned int len;
    unsigned char gzipped;   /* 1=data 为 gzip，需带 Content-Encoding */
} Liot_WebAsset_t;

extern const Liot_WebAsset_t gWebAssets[];
extern const unsigned int gWebAssetCount;

#endif
