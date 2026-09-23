/**
 * @file demo_jpeg.c
 * @brief LIoT JPEG encode/decode conformance demo.
 *
 * The demo builds a synthetic test image, then runs one
 * encode -> get_info -> decode round trip per case. Positive cases must
 * succeed; negative cases must be rejected by the codec, and each prints the
 * reason. Every line is tagged [ok] or [UNEXPECTED] against that expectation.
 * The whole suite runs twice - at WIDTH x HEIGHT and WIDTH x HEIGHT_ALT - to
 * confirm a shorter height behaves the same as the square case.
 *
 * EC7xx JPEG codec capability (verified by runtime test + library symbols):
 *   Encode : YUYV(4:2:2), grayscale-Y, RGB565(auto-converted to YUYV) all OK;
 *            YUV420 NOT usable - JpegE_SetParam rejects JPEG_COLOR_FMT_YUV420P.
 *   Decode : output RGB565 OK (source bitstream may be 4:2:0 / 4:2:2 / 4:4:4);
 *            output grayscale-Y NOT usable - JpegD_DecodeImage refuses Y.
 *   Info   : liot_jpeg_get_info (read image size from header) OK.
 * Coverage: positive cases cover every usable format combination; negative
 *           cases assert YUV420 encode and Y decode stay rejected (so a future
 *           codec that starts accepting them shows up as [UNEXPECTED]).
 *           Not exercised: decoding a genuine 4:2:0 / 4:4:4 bitstream, because
 *           this demo can only encode 4:2:2 / grayscale sources itself.
 * Sibling video helpers (scale, RGB565<->YUYV, mirror/rotate, PNG, GIF decode)
 * live in the same codec library but are outside this liot_jpeg demo's scope.
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-01-01
 * @version 1.0
 */
#include <stdbool.h>
#include <stdint.h>

#include "liot_jpeg.h"
#include "liot_log.h"
#include "liot_os.h"

#define LIOT_JPEG_DEMO_WIDTH         (32U)
#define LIOT_JPEG_DEMO_HEIGHT        (32U)  /* max height, sizes buffers */
#define LIOT_JPEG_DEMO_HEIGHT_ALT    (16U)
#define LIOT_JPEG_DEMO_QUALITY       (80U)
#define LIOT_JPEG_DEMO_MAX_BPP       (2U)
#define LIOT_JPEG_DEMO_HEADER_BUDGET (1024U)
#define LIOT_JPEG_DEMO_IDLE_MS       (5000U)

#define LIOT_JPEG_DEMO_LUMA_BASE  (32U)
#define LIOT_JPEG_DEMO_LUMA_SPAN  (200U)
#define LIOT_JPEG_DEMO_CHROMA_MID (128U)

#define LIOT_JPEG_DEMO_RGB565_R_SHIFT (11U)
#define LIOT_JPEG_DEMO_RGB565_G_SHIFT (5U)
#define LIOT_JPEG_DEMO_RGB565_R_MAX   (0x1FU)
#define LIOT_JPEG_DEMO_RGB565_G_MAX   (0x3FU)
#define LIOT_JPEG_DEMO_RGB565_B_VALUE (0x0FU)

/** One round-trip test case: encode with @c enc_fmt, decode with @c dec_fmt. */
typedef struct
{
    const char *name;
    liot_jpeg_enc_fmt_e enc_fmt;
    liot_jpeg_dec_fmt_e dec_fmt;
    uint32_t enc_bpp;    /**< Bytes per pixel of the encoder input buffer. */
    bool expect_ok;      /**< true: must succeed; false: must be rejected. */
    const char *note;    /**< Why a negative case is rejected; NULL if none. */
} liot_jpeg_demo_case_t;

/** Reusable buffers shared by every case, sized for the worst format. */
typedef struct
{
    uint8_t *raw;
    uint8_t *jpeg;
    uint8_t *decoded;
    uint32_t raw_capacity;
    uint32_t jpeg_capacity;
    uint32_t decoded_capacity;
} liot_jpeg_demo_buf_t;

static void liot_jpeg_demo_fill_yuyv(uint8_t *data, uint32_t w, uint32_t h)
{
    for(uint32_t y = 0; y < h; y++)
    {
        for(uint32_t x = 0; x < w; x += 2U)
        {
            uint32_t offset = (y * w + x) * 2U;
            data[offset] = (uint8_t)(LIOT_JPEG_DEMO_LUMA_BASE +
                                     (x * LIOT_JPEG_DEMO_LUMA_SPAN) / w);
            data[offset + 1U] = LIOT_JPEG_DEMO_CHROMA_MID;
            data[offset + 2U] = (uint8_t)(LIOT_JPEG_DEMO_LUMA_BASE +
                                          (y * LIOT_JPEG_DEMO_LUMA_SPAN) / h);
            data[offset + 3U] = LIOT_JPEG_DEMO_CHROMA_MID;
        }
    }
}

