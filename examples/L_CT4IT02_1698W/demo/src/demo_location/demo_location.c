#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "gui_guider.h"
#include "cJSON.h"
#include "liot_datacall.h"
#include "liot_gpio2.h"
#include "liot_http.h"
#include "liot_log.h"
#include "liot_os.h"
#include "liot_sockets.h"

#include "demo_location.h"
#include "demo_agnss_config.h"
#ifdef HWDEMO_GNSS_EN
#include "demo_gnss.h"
#endif

#define LOCATION_LOG_PREFIX       "[location]"
#define LOCATION_TASK_STACK       (8 * 1024)
#define LOCATION_HOLD_MS          1200U
#define LOCATION_MOVE_LIMIT       12
#define LOCATION_DESC_SIZE        160
#define LOCATION_RESPONSE_SIZE    2048
#define LOCATION_HTTP_TIMEOUT_MS  30000
#define LOCATION_REGISTER_RETRIES  5
#define LOCATION_API_URL          "https://ai.iwg.senthink.com/api/reverse-geocode"
#define LOCATION_KEY_GPIO         L_GPIO_20
#define LOCATION_KEY_MODEM_PIN    5
#define LOCATION_KEY_DEBOUNCE_MS  50
#define LOCATION_FIX_POLL_MS       1000U
#define LOCATION_EXIT_DELAY_MS      300U
#define AGNSS_RESPONSE_SIZE          8192
#define AGNSS_HTTP_TIMEOUT_MS        30000
#define AGNSS_GNSS_READY_TIMEOUT_MS  10000
#define AGNSS_SOCKET_HEADER_SIZE      2048
#define AGNSS_REQUEST_BODY \
    "[{\"rtAssistance\":{\"format\":\"rtcm\",\"msgs\":[\"GPS:1NAF\",\"BDS:2NAF\"]}}]"

typedef struct {
    int32_t latitude_e7;
    int32_t longitude_e7;
    uint16_t accuracy_m;
    bool valid;
} location_fix_t;

typedef struct {
    char response[LOCATION_RESPONSE_SIZE];
    int response_len;
    int http_status;
    int event_type;
    int event_result;
    volatile bool done;
    volatile bool closed;
} location_http_ctx_t;

typedef struct {
    uint8_t *response;
    int response_capacity;
    int response_len;
    const char *request;
    int request_len;
    int request_offset;
    int http_status;
    int event_type;
    int event_result;
    bool overflow;
    volatile bool done;
    volatile bool closed;
} agnss_http_ctx_t;

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *latitude_label;
    lv_obj_t *longitude_label;
    lv_obj_t *status_label;
    lv_obj_t *description_label;
    lv_obj_t *locate_button;
    lv_timer_t *result_timer;
    lv_timer_t *exit_timer;
    liot_task_t request_task;
    location_fix_t fix;
    char description[LOCATION_DESC_SIZE];
    int request_result;
    volatile bool result_pending;
    bool page_active;
    bool page_exiting;
    bool request_running;
    bool press_candidate;
    bool page_del;
    uint32_t generation;
    uint32_t fix_poll_tick;
    uint32_t press_tick;
    lv_point_t press_point;
} location_ctx_t;

static location_ctx_t g_location;
static uint32_t g_location_generation;
static bool g_location_network_ready;
static volatile bool g_location_network_result_pending;
static int g_location_network_result;
static volatile bool g_location_request_active;
#ifdef HWDEMO_LOCATION_HEADLESS_EN
static liot_sem_t g_location_key_sem;
static volatile uint32_t g_location_key_tick;
#endif
extern void lvgl_init(void);
extern const lv_font_t lv_font_location_cjk_16;

static bool location_source_get(location_fix_t *fix)
{
    if (fix == NULL) return false;
#ifdef HWDEMO_GNSS_EN
    if (!demo_gnss_get_location(&fix->latitude_e7, &fix->longitude_e7)) {
        fix->valid = false;
        return false;
    }
#else
    fix->latitude_e7 = 302741500;   /* 30.2741500 N */
    fix->longitude_e7 = 1201551000; /* 120.1551000 E */
#endif
    fix->accuracy_m = 15;
    fix->valid = true;
    return true;
}

static void location_format_coordinate(char *buffer, size_t size, int32_t value_e7,
                                       char positive, char negative)
{
    int64_t value = value_e7;
    char direction = positive;
    if (value < 0) {
        value = -value;
        direction = negative;
    }
    snprintf(buffer, size, "%ld.%07ld %c",
             (long)(value / 10000000),
             (long)(value % 10000000), direction);
}

static int location_http_write(liot_http_client_t *client, void *argument,
                               char *data, int size, unsigned char end)
{
    location_http_ctx_t *ctx = (location_http_ctx_t *)argument;
    int available;
    (void)client;
    (void)end;
    if (ctx == NULL || data == NULL || size <= 0) return 0;
    available = (int)sizeof(ctx->response) - 1 - ctx->response_len;
    if (size > available) size = available;
    if (size > 0) {
        memcpy(ctx->response + ctx->response_len, data, (size_t)size);
        ctx->response_len += size;
        ctx->response[ctx->response_len] = '\0';
    }
    return size;
}

