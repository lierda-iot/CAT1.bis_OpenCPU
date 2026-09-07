#include "baji_photo_bind_ui.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BAJI_PHOTO_BIND_SCREEN_W     360
#define BAJI_PHOTO_BIND_SCREEN_H     360
#define BAJI_PHOTO_BIND_PANEL_W      284
#define BAJI_PHOTO_BIND_PANEL_MIN_H  292
#define BAJI_PHOTO_BIND_QR_SIZE      180
#define BAJI_PHOTO_BIND_NOTICE_INFO_MS    2400u
#define BAJI_PHOTO_BIND_NOTICE_SUCCESS_MS 2600u
#define BAJI_PHOTO_BIND_NOTICE_FAIL_MS    3200u

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *overlay;
    lv_obj_t *panel;
    lv_obj_t *title_label;
    lv_obj_t *body_label;
    lv_obj_t *countdown_label;
#if LV_USE_QRCODE
    lv_obj_t *qr;
#endif
    baji_photo_bind_ui_ops_t ops;
    baji_photo_bind_status_t bind_status;
    baji_photo_bind_token_info_t bind_token;
    baji_photo_bind_notice_t notice;
    uint32_t bind_token_tick;
    uint32_t countdown_start_tick;
    uint32_t countdown_total_ms;
    uint32_t notice_tick;
    uint32_t notice_duration_ms;
    int fatal_error;
    bool visible;
    bool hint_active;
    bool refresh_pending;
    char hint[48];
} baji_photo_bind_ui_ctx_t;

static baji_photo_bind_ui_ctx_t s_baji_photo_bind_ui;

static baji_photo_bind_ui_ctx_t *baji_photo_bind_ui_default(void)
{
    return &s_baji_photo_bind_ui;
}

static bool baji_photo_bind_ui_obj_valid(lv_obj_t *obj)
{
    return (obj != NULL) && lv_obj_is_valid(obj);
}

static bool baji_photo_bind_ui_is_visible_ctx(const baji_photo_bind_ui_ctx_t *ctx)
{
    return (ctx != NULL) &&
           baji_photo_bind_ui_obj_valid(ctx->overlay) &&
           !lv_obj_has_flag(ctx->overlay, LV_OBJ_FLAG_HIDDEN) &&
           ctx->visible;
}

static void baji_photo_bind_ui_refresh_controls(baji_photo_bind_ui_ctx_t *ctx)
{
    if ((ctx != NULL) && (ctx->ops.refresh_buttons != NULL)) {
        ctx->ops.refresh_buttons();
    }
    if ((ctx != NULL) && (ctx->ops.refresh_play_timer != NULL)) {
        ctx->ops.refresh_play_timer();
    }
}

static void baji_photo_bind_ui_notice_clear(baji_photo_bind_ui_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    memset(&ctx->notice, 0, sizeof(ctx->notice));
    ctx->notice_tick = 0u;
    ctx->notice_duration_ms = 0u;
}

static uint32_t baji_photo_bind_ui_notice_duration_ms(baji_photo_bind_notice_level_t level)
{
    switch (level) {
    case BAJI_PHOTO_BIND_NOTICE_SUCCESS:
        return BAJI_PHOTO_BIND_NOTICE_SUCCESS_MS;
    case BAJI_PHOTO_BIND_NOTICE_FAIL:
        return BAJI_PHOTO_BIND_NOTICE_FAIL_MS;
    default:
        return BAJI_PHOTO_BIND_NOTICE_INFO_MS;
    }
}

static bool baji_photo_bind_ui_notice_is_active(baji_photo_bind_ui_ctx_t *ctx)
{
    if ((ctx == NULL) ||
        (ctx->notice.message[0] == '\0') ||
        (ctx->notice_duration_ms == 0u) ||
        (ctx->notice_tick == 0u)) {
        return false;
    }

    if (lv_tick_elaps(ctx->notice_tick) >= ctx->notice_duration_ms) {
        baji_photo_bind_ui_notice_clear(ctx);
        ctx->refresh_pending = true;
        return false;
    }
    return true;
}

