#ifndef FRAMEWORK_TYPES_H
#define FRAMEWORK_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @file frameworkTypes.h
 * @brief 框架公共类型定义。
 */

/**
 * @brief 系统顶层状态。
 */
typedef enum {
    SYS_STATE_IDLE = 0,          /**< 空闲状态。 */
    SYS_STATE_ACTIVE,            /**< 活动态，表示当前存在主 job。 */
}sysState_E;

/**
 * @brief 框架事件标识。
 */
typedef enum {
    EVT_NONE = 0,                 /**< 无效事件。 */
    EVT_TIMER_EXPIRED,           /**< 定时器到期事件。 */
    EVT_STOP_CURRENT_JOB,        /**< 请求停止当前 job 的事件。 */
    /* Key events */
    EVT_KEY_POWER_ON,
    EVT_KEY_POWER_OFF,
    EVT_KEY_WAKEUP,
    EVT_KEY_VOL_UP,
    EVT_KEY_VOL_DOWN,
    /* Network events */
    EVT_NETWORK_READY,
    EVT_NETWORK_FAIL,
    EVT_SIM_ERROR,
    /* AI platform events */
    EVT_AI_CONNECT_REQ,
    EVT_AI_CONNECT_FAIL,
    EVT_AI_CONNECTED,
    EVT_AI_DISCONNECTED,
    EVT_AI_RESPONSE_DONE,
    EVT_AI_RESPONSE_TIMEOUT,
    /* Audio events */
    EVT_AUDIO_RECORD_DONE,
    EVT_AUDIO_PLAY_REQ,
    EVT_AUDIO_PLAY_START,
    EVT_AUDIO_PLAY_DONE,
    EVT_AUDIO_WAKEUP,
    /* Power events */
    EVT_LOW_POWER,
    EVT_CHARGE_INSERT,
    EVT_CHARGE_REMOVE,
    EVT_SYSTEM_SLEEP,
    /* LED events */
    EVT_LED_SET,
}eventId_E;

/**
 * @brief job 停止原因。
 */
typedef enum {
    STOP_REASON_COMPLETED = 0,       /**< 正常结束。 */
    STOP_REASON_ABORTED,             /**< 被中止。 */
    STOP_REASON_REPLACED,            /**< 被新 job 替换。 */
    STOP_REASON_ERROR,               /**< 发生错误后结束。 */
}jobStopReason_E;

/**
 * @brief policy 的处理结果。
 */
typedef enum {
    POLICY_IGNORE = 0,                  /**< 忽略当前事件。 */
    POLICY_START_JOB,                   /**< 启动新 job。 */
    POLICY_STOP_JOB,                    /**< 停止当前 job。 */
    POLICY_POST_CURR_JOB,               /**< 将事件投递给当前 job。 */
    POLICY_ABORT_CURR_AND_START_NEW,    /**< 中止当前 job 并启动新 job。 */
    POLICY_DELAY,                       /**< 延迟事件，等待当前 job 结束后再处理。 */
    POLICY_DIRECT_HANDLE,               /**< 直接执行处理函数。 */
}policyResult_E;

/**
 * @brief job 类型定义。
 */
typedef enum {
    JOB_TYPE_NONE = 0,                /**< 无效 job。 */
    JOB_TYPE_TALK,                    /**< 唤醒交互示例 job。 */
    JOB_TYPE_POWER,                   /**< 开机示例 job。 */
    JOB_TYPE_KEY,                     /**< 按键 job。 */
    JOB_TYPE_NETWORK,
    JOB_TYPE_AI_CONNECT,
    JOB_TYPE_MAX,                     /**< job 类型数量上界。 */
}jobType_E;

/**
 * @brief 软件定时器标识。
 */
typedef enum {
    TIMER_ID_NONE = 0,                  /**< 无效定时器。 */
    TIMER_ID_TALK_LISTEN_TIMEOUT,       /**< 唤醒后的聆听超时。 */
    TIMER_ID_TALK_RESPONSE_TIMEOUT,     /**< AI 回复超时。 */
    TIMER_ID_IDLE_WARNING,              /**< 空闲提示定时器。 */
    TIMER_ID_IDLE_POWEROFF,             /**< 空闲关机倒计时。 */
    TIMER_ID_MAX,                       /**< 定时器数量上界。 */
}timerId_E;

/**
 * @brief 框架内部使用的事件对象。
 */
typedef struct {
    eventId_E eventId;             /**< 事件标识。 */
    uint32_t arg1;                 /**< 通用参数 1。 */
    uint32_t arg2;                 /**< 通用参数 2。 */
    void *data;                    /**< 可选数据指针。 */
    uint32_t ownerJobId;           /**< 事件所属 job 标识，0 表示无归属。 */
}event_t;

struct job; // 前向声明
typedef struct job job_t; // 定义 job_t 类型

/**
 * @brief job 操作接口。
 */
typedef struct {
    int (*onInit)(void);                                              /**< job 注册时调用，初始化硬件。 */
    int (*onDeInit)(void);                                            /**< job 注销时调用，反初始化硬件。 */
    int (*onStart)(job_t *job, const event_t *triggerEvent);          /**< job 启动回调。 */
    int (*onEvent)(job_t *job, const event_t *event);                 /**< job 事件处理回调。 */
    int (*onStop)(job_t *job, jobStopReason_E reason);                /**< job 停止回调。 */
}jobOps_t;

/**
 * @brief 运行时 job 对象。
 */
struct job {
    uint32_t jobId;               /**< 运行时 job 标识。 */
    jobType_E type;               /**< job 类型。 */
    const jobOps_t *ops;          /**< 操作表。 */
    void *context;                /**< 私有上下文。 */
};

/**
 * @brief 直接处理函数类型。
 * @param event 输入事件。
 * @param currentJob 当前 job，没有则为空。
 * @return 处理结果，0 表示成功。
 */
typedef int (*directHandler_t)(const event_t *event, const job_t *currentJob);

/**
 * @brief policy 输出动作。
 */
typedef struct {
    jobType_E nextJobType;                 /**< 需要启动的 job 类型。 */
    directHandler_t directHandler;         /**< 直接执行处理函数。 */
}policyAction_t;

/**
 * @brief policy 描述符。
 */
typedef struct {
    const char *name;             /**< policy 名称。 */
    bool (*match)(const event_t *event, const job_t *currentJob, sysState_E sysState); /**< 粗匹配函数。 */
    policyResult_E (*decide)(const event_t *event, const job_t *currentJob, sysState_E sysState, policyAction_t *action); /**< 决策函数。 */
}policy_t;

/**
 * @brief 静态 job 描述符。
 */
typedef struct {
    jobType_E type;               /**< job 类型。 */
    const jobOps_t *ops;          /**< job 操作表。 */
    uint32_t contextSize;         /**< job 上下文大小。 */
    policy_t *jobPolicy;          /**< 当前 job 专属 policy。 */
}jobDesc_t;

#endif /* FRAMEWORK_TYPES_H */