static void liot_jpeg_demo_fill_gray(uint8_t *data, uint32_t w, uint32_t h)
{
    for(uint32_t y = 0; y < h; y++)
    {
        for(uint32_t x = 0; x < w; x++)
        {
            uint32_t offset = y * w + x;
            data[offset] = (uint8_t)(LIOT_JPEG_DEMO_LUMA_BASE +
                                     (x * LIOT_JPEG_DEMO_LUMA_SPAN) / w);
        }
    }
}

static void liot_jpeg_demo_fill_rgb565(uint8_t *data, uint32_t w, uint32_t h)
{
    uint16_t *pixels = (uint16_t *)data;
    for(uint32_t y = 0; y < h; y++)
    {
        for(uint32_t x = 0; x < w; x++)
        {
            uint32_t r = (x * LIOT_JPEG_DEMO_RGB565_R_MAX) / w;
            uint32_t g = (y * LIOT_JPEG_DEMO_RGB565_G_MAX) / h;
            pixels[y * w + x] =
                (uint16_t)((r << LIOT_JPEG_DEMO_RGB565_R_SHIFT) |
                           (g << LIOT_JPEG_DEMO_RGB565_G_SHIFT) |
                           LIOT_JPEG_DEMO_RGB565_B_VALUE);
        }
    }
}

/** Populate the raw buffer with a test pattern matching the encoder format. */
static void liot_jpeg_demo_fill(uint8_t *raw, liot_jpeg_enc_fmt_e fmt,
                                uint32_t w, uint32_t h)
{
    if(fmt == LIOT_JPEG_ENC_FMT_Y)
    {
        liot_jpeg_demo_fill_gray(raw, w, h);
    }
    else if(fmt == LIOT_JPEG_ENC_FMT_RGB565)
    {
        liot_jpeg_demo_fill_rgb565(raw, w, h);
    }
    else
    {
        liot_jpeg_demo_fill_yuyv(raw, w, h);
    }
}

/**
 * @brief Run one encode/get_info/decode round trip and print the results.
 * @param[in] tc   Test case describing the encode and decode formats.
 * @param[in] buf  Shared work buffers sized for the largest format.
 * @param[in] w    Image width in pixels for this run.
 * @param[in] h    Image height in pixels for this run.
 * @return true when the observed result matches @c tc->expect_ok.
 */
static bool liot_jpeg_demo_run_case(const liot_jpeg_demo_case_t *tc,
                                    const liot_jpeg_demo_buf_t *buf,
                                    uint32_t w, uint32_t h)
{
    const uint32_t raw_size = w * h * tc->enc_bpp;

    liot_jpeg_demo_fill(buf->raw, tc->enc_fmt, w, h);

    uint32_t jpeg_size = buf->jpeg_capacity;
    liot_errcode_jpeg_e enc_ret =
        liot_jpeg_encode(buf->raw, raw_size, w, h, LIOT_JPEG_DEMO_QUALITY,
                         tc->enc_fmt, buf->jpeg, &jpeg_size);

    liot_jpeg_info_t info = {0};
    liot_errcode_jpeg_e info_ret = enc_ret;
    uint32_t dec_size = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    liot_errcode_jpeg_e dec_ret = enc_ret;
    if(enc_ret == LIOT_JPEG_SUCCESS)
    {
        info_ret = liot_jpeg_get_info(buf->jpeg, jpeg_size, &info);
        dec_size = buf->decoded_capacity;
        dec_ret = liot_jpeg_decode(buf->jpeg, jpeg_size, buf->decoded,
                                   &dec_size, &width, &height, tc->dec_fmt);
    }

    bool as_expected =
        ((enc_ret == LIOT_JPEG_SUCCESS) && (info_ret == LIOT_JPEG_SUCCESS) &&
         (dec_ret == LIOT_JPEG_SUCCESS)) == tc->expect_ok;
    const char *verdict = as_expected ? "ok" : "UNEXPECTED";
    const char *note = (tc->note != NULL) ? tc->note : "";
    uint32_t out_bytes = (dec_ret == LIOT_JPEG_SUCCESS) ? dec_size : 0;

    if(enc_ret != LIOT_JPEG_SUCCESS)
    {
        liot_trace("%s: encode rejected ret=%d [%s] %s",
                   tc->name, enc_ret, verdict, note);
    }
    else
    {
        liot_trace("%s: jpeg=%u, info=%ux%u ret=%d, decode=%ux%u out=%u "
                   "ret=%d [%s] %s",
                   tc->name, jpeg_size, info.width, info.height, info_ret,
                   width, height, out_bytes, dec_ret, verdict, note);
    }
    return as_expected;
}