static void baji_photo_bind_ui_notice_set(baji_photo_bind_ui_ctx_t *ctx,
                                          const baji_photo_bind_notice_t *notice)
{
    if (ctx == NULL) {
        return;
    }

    baji_photo_bind_ui_notice_clear(ctx);
    if ((notice == NULL) || (notice->message[0] == '\0')) {
        return;
    }

    ctx->notice = *notice;
    ctx->notice_tick = lv_tick_get();
    ctx->notice_duration_ms =
        baji_photo_bind_ui_notice_duration_ms(notice->level);
}

static void baji_photo_bind_ui_hint_clear(baji_photo_bind_ui_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    ctx->hint_active = false;
    ctx->hint[0] = '\0';
}

static void baji_photo_bind_ui_hint_set(baji_photo_bind_ui_ctx_t *ctx, const char *text)
{
    if (ctx == NULL) {
        return;
    }

    if ((text == NULL) || (text[0] == '\0')) {
        baji_photo_bind_ui_hint_clear(ctx);
        return;
    }

    (void)snprintf(ctx->hint,
                   sizeof(ctx->hint),
                   "%s",
                   text);
    ctx->hint_active = true;
}

static void baji_photo_bind_ui_token_reset(baji_photo_bind_ui_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    memset(&ctx->bind_token, 0, sizeof(ctx->bind_token));
    ctx->bind_token_tick = 0u;
}

static uint32_t baji_photo_bind_ui_token_remaining_seconds_get(const baji_photo_bind_ui_ctx_t *ctx)
{
    uint64_t total_ms;
    uint32_t elapsed_ms;

    if ((ctx == NULL) ||
        (ctx->bind_token.bind_url[0] == '\0') ||
        (ctx->bind_token.expire_seconds == 0u)) {
        return 0u;
    }

    total_ms = (uint64_t)ctx->bind_token.expire_seconds * 1000u;
    if (ctx->bind_token_tick == 0u) {
        return ctx->bind_token.expire_seconds;
    }

    elapsed_ms = lv_tick_elaps(ctx->bind_token_tick);
    if ((uint64_t)elapsed_ms >= total_ms) {
        return 0u;
    }

    return (uint32_t)((total_ms - (uint64_t)elapsed_ms + 999u) / 1000u);
}

static void baji_photo_bind_ui_hint_sync(baji_photo_bind_ui_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    if (ctx->fatal_error != 0) {
        baji_photo_bind_ui_hint_set(ctx, "device id invalid");
        return;
    }

    if (baji_photo_bind_ui_notice_is_active(ctx)) {
        baji_photo_bind_ui_hint_set(ctx, ctx->notice.message);
        return;
    }

    if ((ctx->bind_status == BAJI_PHOTO_BIND_UNBOUND) &&
        (ctx->bind_token.bind_url[0] != '\0') &&
        (baji_photo_bind_ui_token_remaining_seconds_get(ctx) != 0u)) {
        baji_photo_bind_ui_hint_set(ctx, "bind token ready");
        return;
    }

    switch (ctx->bind_status) {
    case BAJI_PHOTO_BIND_UNBOUND:
        baji_photo_bind_ui_hint_set(ctx, "device unbound");
        break;
    case BAJI_PHOTO_BIND_DISABLED:
        baji_photo_bind_ui_hint_set(ctx, "device disabled");
        break;
    default:
        baji_photo_bind_ui_hint_clear(ctx);
        break;
    }
}

static void baji_photo_bind_ui_reset_view_refs(baji_photo_bind_ui_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    ctx->screen = NULL;
    ctx->overlay = NULL;
    ctx->panel = NULL;
    ctx->title_label = NULL;
    ctx->body_label = NULL;
    ctx->countdown_label = NULL;
#if LV_USE_QRCODE
    ctx->qr = NULL;
#endif
    ctx->countdown_start_tick = 0u;
    ctx->countdown_total_ms = 0u;
    ctx->visible = false;
}

bool baji_photo_bind_ui_is_visible(void)
{
    return baji_photo_bind_ui_is_visible_ctx(baji_photo_bind_ui_default());
}

static void baji_photo_bind_ui_hide(baji_photo_bind_ui_ctx_t *ctx)
{
    bool was_visible;

    if (ctx == NULL) {
        return;
    }

    was_visible = baji_photo_bind_ui_is_visible_ctx(ctx) || ctx->visible;

    if (baji_photo_bind_ui_obj_valid(ctx->overlay)) {
        lv_obj_add_flag(ctx->overlay, LV_OBJ_FLAG_HIDDEN);
    }

    ctx->visible = false;
    ctx->countdown_start_tick = 0u;
    ctx->countdown_total_ms = 0u;

    if (was_visible) {
        baji_photo_bind_ui_refresh_controls(ctx);
    }
}

