#include "app_camera_port_liot.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "app_osal.h"

#define APP_CAMERA_LIOT_DEFAULT_BACKEND APP_CAMERA_LIOT_BACKEND_GC032A
#define APP_CAMERA_LIOT_DEFAULT_SENSOR APP_CAMERA_LIOT_SENSOR_GC032A_2DDR
#define APP_CAMERA_LIOT_DEFAULT_POWER_GPIO 27
#define APP_CAMERA_LIOT_DEFAULT_POWER_ON_DELAY_MS 10U
#define APP_CAMERA_LIOT_DEFAULT_WIDTH 640U
#define APP_CAMERA_LIOT_DEFAULT_HEIGHT 480U
#define APP_CAMERA_LIOT_DEFAULT_BYTES_PER_PIXEL 2U
#define APP_CAMERA_LIOT_DEFAULT_OUTPUT_FORMAT APP_CAMERA_OUTPUT_YUYV
#define APP_CAMERA_LIOT_DEFAULT_CSPI_PORT 1
#define APP_CAMERA_LIOT_DEFAULT_CSPI_SPEED 2
#define APP_CAMERA_LIOT_DEFAULT_I2C_NUM 0
#define APP_CAMERA_LIOT_DEFAULT_I2C_SDA_PIN 255
#define APP_CAMERA_LIOT_DEFAULT_I2C_SCL_PIN 255
#define APP_CAMERA_LIOT_DEFAULT_CAPTURE_TIMEOUT_MS 1000U
#define APP_CAMERA_LIOT_DEFAULT_CAPTURE_PERIOD_MS 0U
#define APP_CAMERA_LIOT_FRAME_PADDING_BYTES (4U * 1024U)

#ifndef APP_CAMERA_LIOT_ENABLE_GC032A
#define APP_CAMERA_LIOT_ENABLE_GC032A 1
#endif

#if APP_CAMERA_LIOT_ENABLE_GC032A
#include "liot_camera.h"
#include "liot_gpio2.h"
#include "liot_sleep.h"

LIOT_ADD_CAMERA(liot_gc032a_2ddr);
#endif

typedef struct {
    app_camera_liot_config_t config;
    uint8_t *frame_buf;
    uint32_t frame_bytes;
    uint32_t frame_capacity;
    uint32_t frame_alloc_bytes;
    uint32_t frame_id;
    bool configured;
    bool initialized;
#if APP_CAMERA_LIOT_ENABLE_GC032A
    liot_camera_handle_t handle;
#endif
} app_camera_liot_ctx_t;

static int camera_liot_init(const app_camera_caps_t *caps);
static int camera_liot_deinit(void);
static int camera_liot_capture_frame(uint8_t *data, uint32_t len, uint32_t timeout_ms);
static int camera_liot_get_frame_buffer(uint8_t **data, uint32_t *len, uint32_t *capacity);

static app_camera_liot_ctx_t s_camera_liot;

static const app_camera_port_ops_t s_camera_liot_ops = {
    .init = camera_liot_init,
    .deinit = camera_liot_deinit,
    .capture_frame = camera_liot_capture_frame,
    .get_frame_buffer = camera_liot_get_frame_buffer,
};

static app_camera_port_t s_camera_liot_port = {
    .ops = &s_camera_liot_ops,
};

static bool camera_liot_backend_available(app_camera_liot_backend_t backend)
{
    switch (backend) {
    case APP_CAMERA_LIOT_BACKEND_NONE:
        return true;
    case APP_CAMERA_LIOT_BACKEND_GC032A:
        return APP_CAMERA_LIOT_ENABLE_GC032A != 0;
    default:
        return false;
    }
}

static uint8_t camera_liot_bytes_per_pixel(app_camera_output_t output)
{
    switch (output) {
    case APP_CAMERA_OUTPUT_GRAY:
        return 1U;
    case APP_CAMERA_OUTPUT_YUYV:
    case APP_CAMERA_OUTPUT_RGB565:
    default:
        return 2U;
    }
}