static void location_http_event(liot_http_client_t *client, int event,
                                int event_code, void *argument)
{
    location_http_ctx_t *ctx = (location_http_ctx_t *)argument;
    if (ctx == NULL) return;
    ctx->event_type = event;
    liot_trace("%s HTTP callback type=%d code=0x%x", LOCATION_LOG_PREFIX,
               event, event_code);
    switch (event) {
    case LIOT_HTTPC_RESPONSE_STATUS:
        if (event_code == LIOT_HTTPC_SUCCESS) {
            liot_httpc_getinfo(client, LIOT_HTTPC_STATUS_CODE, &ctx->http_status);
        }
        break;
    case LIOT_HTTPC_RESPONSE_COMPLETE:
    case LIOT_HTTPC_RESPONSE_TIMEOUT:
        ctx->event_result = event_code;
        ctx->done = true;
        break;
    case LIOT_HTTPC_SESSION_OPEN:
        if (event_code != LIOT_HTTPC_SUCCESS) {
            ctx->event_result = event_code;
            ctx->done = true;
        }
        break;
    case LIOT_HTTPC_SESSION_CLOSE:
        ctx->closed = true;
        break;
    default:
        break;
    }
}

static int agnss_http_read(liot_http_client_t *client, void *argument,
                           char *data, int size)
{
    agnss_http_ctx_t *ctx = (agnss_http_ctx_t *)argument;
    int remaining;
    int copied;

    (void)client;
    if (ctx == NULL || data == NULL || size <= 0) return 0;
    remaining = ctx->request_len - ctx->request_offset;
    if (remaining <= 0) return 0;
    copied = remaining < size ? remaining : size;
    memcpy(data, ctx->request + ctx->request_offset, (size_t)copied);
    ctx->request_offset += copied;
    return copied;
}

static int agnss_http_write(liot_http_client_t *client, void *argument,
                            char *data, int size, unsigned char end)
{
    agnss_http_ctx_t *ctx = (agnss_http_ctx_t *)argument;
    int available;

    (void)client;
    (void)end;
    if (ctx == NULL || data == NULL || size <= 0) return 0;
    available = ctx->response_capacity - ctx->response_len;
    if (size > available) {
        ctx->overflow = true;
        size = available;
    }
    if (size > 0) {
        memcpy(ctx->response + ctx->response_len, data, (size_t)size);
        ctx->response_len += size;
    }
    return size;
}

static void agnss_http_event(liot_http_client_t *client, int event,
                             int event_code, void *argument)
{
    agnss_http_ctx_t *ctx = (agnss_http_ctx_t *)argument;

    if (ctx == NULL) return;
    ctx->event_type = event;
    switch (event) {
    case LIOT_HTTPC_RESPONSE_STATUS:
        if (event_code == LIOT_HTTPC_SUCCESS) {
            liot_httpc_getinfo(client, LIOT_HTTPC_STATUS_CODE, &ctx->http_status);
        }
        break;
    case LIOT_HTTPC_RESPONSE_COMPLETE:
    case LIOT_HTTPC_RESPONSE_TIMEOUT:
        ctx->event_result = event_code;
        ctx->done = true;
        break;
    case LIOT_HTTPC_SESSION_OPEN:
        if (event_code != LIOT_HTTPC_SUCCESS) {
            ctx->event_result = event_code;
            ctx->done = true;
        }
        break;
    case LIOT_HTTPC_SESSION_CLOSE:
        ctx->closed = true;
        break;
    default:
        break;
    }
}