static int baji_photo_bind_ui_ensure(baji_photo_bind_ui_ctx_t *ctx)
{
    lv_obj_t *panel;

    if ((ctx == NULL) || !baji_photo_bind_ui_obj_valid(ctx->screen)) {
        return -1;
    }

    if (baji_photo_bind_ui_obj_valid(ctx->overlay)) {
        return 0;
    }

    ctx->overlay = lv_obj_create(ctx->screen);
    if (ctx->overlay == NULL) {
        return -1;
    }

    lv_obj_remove_style_all(ctx->overlay);
    lv_obj_set_size(ctx->overlay, BAJI_PHOTO_BIND_SCREEN_W, BAJI_PHOTO_BIND_SCREEN_H);
    lv_obj_center(ctx->overlay);
    lv_obj_set_style_bg_color(ctx->overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ctx->overlay, LV_OPA_80, 0);
    lv_obj_add_flag(ctx->overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ctx->overlay, LV_OBJ_FLAG_SCROLLABLE);

    panel = lv_obj_create(ctx->overlay);
    if (panel == NULL) {
        baji_photo_bind_ui_hide(ctx);
        baji_photo_bind_ui_reset_view_refs(ctx);
        return -1;
    }

    ctx->panel = panel;
    lv_obj_set_size(panel, BAJI_PHOTO_BIND_PANEL_W, BAJI_PHOTO_BIND_PANEL_MIN_H);
    lv_obj_center(panel);
    lv_obj_set_style_radius(panel, 20, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_bg_color(panel, lv_color_make(18, 24, 32), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(panel, 18, 0);
    lv_obj_set_style_pad_right(panel, 18, 0);
    lv_obj_set_style_pad_top(panel, 20, 0);
    lv_obj_set_style_pad_bottom(panel, 18, 0);

    ctx->title_label = lv_label_create(panel);
    ctx->body_label = lv_label_create(panel);
    ctx->countdown_label = lv_label_create(panel);
    if (!baji_photo_bind_ui_obj_valid(ctx->title_label) ||
        !baji_photo_bind_ui_obj_valid(ctx->body_label) ||
        !baji_photo_bind_ui_obj_valid(ctx->countdown_label)) {
        baji_photo_bind_ui_hide(ctx);
        baji_photo_bind_ui_reset_view_refs(ctx);
        return -1;
    }

    lv_obj_set_width(ctx->title_label, BAJI_PHOTO_BIND_PANEL_W - 36);
    lv_obj_set_style_text_align(ctx->title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(ctx->title_label, lv_color_white(), 0);
    lv_obj_align(ctx->title_label, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_set_width(ctx->body_label, BAJI_PHOTO_BIND_PANEL_W - 36);
    lv_label_set_long_mode(ctx->body_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(ctx->body_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(ctx->body_label, lv_color_make(214, 221, 232), 0);
    lv_obj_align_to(ctx->body_label,
                    ctx->title_label,
                    LV_ALIGN_OUT_BOTTOM_MID,
                    0,
                    12);

    lv_obj_set_width(ctx->countdown_label, BAJI_PHOTO_BIND_PANEL_W - 36);
    lv_obj_set_style_text_align(ctx->countdown_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(ctx->countdown_label, lv_color_make(143, 156, 176), 0);
    lv_obj_align(ctx->countdown_label, LV_ALIGN_BOTTOM_MID, 0, -4);

#if LV_USE_QRCODE
    ctx->qr = lv_qrcode_create(panel,
                               BAJI_PHOTO_BIND_QR_SIZE,
                               lv_color_black(),
                               lv_color_white());
    if (baji_photo_bind_ui_obj_valid(ctx->qr)) {
        lv_obj_align(ctx->qr, LV_ALIGN_CENTER, 0, 18);
        lv_obj_set_style_border_color(ctx->qr, lv_color_white(), 0);
        lv_obj_set_style_border_width(ctx->qr, 6, 0);
    }
#endif

    return 0;
}

static void baji_photo_bind_ui_set_countdown(baji_photo_bind_ui_ctx_t *ctx, uint32_t expire_seconds)
{
    if (ctx == NULL) {
        return;
    }

    ctx->countdown_start_tick = lv_tick_get();
    ctx->countdown_total_ms = expire_seconds * 1000u;
}

static void baji_photo_bind_ui_update_countdown(baji_photo_bind_ui_ctx_t *ctx)
{
    uint32_t elapsed_ms;
    uint32_t remain_ms;
    uint32_t remain_s;
    char text[32];

    if ((ctx == NULL) ||
        !baji_photo_bind_ui_obj_valid(ctx->countdown_label) ||
        !baji_photo_bind_ui_is_visible_ctx(ctx)) {
        return;
    }

    if (ctx->countdown_total_ms == 0u) {
        lv_label_set_text(ctx->countdown_label, "");
        return;
    }

    elapsed_ms = lv_tick_elaps(ctx->countdown_start_tick);
    if (elapsed_ms >= ctx->countdown_total_ms) {
        lv_label_set_text(ctx->countdown_label, "refreshing token");
        return;
    }

    remain_ms = ctx->countdown_total_ms - elapsed_ms;
    remain_s = (remain_ms + 999u) / 1000u;
    (void)snprintf(text, sizeof(text), "expires in %lus", (unsigned long)remain_s);
    lv_label_set_text(ctx->countdown_label, text);
}

static void baji_photo_bind_ui_apply_theme(baji_photo_bind_ui_ctx_t *ctx,
                                           bool emphasize,
                                           baji_photo_bind_notice_level_t level)
{
    lv_color_t panel_bg = lv_color_make(18, 24, 32);
    lv_color_t title_color = lv_color_white();
    lv_color_t body_color = lv_color_make(214, 221, 232);
    lv_color_t countdown_color = lv_color_make(143, 156, 176);

    if (ctx == NULL) {
        return;
    }

    if (emphasize) {
        switch (level) {
        case BAJI_PHOTO_BIND_NOTICE_SUCCESS:
            panel_bg = lv_color_make(20, 72, 44);
            body_color = lv_color_make(222, 247, 230);
            countdown_color = lv_color_make(188, 235, 201);
            break;
        case BAJI_PHOTO_BIND_NOTICE_FAIL:
            panel_bg = lv_color_make(80, 28, 36);
            body_color = lv_color_make(250, 225, 228);
            countdown_color = lv_color_make(242, 186, 193);
            break;
        default:
            panel_bg = lv_color_make(92, 66, 20);
            body_color = lv_color_make(248, 236, 208);
            countdown_color = lv_color_make(239, 207, 144);
            break;
        }
    }

    if (baji_photo_bind_ui_obj_valid(ctx->panel)) {
        lv_obj_set_style_bg_color(ctx->panel, panel_bg, 0);
    }
    if (baji_photo_bind_ui_obj_valid(ctx->title_label)) {
        lv_obj_set_style_text_color(ctx->title_label, title_color, 0);
    }
    if (baji_photo_bind_ui_obj_valid(ctx->body_label)) {
        lv_obj_set_style_text_color(ctx->body_label, body_color, 0);
    }
    if (baji_photo_bind_ui_obj_valid(ctx->countdown_label)) {
        lv_obj_set_style_text_color(ctx->countdown_label, countdown_color, 0);
    }
}

static void baji_photo_bind_ui_show(baji_photo_bind_ui_ctx_t *ctx,
                                    const char *title,
                                    const char *body,
                                    const char *bind_url,
                                    uint32_t expire_seconds,
                                    bool emphasize,
                                    baji_photo_bind_notice_level_t level)
{
    bool was_visible;
    const char *safe_title = (title != NULL) ? title : "Bind Device";
    const char *safe_body = (body != NULL) ? body : "";

    if (ctx == NULL) {
        return;
    }

    was_visible = baji_photo_bind_ui_is_visible_ctx(ctx) || ctx->visible;
    if (baji_photo_bind_ui_ensure(ctx) != 0) {
        return;
    }

    baji_photo_bind_ui_apply_theme(ctx, emphasize, level);
    lv_label_set_text(ctx->title_label, safe_title);
    lv_label_set_text(ctx->body_label, safe_body);

#if LV_USE_QRCODE
    if (baji_photo_bind_ui_obj_valid(ctx->qr) &&
        (bind_url != NULL) &&
        (bind_url[0] != '\0') &&
        (expire_seconds != 0u) &&
        (lv_qrcode_update(ctx->qr, bind_url, strlen(bind_url)) == LV_RES_OK)) {
        lv_obj_clear_flag(ctx->qr, LV_OBJ_FLAG_HIDDEN);
        baji_photo_bind_ui_set_countdown(ctx, expire_seconds);
    } else if (baji_photo_bind_ui_obj_valid(ctx->qr)) {
        lv_obj_add_flag(ctx->qr, LV_OBJ_FLAG_HIDDEN);
        ctx->countdown_total_ms = 0u;
        ctx->countdown_start_tick = 0u;
    }
#else
    LV_UNUSED(bind_url);
    LV_UNUSED(expire_seconds);
#endif

    lv_obj_clear_flag(ctx->overlay, LV_OBJ_FLAG_HIDDEN);
    ctx->visible = true;

    if (ctx->ops.hide_controls != NULL) {
        ctx->ops.hide_controls();
    }
    if (ctx->ops.cancel_hide_timer != NULL) {
        ctx->ops.cancel_hide_timer();
    }
    if (!was_visible) {
        baji_photo_bind_ui_refresh_controls(ctx);
    }

    baji_photo_bind_ui_update_countdown(ctx);
}

static bool baji_photo_bind_ui_refresh(baji_photo_bind_ui_ctx_t *ctx, bool page_active)
{
    uint32_t expire_seconds;

    if (ctx == NULL) {
        return false;
    }

    if (!page_active) {
        baji_photo_bind_ui_hide(ctx);
        return true;
    }

    if (!baji_photo_bind_ui_obj_valid(ctx->screen)) {
        return false;
    }

    if (baji_photo_bind_ui_notice_is_active(ctx)) {
        const char *title = "Bind Device";
        const char *body = ctx->notice.message;

        if (ctx->notice.level == BAJI_PHOTO_BIND_NOTICE_SUCCESS) {
            title = "Bind Success";
        } else if (ctx->notice.level == BAJI_PHOTO_BIND_NOTICE_FAIL) {
            title = "Bind Failed";
        }
        baji_photo_bind_ui_show(ctx,
                                title,
                                body,
                                NULL,
                                0u,
                                true,
                                ctx->notice.level);
        return true;
    }

    if (ctx->fatal_error != 0) {
        baji_photo_bind_ui_show(ctx,
                                "Device Error",
                                "IMEI unavailable",
                                NULL,
                                0u,
                                true,
                                BAJI_PHOTO_BIND_NOTICE_FAIL);
        return true;
    }

    if (ctx->bind_status == BAJI_PHOTO_BIND_DISABLED) {
        baji_photo_bind_ui_show(ctx,
                                "Device Disabled",
                                "Device disabled by server",
                                NULL,
                                0u,
                                true,
                                BAJI_PHOTO_BIND_NOTICE_INFO);
        return true;
    }

    if (ctx->bind_status == BAJI_PHOTO_BIND_UNBOUND) {
        expire_seconds = baji_photo_bind_ui_token_remaining_seconds_get(ctx);
        if ((ctx->bind_token.bind_url[0] != '\0') && (expire_seconds != 0u)) {
            baji_photo_bind_ui_show(ctx,
                                    "Bind Device",
                                    "Scan QR to bind",
                                    ctx->bind_token.bind_url,
                                    expire_seconds,
                                    false,
                                    BAJI_PHOTO_BIND_NOTICE_INFO);
        } else if (ctx->bind_token.bind_url[0] != '\0') {
            baji_photo_bind_ui_show(ctx,
                                    "Bind Device",
                                    "Refreshing QR code",
                                    NULL,
                                    0u,
                                    false,
                                    BAJI_PHOTO_BIND_NOTICE_INFO);
        } else {
            baji_photo_bind_ui_show(ctx,
                                    "Bind Device",
                                    "Requesting QR code",
                                    NULL,
                                    0u,
                                    false,
                                    BAJI_PHOTO_BIND_NOTICE_INFO);
        }
        return true;
    }

    baji_photo_bind_ui_hide(ctx);
    return true;
}

void baji_photo_bind_ui_set_ops(const baji_photo_bind_ui_ops_t *ops)
{
    baji_photo_bind_ui_ctx_t *ctx = baji_photo_bind_ui_default();

    memset(&ctx->ops, 0, sizeof(ctx->ops));
    if (ops != NULL) {
        ctx->ops = *ops;
    }
}

void baji_photo_bind_ui_set_screen(lv_obj_t *screen)
{
    baji_photo_bind_ui_ctx_t *ctx = baji_photo_bind_ui_default();

    if (ctx->screen == screen) {
        ctx->refresh_pending = true;
        return;
    }

    baji_photo_bind_ui_reset_view_refs(ctx);
    ctx->screen = screen;
    ctx->refresh_pending = true;
}

void baji_photo_bind_ui_clear_screen(lv_obj_t *screen)
{
    baji_photo_bind_ui_ctx_t *ctx = baji_photo_bind_ui_default();

    if ((screen == NULL) || (ctx->screen != screen)) {
        return;
    }

    baji_photo_bind_ui_reset_view_refs(ctx);
    ctx->refresh_pending = true;
}

void baji_photo_bind_ui_set_bind_status(baji_photo_bind_status_t status)
{
    baji_photo_bind_ui_ctx_t *ctx = baji_photo_bind_ui_default();

    if (ctx->bind_status != status) {
        ctx->bind_status = status;
        baji_photo_bind_ui_token_reset(ctx);
        ctx->refresh_pending = true;
    } else if (status != BAJI_PHOTO_BIND_UNBOUND) {
        baji_photo_bind_ui_token_reset(ctx);
        ctx->refresh_pending = true;
    }

    if (status != BAJI_PHOTO_BIND_BOUND) {
        baji_photo_bind_ui_notice_clear(ctx);
    }

    baji_photo_bind_ui_hint_sync(ctx);
}

void baji_photo_bind_ui_set_bind_token(const baji_photo_bind_token_info_t *token)
{
    baji_photo_bind_ui_ctx_t *ctx = baji_photo_bind_ui_default();

    baji_photo_bind_ui_token_reset(ctx);

    if (token != NULL) {
        ctx->bind_token = *token;
        if (token->bind_url[0] != '\0') {
            ctx->bind_token_tick = lv_tick_get();
            baji_photo_bind_ui_notice_clear(ctx);
        }
    }

    ctx->refresh_pending = true;
    baji_photo_bind_ui_hint_sync(ctx);
}

void baji_photo_bind_ui_set_notice(const baji_photo_bind_notice_t *notice)
{
    baji_photo_bind_ui_ctx_t *ctx = baji_photo_bind_ui_default();

    baji_photo_bind_ui_notice_set(ctx, notice);
    ctx->refresh_pending = true;
    baji_photo_bind_ui_hint_sync(ctx);
}

void baji_photo_bind_ui_set_fatal_error(int fatal_error)
{
    baji_photo_bind_ui_ctx_t *ctx = baji_photo_bind_ui_default();

    if (ctx->fatal_error != fatal_error) {
        ctx->fatal_error = fatal_error;
        if (fatal_error != 0) {
            baji_photo_bind_ui_notice_clear(ctx);
        }
        ctx->refresh_pending = true;
    }

    baji_photo_bind_ui_hint_sync(ctx);
}

void baji_photo_bind_ui_tick(bool page_active)
{
    baji_photo_bind_ui_ctx_t *ctx = baji_photo_bind_ui_default();

    (void)baji_photo_bind_ui_notice_is_active(ctx);
    baji_photo_bind_ui_hint_sync(ctx);

    if (ctx->refresh_pending) {
        if (baji_photo_bind_ui_refresh(ctx, page_active)) {
            ctx->refresh_pending = false;
        }
    } else if (!page_active) {
        baji_photo_bind_ui_hide(ctx);
    }

    if (page_active && baji_photo_bind_ui_is_visible_ctx(ctx)) {
        baji_photo_bind_ui_update_countdown(ctx);
    }
}

const char *baji_photo_bind_ui_get_hint(void)
{
    const baji_photo_bind_ui_ctx_t *ctx = baji_photo_bind_ui_default();

    if (!ctx->hint_active || (ctx->hint[0] == '\0')) {
        return NULL;
    }

    return ctx->hint;
}