static void camera_liot_apply_default_config(app_camera_liot_config_t *config)
{
    if (config == NULL) {
        return;
    }
    if (config->width == 0U) {
        config->width = APP_CAMERA_LIOT_DEFAULT_WIDTH;
    }
    if (config->height == 0U) {
        config->height = APP_CAMERA_LIOT_DEFAULT_HEIGHT;
    }
    if (config->bytes_per_pixel == 0U) {
        config->bytes_per_pixel = camera_liot_bytes_per_pixel(config->output_format);
    }
    if (config->cspi_port < 0) {
        config->cspi_port = APP_CAMERA_LIOT_DEFAULT_CSPI_PORT;
    }
    if (config->cspi_speed < 0) {
        config->cspi_speed = APP_CAMERA_LIOT_DEFAULT_CSPI_SPEED;
    }
    if (config->i2c_num < 0) {
        config->i2c_num = APP_CAMERA_LIOT_DEFAULT_I2C_NUM;
    }
    if (config->i2c_sda_pin < 0) {
        config->i2c_sda_pin = APP_CAMERA_LIOT_DEFAULT_I2C_SDA_PIN;
    }
    if (config->i2c_scl_pin < 0) {
        config->i2c_scl_pin = APP_CAMERA_LIOT_DEFAULT_I2C_SCL_PIN;
    }
    if (config->capture_timeout_ms == 0U) {
        config->capture_timeout_ms = APP_CAMERA_LIOT_DEFAULT_CAPTURE_TIMEOUT_MS;
    }
    /* capture_period_ms == 0 means no delay between successful captures. */
    if (config->frame_buffer_capacity != 0U &&
        config->frame_buffer_capacity < ((uint32_t)config->width *
                                         (uint32_t)config->height *
                                         (uint32_t)config->bytes_per_pixel)) {
        config->frame_buffer_capacity = 0U;
    }
}