static int location_agnss_download_httpc(void)
{
#ifdef HWDEMO_GNSS_EN
    agnss_http_ctx_t ctx;
    liot_http_client_t client = 0;
    liot_httpc_url_s parsed_url;
    char url[192];
    char headers[384];
    int waited = 0;
    int result = -1;
    int perform_result;
    int header_len;

    memset(&ctx, 0, sizeof(ctx));
    memset(&parsed_url, 0, sizeof(parsed_url));
    snprintf(url, sizeof(url), "%s", DEMO_AGNSS_URL);
    ctx.response = liot_rtos_malloc(AGNSS_RESPONSE_SIZE);
    if (ctx.response == NULL) return -1;
    ctx.response_capacity = AGNSS_RESPONSE_SIZE;
    ctx.request = AGNSS_REQUEST_BODY;
    ctx.request_len = (int)strlen(ctx.request);

    header_len = snprintf(headers, sizeof(headers),
                          "Authorization: RXN-BASIC cId=%s,mId=%s,pw=%s\r\n"
                          "Content-Type: application/json\r\n"
                          "Accept: application/octet-stream",
                          DEMO_AGNSS_CID, DEMO_AGNSS_MID,
                          DEMO_AGNSS_PASSWORD);
    if (header_len <= 0 || header_len >= (int)sizeof(headers)) {
        result = -2;
        goto cleanup;
    }
    if (!liot_httpc_url_parse(url, &parsed_url)) {
        result = -3;
        goto cleanup;
    }
    if (liot_httpc_new(&client, agnss_http_event, &ctx) != LIOT_HTTPC_SUCCESS) {
        result = -4;
        goto cleanup;
    }
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_SIM_ID, 0);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_PDPCID, 1);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_METHOD, LIOT_HTTPC_METHOD_POST);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_URL, &parsed_url);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_READ_FUNC, agnss_http_read);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_READ_DATA, &ctx);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_UPLOAD_LEN, ctx.request_len);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_BODY_DATA_TYPE,
                      LIOT_HTTPC_RAW_DATA);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_RAW_REQUEST, 0);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_REQUEST_HEADER, headers);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_CUSTOME_HEADER,
                      "Content-Type: application/json");
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_WRITE_FUNC, agnss_http_write);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_WRITE_DATA, &ctx);

    liot_trace("%s AGNSS POST url=%s body_bytes=%d",
               LOCATION_LOG_PREFIX, DEMO_AGNSS_URL, ctx.request_len);
    perform_result = liot_httpc_perform(&client);
    if (perform_result != LIOT_HTTPC_SUCCESS) {
        liot_trace("%s AGNSS perform failed=0x%x", LOCATION_LOG_PREFIX,
                   perform_result);
        result = -5;
        goto cleanup;
    }
    while (!ctx.done && waited < AGNSS_HTTP_TIMEOUT_MS) {
        liot_rtos_task_sleep_ms(100);
        waited += 100;
    }
    liot_trace("%s AGNSS HTTP done=%d event=%d code=0x%x status=%d bytes=%d overflow=%d",
               LOCATION_LOG_PREFIX, ctx.done ? 1 : 0, ctx.event_type,
               ctx.event_result, ctx.http_status, ctx.response_len,
               ctx.overflow ? 1 : 0);
    if (!ctx.done || ctx.event_result != LIOT_HTTPC_SUCCESS ||
        ctx.http_status != 200 || ctx.response_len <= 0 || ctx.overflow) {
        if (ctx.response_len > 0) {
            int text_len = ctx.response_len < 256 ? ctx.response_len : 256;
            liot_trace("%s AGNSS error body=%.*s", LOCATION_LOG_PREFIX,
                       text_len, (char *)ctx.response);
        }
        result = -6;
        goto cleanup;
    }
    waited = 0;
    while (!demo_gnss_is_ready() && waited < AGNSS_GNSS_READY_TIMEOUT_MS) {
        liot_rtos_task_sleep_ms(100);
        waited += 100;
    }
    if (!demo_gnss_is_ready()) {
        result = -7;
        goto cleanup;
    }
    liot_trace("%s AGNSS RTCM head=%02X %02X %02X %02X",
               LOCATION_LOG_PREFIX, ctx.response[0],
               ctx.response_len > 1 ? ctx.response[1] : 0,
               ctx.response_len > 2 ? ctx.response[2] : 0,
               ctx.response_len > 3 ? ctx.response[3] : 0);
    result = demo_gnss_inject_assistance(ctx.response,
                                         (size_t)ctx.response_len);
cleanup:
    if (client != 0) {
        liot_httpc_stop(&client);
        waited = 0;
        while (!ctx.closed && waited < 2000) {
            liot_rtos_task_sleep_ms(100);
            waited += 100;
        }
        liot_httpc_release(&client);
    }
    if (ctx.response != NULL) liot_rtos_free(ctx.response);
    liot_trace("%s AGNSS result=%d", LOCATION_LOG_PREFIX, result);
    return result;
#else
    return -20;
#endif
}

static int agnss_socket_send_all(int socket_fd, const uint8_t *data, int size)
{
    int offset = 0;

    while (offset < size) {
        int sent = lwip_send(socket_fd, data + offset, (size_t)(size - offset), 0);
        if (sent <= 0) return -1;
        offset += sent;
    }
    return offset;
}

