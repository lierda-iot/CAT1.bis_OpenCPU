#ifndef TIRTC_ASSET_MANIFEST_H
#define TIRTC_ASSET_MANIFEST_H
#include <stdint.h>
#define TIRTC_FONT_BYTES 631201U
#define TIRTC_FONT_CRC32 0x5ef3a3caU
extern const uint8_t tirtc_install_font[TIRTC_FONT_BYTES];
#define TIRTC_BACKGROUND_BYTES 153600U
#define TIRTC_BACKGROUND_CRC32 0x87dd3141U
extern const uint8_t tirtc_install_background[TIRTC_BACKGROUND_BYTES];
#define TIRTC_FONT_META_BYTES 70108U
#define TIRTC_FONT_META_CRC32 0x3d1c9e6fU
extern const uint8_t tirtc_install_font_meta[TIRTC_FONT_META_BYTES];
#define TIRTC_FONT_DIGITS_BYTES 4495U
#define TIRTC_FONT_DIGITS_CRC32 0x34edc6cdU
extern const uint8_t tirtc_install_font_digits[TIRTC_FONT_DIGITS_BYTES];
#define TIRTC_ICON_PHONE_BYTES 5292U
#define TIRTC_ICON_PHONE_CRC32 0xd442f208U
extern const uint8_t tirtc_install_icon_phone[TIRTC_ICON_PHONE_BYTES];
#define TIRTC_ICON_WECHAT_BYTES 4448U
#define TIRTC_ICON_WECHAT_CRC32 0x8c370f14U
extern const uint8_t tirtc_install_icon_wechat[TIRTC_ICON_WECHAT_BYTES];
#define TIRTC_FACE_DATA_BYTES 7744U
#define TIRTC_FACE_DATA_CRC32 0x843c5f6bU
extern const uint8_t tirtc_install_face_data[TIRTC_FACE_DATA_BYTES];
#define TIRTC_UI_ASSET_COUNT 7U
#define TIRTC_UI_ASSET_TOTAL_BYTES 876888U
/* X(macro prefix, installer symbol suffix, absolute resource path). */
#define TIRTC_UI_ASSET_LIST(X) \
    X(FONT, font, "/ui14-font.bin") \
    X(BACKGROUND, background, "/ui13-bg.bin") \
    X(FONT_META, font_meta, "/ui14-meta.bin") \
    X(FONT_DIGITS, font_digits, "/ui48-digits.bin") \
    X(ICON_PHONE, icon_phone, "/ui-phone.bin") \
    X(ICON_WECHAT, icon_wechat, "/ui-wechat.bin") \
    X(FACE_DATA, face_data, "/ui-face-v1.bin")
#endif