int app_camera_liot_get_default_config(app_camera_liot_config_t *config)
{
    if (config == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    memset(config, 0, sizeof(*config));
    config->backend = APP_CAMERA_LIOT_DEFAULT_BACKEND;
    config->sensor = APP_CAMERA_LIOT_DEFAULT_SENSOR;
    config->power_gpio = APP_CAMERA_LIOT_DEFAULT_POWER_GPIO;
    config->power_on_delay_ms = APP_CAMERA_LIOT_DEFAULT_POWER_ON_DELAY_MS;
    config->width = APP_CAMERA_LIOT_DEFAULT_WIDTH;
    config->height = APP_CAMERA_LIOT_DEFAULT_HEIGHT;
    config->bytes_per_pixel = APP_CAMERA_LIOT_DEFAULT_BYTES_PER_PIXEL;
    config->output_format = APP_CAMERA_LIOT_DEFAULT_OUTPUT_FORMAT;
    config->cspi_port = APP_CAMERA_LIOT_DEFAULT_CSPI_PORT;
    config->cspi_speed = APP_CAMERA_LIOT_DEFAULT_CSPI_SPEED;
    config->i2c_num = APP_CAMERA_LIOT_DEFAULT_I2C_NUM;
    config->i2c_sda_pin = APP_CAMERA_LIOT_DEFAULT_I2C_SDA_PIN;
    config->i2c_scl_pin = APP_CAMERA_LIOT_DEFAULT_I2C_SCL_PIN;
    config->capture_timeout_ms = APP_CAMERA_LIOT_DEFAULT_CAPTURE_TIMEOUT_MS;
    config->capture_period_ms = APP_CAMERA_LIOT_DEFAULT_CAPTURE_PERIOD_MS;
    return APP_OK;
}

static void camera_liot_update_caps(const app_camera_liot_config_t *config)
{
    memset(&s_camera_liot_port.caps, 0, sizeof(s_camera_liot_port.caps));
    if (config == NULL || config->backend == APP_CAMERA_LIOT_BACKEND_NONE) {
        return;
    }

    s_camera_liot_port.caps.has_capture = true;
    s_camera_liot_port.caps.width = config->width;
    s_camera_liot_port.caps.height = config->height;
    s_camera_liot_port.caps.bytes_per_pixel = config->bytes_per_pixel;
    s_camera_liot_port.caps.output_format = config->output_format;
}

static int camera_liot_require_ready(void)
{
    if (!s_camera_liot.configured || !s_camera_liot.initialized) {
        return APP_ERR_NOT_READY;
    }
    return APP_OK;
}

static int camera_liot_calc_frame_bytes(uint32_t *frame_bytes)
{
    uint32_t pixel_count;

    if (frame_bytes == NULL || s_camera_liot.config.width == 0U ||
        s_camera_liot.config.height == 0U ||
        s_camera_liot.config.bytes_per_pixel == 0U) {
        return APP_ERR_INVALID_ARG;
    }

    pixel_count = (uint32_t)s_camera_liot.config.width *
                  (uint32_t)s_camera_liot.config.height;
    if (pixel_count == 0U ||
        pixel_count > (UINT32_MAX / s_camera_liot.config.bytes_per_pixel)) {
        return APP_ERR_NO_MEMORY;
    }

    *frame_bytes = pixel_count * s_camera_liot.config.bytes_per_pixel;
    return APP_OK;
}

static uint32_t camera_liot_frame_capacity(uint32_t frame_bytes)
{
    uint32_t capacity = s_camera_liot.config.frame_buffer_capacity;

    if (capacity < frame_bytes) {
        capacity = frame_bytes;
    }
    return capacity;
}

static uint32_t camera_liot_frame_alloc_bytes(uint32_t frame_capacity)
{
    if (frame_capacity == 0U ||
        frame_capacity > (UINT32_MAX - APP_CAMERA_LIOT_FRAME_PADDING_BYTES)) {
        return 0U;
    }

    return frame_capacity + APP_CAMERA_LIOT_FRAME_PADDING_BYTES;
}

#if APP_CAMERA_LIOT_ENABLE_GC032A
static int camera_liot_output_to_liot(app_camera_output_t output,
                                      liot_camera_output_format_e *liot_output)
{
    if (liot_output == NULL) {
        return APP_ERR_INVALID_ARG;
    }

    switch (output) {
    case APP_CAMERA_OUTPUT_GRAY:
        *liot_output = LIOT_CAMERA_OUTPUT_GRAY;
        return APP_OK;
    case APP_CAMERA_OUTPUT_YUYV:
        *liot_output = LIOT_CAMERA_OUTPUT_YUYV;
        return APP_OK;
    case APP_CAMERA_OUTPUT_RGB565:
        *liot_output = LIOT_CAMERA_OUTPUT_RGB565;
        return APP_OK;
    default:
        return APP_ERR_NOT_SUPPORTED;
    }
}

static liot_camera_sensor_t *camera_liot_sensor(void)
{
    switch (s_camera_liot.config.sensor) {
    case APP_CAMERA_LIOT_SENSOR_GC032A_2DDR:
    default:
        return &liot_gc032a_2ddr;
    }
}

static int camera_liot_power_on(void)
{
    LiotSleepModeCfg_t mode_cfg = {LIOT_SLEEP_MODE_NORMAL};
    liot_gpioerr_e gpio_ret;

    app_log("camera liot power: AON enable");
    if (Liot_AonPowerCtl(true) != L_GPIO_ERR_SUCCESS) {
        app_log("camera liot power AON enable failed");
        return APP_ERR_FAIL;
    }
    app_log("camera liot power: set 3.3V domain");
    if (Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V) != L_GPIO_ERR_SUCCESS) {
        app_log("camera liot power voltage set failed");
        return APP_ERR_FAIL;
    }
    if (Liot_SleepSetMode(&mode_cfg) != LIOT_SLEEP_SUCCESS) {
        app_log("camera liot power sleep mode set failed");
        return APP_ERR_FAIL;
    }
    if (s_camera_liot.config.power_gpio >= 0) {
        app_log("camera liot power: GPIO%d high",
                s_camera_liot.config.power_gpio);
        gpio_ret = Liot_GpioInit((liot_gpio_e)s_camera_liot.config.power_gpio,
                                 L_IO_OUTPUT,
                                 L_IO_HIGH,
                                 NULL);
        if (gpio_ret != L_GPIO_ERR_SUCCESS) {
            app_log("camera liot power GPIO%d init failed: %d",
                    s_camera_liot.config.power_gpio,
                    (int)gpio_ret);
            return APP_ERR_FAIL;
        }
        if (s_camera_liot.config.power_on_delay_ms != 0U) {
            app_log("camera liot power settling: %u ms",
                    (unsigned int)s_camera_liot.config.power_on_delay_ms);
            app_os_task_delay_ms(s_camera_liot.config.power_on_delay_ms);
        }
    }
    app_log("camera liot power ready");
    return APP_OK;
}