static int location_agnss_download_and_inject(void)
{
#ifdef HWDEMO_GNSS_EN
    struct addrinfo hints;
    struct addrinfo *addresses = NULL;
    struct addrinfo *address;
    struct timeval timeout;
    uint8_t *rtcm = NULL;
    char *header = NULL;
    char request[768];
    char *header_end;
    char *content_length_text;
    int socket_fd = -1;
    int request_len;
    int header_len = 0;
    int rtcm_len = 0;
    int content_length = -1;
    int http_status = 0;
    int result = -30;
    int ret;
    int waited;

    (void)location_agnss_download_httpc;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    ret = lwip_getaddrinfowithcid(DEMO_AGNSS_HOST, "80", &hints,
                                  &addresses, 1);
    liot_trace("%s AGNSS socket DNS ret=%d addresses=%p",
               LOCATION_LOG_PREFIX, ret, addresses);
    if (ret != 0 || addresses == NULL) return -31;

    for (address = addresses; address != NULL; address = address->ai_next) {
        socket_fd = lwip_socket(address->ai_family, address->ai_socktype,
                                address->ai_protocol);
        if (socket_fd < 0) continue;
        timeout.tv_sec = 30;
        timeout.tv_usec = 0;
        lwip_setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO,
                        &timeout, sizeof(timeout));
        lwip_setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO,
                        &timeout, sizeof(timeout));
        ret = lwip_connect(socket_fd, address->ai_addr, address->ai_addrlen);
        liot_trace("%s AGNSS socket connect fd=%d ret=%d errno=%d",
                   LOCATION_LOG_PREFIX, socket_fd, ret, errno);
        if (ret == 0) break;
        lwip_close(socket_fd);
        socket_fd = -1;
    }
    lwip_freeaddrinfo(addresses);
    addresses = NULL;
    if (socket_fd < 0) return -32;

    request_len = snprintf(
        request, sizeof(request),
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Authorization: RXN-BASIC cId=%s,mId=%s,pw=%s\r\n"
        "Content-Type: application/json\r\n"
        "Accept: application/octet-stream\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n\r\n%s",
        DEMO_AGNSS_PATH, DEMO_AGNSS_HOST, DEMO_AGNSS_CID,
        DEMO_AGNSS_MID, DEMO_AGNSS_PASSWORD,
        (unsigned int)strlen(AGNSS_REQUEST_BODY), AGNSS_REQUEST_BODY);
    if (request_len <= 0 || request_len >= (int)sizeof(request)) {
        result = -33;
        goto cleanup;
    }
    ret = agnss_socket_send_all(socket_fd, (const uint8_t *)request,
                                request_len);
    liot_trace("%s AGNSS socket request sent=%d/%d",
               LOCATION_LOG_PREFIX, ret, request_len);
    if (ret != request_len) {
        result = -34;
        goto cleanup;
    }

    rtcm = liot_rtos_malloc(AGNSS_RESPONSE_SIZE);
    header = liot_rtos_malloc(AGNSS_SOCKET_HEADER_SIZE);
    if (rtcm == NULL || header == NULL) {
        result = -35;
        goto cleanup;
    }
    while (header_len < AGNSS_SOCKET_HEADER_SIZE - 1) {
        ret = lwip_recv(socket_fd, header + header_len,
                        (size_t)(AGNSS_SOCKET_HEADER_SIZE - 1 - header_len), 0);
        if (ret <= 0) {
            result = -36;
            goto cleanup;
        }
        header_len += ret;
        header[header_len] = '\0';
        header_end = strstr(header, "\r\n\r\n");
        if (header_end != NULL) {
            int body_in_buffer;
            int header_bytes = (int)(header_end + 4 - header);

            sscanf(header, "HTTP/%*s %d", &http_status);
            content_length_text = strstr(header, "Content-Length:");
            if (content_length_text != NULL) {
                sscanf(content_length_text, "Content-Length: %d", &content_length);
            }
            body_in_buffer = header_len - header_bytes;
            if (body_in_buffer > AGNSS_RESPONSE_SIZE) body_in_buffer = AGNSS_RESPONSE_SIZE;
            if (body_in_buffer > 0) {
                memcpy(rtcm, header + header_bytes, (size_t)body_in_buffer);
                rtcm_len = body_in_buffer;
            }
            break;
        }
    }
    liot_trace("%s AGNSS socket HTTP status=%d content_length=%d initial_body=%d",
               LOCATION_LOG_PREFIX, http_status, content_length, rtcm_len);
    if (http_status != 200 || content_length <= 0 ||
        content_length > AGNSS_RESPONSE_SIZE) {
        if (rtcm_len > 0) {
            liot_trace("%s AGNSS socket error body=%.*s",
                       LOCATION_LOG_PREFIX, rtcm_len, (char *)rtcm);
        }
        result = -37;
        goto cleanup;
    }
    while (rtcm_len < content_length) {
        ret = lwip_recv(socket_fd, rtcm + rtcm_len,
                        (size_t)(content_length - rtcm_len), 0);
        if (ret <= 0) {
            result = -38;
            goto cleanup;
        }
        rtcm_len += ret;
    }
    liot_trace("%s AGNSS socket RTCM bytes=%d head=%02X %02X %02X %02X",
               LOCATION_LOG_PREFIX, rtcm_len, rtcm[0], rtcm[1], rtcm[2], rtcm[3]);

    waited = 0;
    while (!demo_gnss_is_ready() && waited < AGNSS_GNSS_READY_TIMEOUT_MS) {
        liot_rtos_task_sleep_ms(100);
        waited += 100;
    }
    if (!demo_gnss_is_ready()) {
        result = -39;
        goto cleanup;
    }
    result = demo_gnss_inject_assistance(rtcm, (size_t)rtcm_len);
    if (result == 0) {
        liot_rtos_task_sleep_ms(500);
        demo_gnss_query_assistance();
    }
cleanup:
    if (socket_fd >= 0) lwip_close(socket_fd);
    if (header != NULL) liot_rtos_free(header);
    if (rtcm != NULL) liot_rtos_free(rtcm);
    liot_trace("%s AGNSS socket result=%d", LOCATION_LOG_PREFIX, result);
    return result;
#else
    return -40;
#endif
}

static int location_network_prepare(void)
{
    liot_data_call_info_t info;
    int ret;
    int attempts = 0;

    liot_trace("%s SDK network register wait start", LOCATION_LOG_PREFIX);
    do {
        ret = liot_network_register_wait(0, 60);
        if (ret == LIOT_DATACALL_SUCCESS) break;
        attempts++;
        if (attempts == 1 || (attempts % 10) == 0) {
            liot_trace("%s SDK register retry=%d ret=0x%x",
                       LOCATION_LOG_PREFIX, attempts, ret);
        }
        liot_rtos_task_sleep_s(1);
    } while (attempts < LOCATION_REGISTER_RETRIES);
    if (ret != LIOT_DATACALL_SUCCESS) return -10;
    liot_trace("%s SDK network registered attempts=%d", LOCATION_LOG_PREFIX, attempts);

    liot_set_data_call_asyn_mode(0, 1, false);
    ret = liot_start_data_call(0, 1, LIOT_DATA_TYPE_IPV4V6,
                               "APNTEST", "", "", LIOT_DATA_AUTH_TYPE_NONE);
    liot_trace("%s SDK data call ret=0x%x", LOCATION_LOG_PREFIX, ret);
    if (ret != LIOT_DATACALL_SUCCESS &&
        ret != LIOT_DATACALL_REPEAT_ACTIVE_ERR) return -11;

    /* Match demo_zhongyi_ai: the synchronous start result is authoritative. */
    liot_rtos_task_sleep_s(4);
    memset(&info, 0, sizeof(info));
    ret = liot_get_data_call_info(0, 1, &info);
    liot_trace("%s PDP info ret=0x%x cid=%d ipver=%d v4state=%d v6state=%d",
               LOCATION_LOG_PREFIX, ret, info.cid, info.ip_version,
               info.v4.state, info.v6.state);
    liot_trace("%s PDP info cid=%d ipver=%d v4state=%d",
               LOCATION_LOG_PREFIX, info.cid, info.ip_version, info.v4.state);
    liot_trace("%s PDP IPv4=%s", LOCATION_LOG_PREFIX,
               liot_ip4addr_ntoa(&info.v4.addr.ip));
    liot_trace("%s PDP DNS1=%s", LOCATION_LOG_PREFIX,
               liot_ip4addr_ntoa(&info.v4.addr.pri_dns));
    liot_trace("%s PDP DNS2=%s", LOCATION_LOG_PREFIX,
               liot_ip4addr_ntoa(&info.v4.addr.sec_dns));
    g_location_network_ready = true;
    return 0;
}