/*
 * Positive cases (expect_ok = true) cover every usable format path. Negative
 * cases (expect_ok = false) document the two paths the EC7xx codec rejects, so
 * they are asserted rather than silently omitted:
 *   - yuv420 encode: JpegE_SetParam refuses JPEG_COLOR_FMT_YUV420P.
 *   - Y decode: JpegD_DecodeImage refuses JPEG_COLOR_FMT_Y output.
 * If a future codec starts accepting either, its line prints [UNEXPECTED].
 */
static const liot_jpeg_demo_case_t s_liot_jpeg_demo_cases[] = {
    {"yuv422->rgb565", LIOT_JPEG_ENC_FMT_YUV422, LIOT_JPEG_DEC_FMT_RGB565, 2U,
     true, NULL},
    {"gray->rgb565", LIOT_JPEG_ENC_FMT_Y, LIOT_JPEG_DEC_FMT_RGB565, 1U, true,
     NULL},
    {"rgb565->rgb565", LIOT_JPEG_ENC_FMT_RGB565, LIOT_JPEG_DEC_FMT_RGB565, 2U,
     true, NULL},
    {"yuv420->rgb565", LIOT_JPEG_ENC_FMT_YUV420, LIOT_JPEG_DEC_FMT_RGB565, 2U,
     false, "encoder rejects YUV420P, only YUYV(4:2:2) accepted"},
    {"gray->y", LIOT_JPEG_ENC_FMT_Y, LIOT_JPEG_DEC_FMT_Y, 1U,
     false, "decoder rejects Y output, only RGB565 produced"},
};

/**
 * @brief Run every case at one resolution and print a per-suite summary.
 * @param[in] buf  Shared work buffers sized for the largest format.
 * @param[in] w    Image width in pixels for this suite run.
 * @param[in] h    Image height in pixels for this suite run.
 */
static void liot_jpeg_demo_run_suite(const liot_jpeg_demo_buf_t *buf,
                                     uint32_t w, uint32_t h)
{
    const uint32_t total = (uint32_t)(sizeof(s_liot_jpeg_demo_cases) /
                                      sizeof(s_liot_jpeg_demo_cases[0]));
    liot_trace("jpeg demo start, input=%ux%u, cases=%u", w, h, total);

    uint32_t as_expected = 0;
    for(uint32_t i = 0; i < total; i++)
    {
        if(liot_jpeg_demo_run_case(&s_liot_jpeg_demo_cases[i], buf, w, h))
        {
            as_expected++;
        }
    }
    liot_trace("jpeg demo done, input=%ux%u, as_expected=%u/%u", w, h,
               as_expected, total);
}

void liot_jpeg_demo_thread(void *argv)
{
    (void)argv;

    liot_rtos_task_sleep_ms(500);
    liot_trace("==== jpeg demo start ====");

    const uint32_t pixels = LIOT_JPEG_DEMO_WIDTH * LIOT_JPEG_DEMO_HEIGHT;
    liot_jpeg_demo_buf_t buf = {0};
    buf.raw_capacity = pixels * LIOT_JPEG_DEMO_MAX_BPP;
    buf.jpeg_capacity = buf.raw_capacity + LIOT_JPEG_DEMO_HEADER_BUDGET;
    buf.decoded_capacity = pixels * LIOT_JPEG_DEMO_MAX_BPP;

    buf.raw = (uint8_t *)liot_rtos_malloc(buf.raw_capacity);
    buf.jpeg = (uint8_t *)liot_rtos_malloc(buf.jpeg_capacity);
    buf.decoded = (uint8_t *)liot_rtos_malloc(buf.decoded_capacity);
    if(buf.raw == NULL || buf.jpeg == NULL || buf.decoded == NULL)
    {
        liot_trace("jpeg demo buffer allocation failed");
        liot_rtos_free(buf.raw);
        liot_rtos_free(buf.jpeg);
        liot_rtos_free(buf.decoded);
        return;
    }

    liot_jpeg_demo_run_suite(&buf, LIOT_JPEG_DEMO_WIDTH, LIOT_JPEG_DEMO_HEIGHT);
    liot_jpeg_demo_run_suite(&buf, LIOT_JPEG_DEMO_WIDTH,
                             LIOT_JPEG_DEMO_HEIGHT_ALT);

    liot_rtos_free(buf.raw);
    liot_rtos_free(buf.jpeg);
    liot_rtos_free(buf.decoded);

    for(;;)
    {
        liot_rtos_task_sleep_ms(LIOT_JPEG_DEMO_IDLE_MS);
    }
}