static int camera_liot_open(void)
{
    liot_camera_output_format_e output;
    liot_camera_config_t cfg;
    int ret;

    if (s_camera_liot.handle != NULL) {
        return APP_OK;
    }

    app_log("camera liot open: sensor=%d cspi=%d speed=%d i2c=%d sda=%d scl=%d %ux%u %s",
            (int)s_camera_liot.config.sensor,
            s_camera_liot.config.cspi_port,
            s_camera_liot.config.cspi_speed,
            s_camera_liot.config.i2c_num,
            s_camera_liot.config.i2c_sda_pin,
            s_camera_liot.config.i2c_scl_pin,
            (unsigned int)s_camera_liot.config.width,
            (unsigned int)s_camera_liot.config.height,
            app_camera_output_name(s_camera_liot.config.output_format));
    ret = camera_liot_output_to_liot(s_camera_liot.config.output_format, &output);
    if (ret != APP_OK) {
        app_log("camera liot output map failed: %d", ret);
        return ret;
    }
    ret = camera_liot_power_on();
    if (ret != APP_OK) {
        app_log("camera liot power on failed: %d", ret);
        return ret;
    }

    memset(&cfg, 0, sizeof(cfg));
    cfg.sensor = camera_liot_sensor();
    cfg.cspi.num = (liot_cspi_port_e)s_camera_liot.config.cspi_port;
    cfg.cspi.speed = (liot_camFrequence_e)s_camera_liot.config.cspi_speed;
    cfg.i2c.num = (liot_i2c_channel_e)s_camera_liot.config.i2c_num;
    cfg.i2c.scl = (int8_t)s_camera_liot.config.i2c_scl_pin;
    cfg.i2c.sda = (int8_t)s_camera_liot.config.i2c_sda_pin;
    cfg.info.format = output;
    cfg.info.resolution.width.offset = 0U;
    cfg.info.resolution.width.scale = 0U;
    cfg.info.resolution.width.size = s_camera_liot.config.width;
    cfg.info.resolution.height.offset = 0U;
    cfg.info.resolution.height.scale = 0U;
    cfg.info.resolution.height.size = s_camera_liot.config.height;

    s_camera_liot.handle = Liot_CameraInit(&cfg);
    if (s_camera_liot.handle == NULL) {
        app_log("camera liot open failed");
        return APP_ERR_FAIL;
    }

    app_log("camera liot open complete");
    return APP_OK;
}

static int camera_liot_close(void)
{
    liot_errcode_camera_e ret;

    if (s_camera_liot.handle == NULL) {
        return APP_OK;
    }

    ret = Liot_CameraDeinit(s_camera_liot.handle);
    s_camera_liot.handle = NULL;
    return (ret == LIOT_CAMERA_SUCCESS) ? APP_OK : APP_ERR_FAIL;
}