static int location_parse_response(const char *json, char *description, size_t size)
{
    cJSON *root = NULL;
    cJSON *success;
    cJSON *data;
    cJSON *display_name;
    int result = -1;
    if (json == NULL || description == NULL || size == 0) return -1;
    root = cJSON_Parse(json);
    if (root == NULL) goto exit;
    success = cJSON_GetObjectItemCaseSensitive(root, "success");
    data = cJSON_GetObjectItemCaseSensitive(root, "data");
    display_name = cJSON_IsObject(data) ?
        cJSON_GetObjectItemCaseSensitive(data, "displayName") : NULL;
    if (!cJSON_IsTrue(success) || !cJSON_IsString(display_name) ||
        display_name->valuestring == NULL) goto exit;
    snprintf(description, size, "%s", display_name->valuestring);
    result = 0;
exit:
    cJSON_Delete(root);
    return result;
}

static int location_transport_request(const location_fix_t *fix,
                                      char *description, size_t size)
{
    location_http_ctx_t ctx;
    liot_http_client_t client = 0;
    liot_httpc_url_s parsed_url;
    char url[256];
    char longitude[32];
    char latitude[32];
    int waited = 0;
    int result = -1;
    int perform_result;
    if (fix == NULL || !fix->valid) return -1;
    if (!g_location_network_ready) return -10;
    memset(&ctx, 0, sizeof(ctx));
    memset(&parsed_url, 0, sizeof(parsed_url));
    {
        int64_t lon = fix->longitude_e7;
        int64_t lat = fix->latitude_e7;
        uint64_t lon_abs = (uint64_t)(lon < 0 ? -lon : lon);
        uint64_t lat_abs = (uint64_t)(lat < 0 ? -lat : lat);
        snprintf(longitude, sizeof(longitude), "%s%ld.%07ld",
                 lon < 0 ? "-" : "", (long)(lon_abs / 10000000U),
                 (long)(lon_abs % 10000000U));
        snprintf(latitude, sizeof(latitude), "%s%ld.%07ld",
                 lat < 0 ? "-" : "", (long)(lat_abs / 10000000U),
                 (long)(lat_abs % 10000000U));
    }
    snprintf(url, sizeof(url), "%s?longitude=%s&latitude=%s",
             LOCATION_API_URL, longitude, latitude);
    liot_trace("%s GET %s", LOCATION_LOG_PREFIX, url);
    if (!liot_httpc_url_parse(url, &parsed_url)) return -2;
    if (liot_httpc_new(&client, location_http_event, &ctx) != LIOT_HTTPC_SUCCESS) return -3;
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_SIM_ID, 0);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_PDPCID, 1);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_METHOD, LIOT_HTTPC_METHOD_GET);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_URL, &parsed_url);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_WRITE_FUNC, location_http_write);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_WRITE_DATA, &ctx);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_REQUEST_HEADER,
                      "Accept: application/json\r\nAccept-Charset: utf-8");
#ifdef FEATURE_HTTP_TLS_ENABLE
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_SSLCTXID, 2);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_SSL_VERSION, LIOT_SSL_VERSION_3);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_SSL_HS_TIMEOUT, 300);
    liot_httpc_setopt(&client, LIOT_HTTP_CLIENT_OPT_SSL_VERIFY_LEVEL, LIOT_HTTPS_VERIFY_NONE);
#endif
    perform_result = liot_httpc_perform(&client);
    if (perform_result != LIOT_HTTPC_SUCCESS) goto cleanup;
    while (!ctx.done && waited < LOCATION_HTTP_TIMEOUT_MS) {
        liot_rtos_task_sleep_ms(100);
        waited += 100;
    }
    liot_trace("%s HTTP done=%d type=%d code=0x%x perform=0x%x status=%d bytes=%d",
               LOCATION_LOG_PREFIX, ctx.done ? 1 : 0, ctx.event_type,
               ctx.event_result, perform_result, ctx.http_status, ctx.response_len);
    if (ctx.done && ctx.event_result == LIOT_HTTPC_SUCCESS &&
        ctx.http_status >= 200 && ctx.http_status < 300) {
        result = location_parse_response(ctx.response, description, size);
    }
cleanup:
    if (client != 0) {
        liot_httpc_stop(&client);
        waited = 0;
        while (!ctx.closed && waited < 2000) {
            liot_rtos_task_sleep_ms(100);
            waited += 100;
        }
        liot_httpc_release(&client);
    }
    return result;
}

