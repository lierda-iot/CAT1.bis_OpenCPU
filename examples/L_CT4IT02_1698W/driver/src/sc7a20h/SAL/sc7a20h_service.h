#ifndef SC7A20H_SERVICE_H
#define SC7A20H_SERVICE_H

/* ================= 结构体定义 ================= */
/**
 * @brief 摇晃功能配置结构体
 */
typedef struct {
    uint8_t enable;     // 功能开关 (0 关闭，1 开启)
    uint8_t level;      // 灵敏度阈值级别 (0-128)，实际阈值为 level * 32 mg
    uint8_t times;      // 摇晃次数阈值
    uint32_t timeout;   // 摇晃超时时间 (ms)
} sc7a20h_shake_config_t;

/**
 * @brief 点击功能配置结构体
 */
typedef struct {
    uint8_t enable;     // 功能开关 (0 关闭，1 开启)
    uint8_t level;      // 灵敏度阈值级别 (0-128)，实际阈值为 level * 32 mg
    uint8_t times;      // 点击次数 (1-3)
} sc7a20h_click_config_t;

/* 此处定义一个枚举类型，用于表示摇晃唤醒 摇晃打断事件 */
typedef enum {
    SC7A20H_SHAKE_WAKEUP = 0,
    SC7A20H_SHAKE_INTERRUPT,
    SC7A20H_SHAKE_EVENT_DEBUG,
    SC7A20H_SHAKE_EVENT_MAX
} sc7a20h_shake_event_E;


/* ================= 前向声明 ================= */
struct sc7a20h_dev_t;
typedef struct sc7a20h_dev_t sc7a20h_dev_t;

/**
 *  @brief 传感器服务接口定义
 */
void sc7a20h_sensor_init(void);


/**
 *  @brief 传感器服务释放接口定义
 */
void sc7a20h_sensor_release(void);

/**
 * @brief 获取传感器设备指针
 * @return 指向传感器设备结构体的指针
 */
sc7a20h_dev_t *get_sensor_device(void);

/**
 * @brief 设置摇晃功能状态
 * @param enable 摇晃功能开关状态 (0 关闭，1 开启)
 */
void sc7a20h_sensor_set_shake_state(bool enable);

/**
 * @brief 设置摇晃功能配置
 * @param event 摇晃事件枚举值
 * @param config 摇晃配置结构体
 */
void sc7a20h_sensor_set_shake_config(sc7a20h_shake_event_E event, const sc7a20h_shake_config_t *config);

/**
 * @brief 获取摇晃功能配置
 * @param event 摇晃事件枚举值
 * @param config 摇晃配置结构体指针，用于存储配置
 */
void sc7a20h_sensor_get_shake_config(sc7a20h_shake_event_E event, sc7a20h_shake_config_t *config);

/**
 * @brief 设置摇晃事件
 * @param event 摇晃事件枚举值
 */
void sc7a20h_sensor_set_shake_event(sc7a20h_shake_event_E event);

/**
 * @brief 设置点击功能状态
 * @param enable 点击功能开关状态 (0 关闭，1 开启)
 */
void sc7a20h_sensor_set_click_state(bool enable);

/**
 * @brief 设置点击功能配置
 * @param config 点击配置结构体
 */
void sc7a20h_sensor_set_click_config(const sc7a20h_click_config_t *config);

/**
 * @brief 获取点击功能配置
 * @param config 点击配置结构体指针，用于存储配置
 */
void sc7a20h_sensor_get_click_config(sc7a20h_click_config_t *config);

#endif /* SC7A20H_SERVICE_H */