static int camera_liot_capture_to(uint8_t *data, uint32_t len, uint32_t timeout_ms)
{
    liot_errcode_camera_e ret;

    if (data == NULL || len < s_camera_liot.frame_bytes) {
        return APP_ERR_INVALID_ARG;
    }
    if (camera_liot_open() != APP_OK) {
        return APP_ERR_FAIL;
    }

    ret = Liot_CameraCaptureImage(s_camera_liot.handle,
                                  data,
                                  (timeout_ms != 0U) ? timeout_ms :
                                  s_camera_liot.config.capture_timeout_ms);
    return (int)ret;
}
#endif

static int camera_liot_capture_backend(uint8_t *data, uint32_t len, uint32_t timeout_ms)
{
    if (s_camera_liot.config.backend != APP_CAMERA_LIOT_BACKEND_GC032A) {
        return APP_ERR_NOT_SUPPORTED;
    }
#if APP_CAMERA_LIOT_ENABLE_GC032A
    return camera_liot_capture_to(data, len, timeout_ms);
#else
    (void)data;
    (void)len;
    (void)timeout_ms;
    return APP_ERR_NOT_SUPPORTED;
#endif
}

static int camera_liot_init(const app_camera_caps_t *caps)
{
    uint32_t frame_bytes;
    uint32_t frame_capacity;
    uint32_t frame_alloc_bytes;
    int ret;

    (void)caps;
    if (!s_camera_liot.configured) {
        app_log("camera liot init rejected: not configured");
        return APP_ERR_NOT_READY;
    }
    if (!camera_liot_backend_available(s_camera_liot.config.backend)) {
        app_log("camera liot init rejected: backend=%d unavailable",
                (int)s_camera_liot.config.backend);
        return APP_ERR_NOT_SUPPORTED;
    }

    ret = camera_liot_calc_frame_bytes(&frame_bytes);
    if (ret != APP_OK) {
        app_log("camera liot frame size calculation failed: %d", ret);
        return ret;
    }
    frame_capacity = camera_liot_frame_capacity(frame_bytes);
    frame_alloc_bytes = camera_liot_frame_alloc_bytes(frame_capacity);
    if (frame_alloc_bytes == 0U) {
        app_log("camera liot frame alloc size invalid: frame=%lu",
                (unsigned long)frame_bytes);
        return APP_ERR_NO_MEMORY;
    }
    if (s_camera_liot.frame_buf != NULL &&
        s_camera_liot.frame_alloc_bytes < frame_alloc_bytes) {
        app_os_free(s_camera_liot.frame_buf);
        s_camera_liot.frame_buf = NULL;
        s_camera_liot.frame_alloc_bytes = 0U;
    }
    if (s_camera_liot.frame_buf == NULL) {
        s_camera_liot.frame_buf = (uint8_t *)app_os_malloc(frame_alloc_bytes);
        if (s_camera_liot.frame_buf == NULL) {
            app_log("camera liot frame buffer alloc failed: expected=%lu alloc=%lu",
                    (unsigned long)frame_bytes,
                    (unsigned long)frame_alloc_bytes);
            return APP_ERR_NO_MEMORY;
        }
        s_camera_liot.frame_alloc_bytes = frame_alloc_bytes;
    }
    memset(s_camera_liot.frame_buf, 0, frame_capacity);
    s_camera_liot.frame_bytes = frame_bytes;
    s_camera_liot.frame_capacity = frame_capacity;
    s_camera_liot.frame_id = 0U;
    s_camera_liot.initialized = true;
    app_log("camera liot initialized: backend=%d %ux%u %s expected=%lu capacity=%lu alloc=%lu padding=%lu",
            (int)s_camera_liot.config.backend,
            (unsigned int)s_camera_liot.config.width,
            (unsigned int)s_camera_liot.config.height,
            app_camera_output_name(s_camera_liot.config.output_format),
            (unsigned long)s_camera_liot.frame_bytes,
            (unsigned long)s_camera_liot.frame_capacity,
            (unsigned long)s_camera_liot.frame_alloc_bytes,
            (unsigned long)(s_camera_liot.frame_alloc_bytes -
                            s_camera_liot.frame_capacity));
    return APP_OK;
}