static void location_request_task(void *argument)
{
    uint32_t generation = (uint32_t)(uintptr_t)argument;
    location_fix_t fix = g_location.fix;
    char description[LOCATION_DESC_SIZE];
    int result = location_transport_request(&fix, description, sizeof(description));
    if (generation == g_location.generation) {
        g_location.request_result = result;
        if (result == 0) {
            snprintf(g_location.description, sizeof(g_location.description),
                     "%s", description);
        }
        g_location.result_pending = true;
        g_location.request_task = NULL;
    }
    g_location_request_active = false;
    liot_rtos_task_delete(NULL);
}

static void location_refresh_fix(void)
{
    char text[48];
    if (!location_source_get(&g_location.fix)) {
        g_location.fix.valid = false;
        lv_label_set_text(g_location.latitude_label, "Latitude\nWaiting...");
        lv_label_set_text(g_location.longitude_label, "Longitude\nWaiting...");
        return;
    }
    location_format_coordinate(text, sizeof(text), g_location.fix.latitude_e7, 'N', 'S');
    lv_label_set_text_fmt(g_location.latitude_label, "Latitude\n%s", text);
    location_format_coordinate(text, sizeof(text), g_location.fix.longitude_e7, 'E', 'W');
    lv_label_set_text_fmt(g_location.longitude_label, "Longitude\n%s", text);
}

static void location_update_ready_state(bool update_status)
{
    if (g_location.request_running) return;
    if (!g_location_network_ready || !g_location.fix.valid ||
        g_location_request_active) {
        lv_obj_add_state(g_location.locate_button, LV_STATE_DISABLED);
    } else {
        lv_obj_clear_state(g_location.locate_button, LV_STATE_DISABLED);
    }
    if (!update_status) return;
    if (!g_location_network_ready) {
        lv_label_set_text(g_location.status_label, "Connecting network...");
    } else if (!g_location.fix.valid) {
        lv_label_set_text(g_location.status_label, "Waiting for GNSS fix...");
    } else {
        lv_label_set_text(g_location.status_label, "Ready to locate");
    }
}

static void location_result_timer(lv_timer_t *timer)
{
    uint32_t now = lv_tick_get();
    (void)timer;
    if ((now - g_location.fix_poll_tick) >= LOCATION_FIX_POLL_MS) {
        bool was_valid = g_location.fix.valid;
        int32_t old_latitude = g_location.fix.latitude_e7;
        int32_t old_longitude = g_location.fix.longitude_e7;
        g_location.fix_poll_tick = now;
        location_refresh_fix();
        if (!was_valid && g_location.fix.valid) {
            liot_trace("%s GNSS fix acquired lat_e7=%ld lon_e7=%ld",
                       LOCATION_LOG_PREFIX, (long)g_location.fix.latitude_e7,
                       (long)g_location.fix.longitude_e7);
        } else if (g_location.fix.valid &&
                   (old_latitude != g_location.fix.latitude_e7 ||
                    old_longitude != g_location.fix.longitude_e7)) {
            liot_trace("%s GNSS fix updated lat_e7=%ld lon_e7=%ld",
                       LOCATION_LOG_PREFIX, (long)g_location.fix.latitude_e7,
                       (long)g_location.fix.longitude_e7);
        }
        location_update_ready_state(true);
    }
    if (g_location_network_result_pending) {
        g_location_network_result_pending = false;
        if (g_location.page_active) {
            if (g_location_network_result == 0) {
                location_update_ready_state(true);
            } else {
                lv_label_set_text(g_location.status_label, "Network unavailable");
            }
        }
    }
    if (!g_location.result_pending) return;
    g_location.result_pending = false;
    g_location.request_running = false;
    if (!g_location.page_active) return;
    if (g_location.request_result == 0) {
        lv_label_set_text(g_location.status_label, "Located");
        lv_label_set_text(g_location.description_label, g_location.description);
    } else if (g_location.request_result <= -10 &&
               g_location.request_result >= -16) {
        lv_label_set_text(g_location.status_label, "Network unavailable");
        lv_label_set_text(g_location.description_label, "Check SIM and mobile network");
    } else {
        lv_label_set_text(g_location.status_label, "Request failed");
        lv_label_set_text(g_location.description_label, "No location description");
    }
    location_update_ready_state(false);
}

static void location_locate_clicked(lv_event_t *event)
{
    LiotOSStatus_t result;
    (void)event;
    location_refresh_fix();
    if (g_location.request_running || g_location_request_active ||
        !g_location_network_ready ||
        !g_location.fix.valid) {
        location_update_ready_state(true);
        return;
    }
    g_location.request_running = true;
    g_location_request_active = true;
    lv_obj_add_state(g_location.locate_button, LV_STATE_DISABLED);
    lv_label_set_text(g_location.status_label, "Requesting server...");
    lv_label_set_text(g_location.description_label, "Waiting for server response");
    result = liot_rtos_task_create(&g_location.request_task, LOCATION_TASK_STACK,
                                   LIOT_APP_TASK_PRIORITY, "location_req",
                                   location_request_task,
                                   (void *)(uintptr_t)g_location.generation);
    liot_trace("%s request task ret=%d handle=%p", LOCATION_LOG_PREFIX,
               (int)result, g_location.request_task);
    if (result != LIOT_OSI_SUCCESS) {
        g_location.request_running = false;
        g_location_request_active = false;
        lv_obj_clear_state(g_location.locate_button, LV_STATE_DISABLED);
        lv_label_set_text(g_location.status_label, "Cannot start request");
    }
}

static void location_page_exit(void)
{
    lv_obj_t *old_screen = g_location.screen;

    if (!g_location.page_active || old_screen == NULL) return;
    liot_trace("%s exit begin screen=%p menu=%p request=%d",
               LOCATION_LOG_PREFIX, old_screen, guider_ui.positioning,
               g_location_request_active ? 1 : 0);
    g_location.page_active = false;
    g_location.page_exiting = false;
    g_location.generation = ++g_location_generation;
    if (g_location.result_timer != NULL) {
        lv_timer_del(g_location.result_timer);
        g_location.result_timer = NULL;
    }
    if (g_location.exit_timer != NULL) {
        lv_timer_del(g_location.exit_timer);
        g_location.exit_timer = NULL;
    }
    ui_load_scr_animation(&guider_ui, &guider_ui.positioning,
                          guider_ui.positioning_del, &g_location.page_del,
                          setup_scr_positioning, LV_SCR_LOAD_ANIM_FADE_ON,
                          200, 0, false, false);
    if (old_screen != guider_ui.positioning) lv_obj_del_delayed(old_screen, 800);
    g_location.screen = NULL;
    g_location.latitude_label = NULL;
    g_location.longitude_label = NULL;
    g_location.status_label = NULL;
    g_location.description_label = NULL;
    g_location.locate_button = NULL;
    g_location.page_del = true;
    liot_trace("%s exit end menu=%p", LOCATION_LOG_PREFIX, guider_ui.positioning);
}

static void location_exit_timer_cb(lv_timer_t *timer)
{
    if (g_location.exit_timer == timer) g_location.exit_timer = NULL;
    lv_timer_del(timer);
    location_page_exit();
}

static void location_exit_request(void)
{
    lv_indev_t *indev;

    if (g_location.page_exiting) return;
    g_location.page_exiting = true;
    indev = lv_indev_get_act();
    if (indev != NULL) lv_indev_wait_release(indev);
    g_location.exit_timer = lv_timer_create(location_exit_timer_cb,
                                             LOCATION_EXIT_DELAY_MS, NULL);
    lv_timer_set_repeat_count(g_location.exit_timer, 1);
    liot_trace("%s exit request", LOCATION_LOG_PREFIX);
}

static void location_page_event(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *indev;
    lv_point_t point;
    int32_t dx;
    int32_t dy;

    if (code == LV_EVENT_PRESSED) {
        g_location.press_candidate =
            (lv_event_get_target(event) == lv_event_get_current_target(event));
        if (!g_location.press_candidate) return;
        indev = lv_indev_get_act();
        if (indev == NULL) {
            g_location.press_candidate = false;
            return;
        }
        lv_indev_get_point(indev, &g_location.press_point);
        g_location.press_tick = lv_tick_get();
        return;
    }
    if (code == LV_EVENT_PRESSING && g_location.press_candidate) {
        indev = lv_indev_get_act();
        if (indev == NULL) return;
        lv_indev_get_point(indev, &point);
        dx = point.x - g_location.press_point.x;
        dy = point.y - g_location.press_point.y;
        if (dx > LOCATION_MOVE_LIMIT || dx < -LOCATION_MOVE_LIMIT ||
            dy > LOCATION_MOVE_LIMIT || dy < -LOCATION_MOVE_LIMIT) {
            g_location.press_candidate = false;
            return;
        }
        if (lv_tick_elaps(g_location.press_tick) >= LOCATION_HOLD_MS) {
            g_location.press_candidate = false;
            lv_event_stop_bubbling(event);
            lv_event_stop_processing(event);
            location_exit_request();
        }
        return;
    }
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        g_location.press_candidate = false;
    }
}

static lv_obj_t *location_label(lv_obj_t *parent, const char *text, int x, int y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    return label;
}