static int camera_liot_deinit(void)
{
    int ret = camera_liot_require_ready();

    if (ret != APP_OK) {
        return ret;
    }

#if APP_CAMERA_LIOT_ENABLE_GC032A
    ret = camera_liot_close();
    if (ret != APP_OK) {
        return ret;
    }
#endif
    if (s_camera_liot.frame_buf != NULL) {
        app_log("camera liot deinit: free frame buffer alloc=%lu",
                (unsigned long)s_camera_liot.frame_alloc_bytes);
        app_os_free(s_camera_liot.frame_buf);
        s_camera_liot.frame_buf = NULL;
    }
    s_camera_liot.frame_bytes = 0U;
    s_camera_liot.frame_capacity = 0U;
    s_camera_liot.frame_alloc_bytes = 0U;
    s_camera_liot.initialized = false;
    app_log("camera liot deinitialized");
    return APP_OK;
}

static int camera_liot_capture_frame(uint8_t *data, uint32_t len, uint32_t timeout_ms)
{
    int ret = camera_liot_require_ready();

    if (ret != APP_OK) {
        return ret;
    }
    if (!s_camera_liot_port.caps.has_capture) {
        return APP_ERR_NOT_SUPPORTED;
    }
    ret = camera_liot_capture_backend(data, len, timeout_ms);
    s_camera_liot.frame_id++;
    return ret;
}

static int camera_liot_get_frame_buffer(uint8_t **data, uint32_t *len, uint32_t *capacity)
{
    int ret = camera_liot_require_ready();

    if (ret != APP_OK) {
        return ret;
    }
    if (data == NULL || len == NULL || capacity == NULL ||
        s_camera_liot.frame_buf == NULL ||
        s_camera_liot.frame_bytes == 0U ||
        s_camera_liot.frame_capacity == 0U) {
        return APP_ERR_INVALID_ARG;
    }

    *data = s_camera_liot.frame_buf;
    *len = s_camera_liot.frame_bytes;
    *capacity = s_camera_liot.frame_capacity;
    return APP_OK;
}

int app_camera_liot_setup(const app_camera_liot_config_t *config)
{
    app_camera_liot_config_t effective_config;

    if (config == NULL) {
        app_log("camera liot setup rejected: null config");
        return APP_ERR_INVALID_ARG;
    }
    if (!camera_liot_backend_available(config->backend)) {
        app_log("camera liot setup rejected: backend=%d unavailable", (int)config->backend);
        return APP_ERR_NOT_SUPPORTED;
    }

    effective_config = *config;
    camera_liot_apply_default_config(&effective_config);

    memset(&s_camera_liot, 0, sizeof(s_camera_liot));
    s_camera_liot.config = effective_config;
    s_camera_liot.configured = true;
    camera_liot_update_caps(&effective_config);
    app_log("camera liot setup: backend=%d sensor=%d %ux%u cspi=%d i2c=%d",
            (int)effective_config.backend,
            (int)effective_config.sensor,
            (unsigned int)effective_config.width,
            (unsigned int)effective_config.height,
            effective_config.cspi_port,
            effective_config.i2c_num);
    return APP_OK;
}

int app_camera_liot_register(void)
{
    int ret;

    if (!s_camera_liot.configured) {
        app_log("camera liot register rejected: not configured");
        return APP_ERR_NOT_READY;
    }
    ret = app_camera_register_port(&s_camera_liot_port);
    if (ret != APP_OK) {
        app_log("camera liot register failed: %d", ret);
        return ret;
    }
    app_log("camera liot port registered");
    return APP_OK;
}

const app_camera_port_t *app_camera_liot_port(void)
{
    return &s_camera_liot_port;
}