void demo_location_page_enter(void)
{
    lv_obj_t *button_text;
    if (g_location.page_active) return;
    memset(&g_location, 0, sizeof(g_location));
    g_location.generation = ++g_location_generation;
    g_location.page_active = true;

    g_location.screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_location.screen, lv_color_hex(0x071D2B), 0);
    lv_obj_set_style_bg_opa(g_location.screen, LV_OPA_COVER, 0);
    lv_obj_add_flag(g_location.screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_location.screen, location_page_event, LV_EVENT_ALL, NULL);

    lv_obj_t *title = location_label(g_location.screen, "Location", 128, 24);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    g_location.status_label = location_label(g_location.screen, "Starting...", 92, 58);
    g_location.latitude_label = location_label(g_location.screen, "Latitude", 42, 104);
    g_location.longitude_label = location_label(g_location.screen, "Longitude", 200, 104);

    g_location.locate_button = lv_btn_create(g_location.screen);
    lv_obj_set_size(g_location.locate_button, 180, 54);
    lv_obj_align(g_location.locate_button, LV_ALIGN_CENTER, 0, 38);
    lv_obj_add_event_cb(g_location.locate_button, location_locate_clicked,
                        LV_EVENT_CLICKED, NULL);
    lv_obj_add_state(g_location.locate_button, LV_STATE_DISABLED);
    button_text = lv_label_create(g_location.locate_button);
    lv_label_set_text(button_text, "Locate");
    lv_obj_center(button_text);

    g_location.description_label = location_label(
        g_location.screen, "No server location yet", 45, 248);
    lv_obj_set_size(g_location.description_label, 270, 44);
    lv_label_set_long_mode(g_location.description_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(g_location.description_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(g_location.description_label, &lv_font_location_cjk_16, 0);
    location_refresh_fix();
    location_update_ready_state(true);
    g_location.result_timer = lv_timer_create(location_result_timer, 100, NULL);
    lv_scr_load_anim(g_location.screen, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, false);
    liot_trace("%s page enter GNSS valid=%d lat_e7=%ld lon_e7=%ld",
               LOCATION_LOG_PREFIX, g_location.fix.valid ? 1 : 0,
               (long)g_location.fix.latitude_e7,
               (long)g_location.fix.longitude_e7);
}

static void location_enter_async(void *argument)
{
    (void)argument;
    demo_location_page_enter();
}

void demo_location_page_enter_async(void)
{
    lv_async_call(location_enter_async, NULL);
}

#ifdef HWDEMO_LOCATION_HEADLESS_EN
static void location_key_isr(void *argument)
{
    uint32_t now = liot_rtos_get_system_tick();
    (void)argument;
    if ((now - g_location_key_tick) < LOCATION_KEY_DEBOUNCE_MS) return;
    g_location_key_tick = now;
    if (g_location_key_sem != NULL) liot_rtos_semaphore_release(g_location_key_sem);
}

static int location_key_init(void)
{
    liot_intcb_t callback;
    int ret;
    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_AON, L_VOLT_3_30V);
    Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL);
    Liot_SetPinFunc(LOCATION_KEY_MODEM_PIN, L_PIN_FUNC_0);
    callback.signal = L_INT_EDGE_FALL;
    callback.callback = location_key_isr;
    callback.arg = NULL;
    ret = Liot_GpioInit(LOCATION_KEY_GPIO, L_IO_INPUT, L_IO_HIGH, &callback);
    if (ret != L_GPIO_ERR_SUCCESS) return -1;
    ret = Liot_GpioIntEnable();
    if (ret != L_GPIO_ERR_SUCCESS) return -2;
    liot_trace("%s key ready GPIO20 level=%d", LOCATION_LOG_PREFIX,
               Liot_GpioGetLevel(LOCATION_KEY_GPIO));
    return 0;
}
#endif

void liot_location_demo_thread(void *argument)
{
    (void)argument;
    liot_trace("%s standalone demo start", LOCATION_LOG_PREFIX);
#ifdef HWDEMO_LOCATION_HEADLESS_EN
    location_fix_t fix;
    char description[LOCATION_DESC_SIZE];
    int result;
    liot_rtos_semaphore_create(&g_location_key_sem, 0);
    result = location_key_init();
    liot_trace("%s key init result=%d", LOCATION_LOG_PREFIX, result);
    if (!location_source_get(&fix)) {
        liot_trace("%s mock GNSS unavailable", LOCATION_LOG_PREFIX);
        liot_rtos_task_delete(NULL);
        return;
    }
    liot_trace("%s mock GNSS lat_e7=%ld lon_e7=%ld", LOCATION_LOG_PREFIX,
               (long)fix.latitude_e7, (long)fix.longitude_e7);
    do {
        result = location_network_prepare();
        liot_trace("%s boot network result=%d", LOCATION_LOG_PREFIX, result);
        if (result != 0) liot_rtos_task_sleep_s(10);
    } while (result != 0);
    liot_trace("%s AGNSS start after network ready", LOCATION_LOG_PREFIX);
    location_agnss_download_and_inject();
    liot_trace("%s network ready; press KEY_USER0 for GET", LOCATION_LOG_PREFIX);
    while (1) {
        liot_rtos_semaphore_wait(g_location_key_sem, LIOT_WAIT_FOREVER);
        memset(description, 0, sizeof(description));
        liot_trace("%s key pressed; GET begin", LOCATION_LOG_PREFIX);
        result = location_transport_request(&fix, description, sizeof(description));
        liot_trace("%s GET result=%d location=%s", LOCATION_LOG_PREFIX, result,
                   result == 0 ? description : "<unavailable>");
    }
#else
    lvgl_init();
    demo_location_page_enter_async();
    liot_rtos_task_sleep_ms(300);
    do {
        g_location_network_result = location_network_prepare();
        g_location_network_result_pending = true;
        liot_trace("%s boot network result=%d", LOCATION_LOG_PREFIX,
                   g_location_network_result);
        if (g_location_network_result != 0) liot_rtos_task_sleep_s(10);
    } while (g_location_network_result != 0);
    liot_trace("%s AGNSS start after network ready", LOCATION_LOG_PREFIX);
    location_agnss_download_and_inject();
    while (1) {
        liot_rtos_task_sleep_s(10);
    }
#endif
}

void demo_location_service_thread(void *argument)
{
    (void)argument;
    liot_trace("%s background network service start", LOCATION_LOG_PREFIX);
    do {
        g_location_network_result = location_network_prepare();
        g_location_network_result_pending = true;
        liot_trace("%s background network result=%d", LOCATION_LOG_PREFIX,
                   g_location_network_result);
        if (g_location_network_result != 0) liot_rtos_task_sleep_s(10);
    } while (g_location_network_result != 0);
    liot_trace("%s AGNSS start after network ready", LOCATION_LOG_PREFIX);
    location_agnss_download_and_inject();
    liot_rtos_task_delete(NULL);
}
