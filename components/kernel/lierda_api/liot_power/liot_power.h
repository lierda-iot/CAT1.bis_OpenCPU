/**
 * @file liot_power.h
 * @brief This file defines constants, enumerations, structures, 
 *        and function interfaces related to power management.
 *
 * @copyright Copyright (c) 2023 Lierda Technology Co., Ltd.
 * @date 2025-08-18
 * @version 1.0
 */

#ifndef _LIOT_POWER_H_
#define _LIOT_POWER_H_

#include "liot_api_common.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @def LIOT_LOCK_NAME_MAX_LEN
 * @brief The maximum length of the wake lock name.
 */
#define LIOT_LOCK_NAME_MAX_LEN 32
/**
 * @def LIOT_LOCK_MAX_CNT
 * @brief The maximum number of wake locks.
 */
#define LIOT_LOCK_MAX_CNT      10

/**
 * @def LIOT_UART_ERRCODE_BASE
 * @brief The base value of UART error codes.
 */
#define LIOT_UART_ERRCODE_BASE      (LIOT_COMPONENT_BSP_UART << 16)
/**
 * @def LIOT_UART_SUSPEND_MIN_DELAY
 * @brief The minimum delay time for UART suspension, unit: seconds.
 */
#define LIOT_UART_SUSPEND_MIN_DELAY 1 // unit:s
/**
 * @def LIOT_UART_SUSPEND_MAX_DELAY
 * @brief The maximum delay time for UART suspension, unit: seconds.
 */
#define LIOT_UART_SUSPEND_MAX_DELAY 255 // unit:s
/**
 * @def LIOT_UART_SUSPEND_DELAY_STD
 * @brief The standard delay time for UART suspension, unit: milliseconds.
 */
#define LIOT_UART_SUSPEND_DELAY_STD 5000 // unit:ms

/**
 * @def LIOT_POWER_ERRCODE_BASE
 * @brief The base value of power management error codes.
 */
#define LIOT_POWER_ERRCODE_BASE (LIOT_COMPONENT_PM << 16)

/**
 * @enum liot_wakeup_src_e
 * @brief Enumeration of wake-up sources.
 */
typedef enum
{
    LIOT_WAKEUP_FROM_POR = 0,  /**< Wake-up from power-on reset */
    LIOT_WAKEUP_FROM_RTC,      /**< Wake-up from RTC */
    LIOT_WAKEUP_FROM_PAD,      /**< Wake-up from pin */
    LIOT_WAKEUP_FROM_LPUART,   /**< Wake-up from low-power UART */
    LIOT_WAKEUP_FROM_LPUSB,    /**< Wake-up from low-power USB */
    LIOT_WAKEUP_FROM_PWRKEY,   /**< Wake-up from power key */
    LIOT_WAKEUP_FROM_CHARG,    /**< Wake-up from charging */
    LIOT_WAKEUP_FROM_T3412,    /**< Wake-up from T3412 timer */
} liot_wakeup_src_e;

/**
 * @enum liot_sleep_errcode_e
 * @brief Enumeration of sleep-related error codes.
 */
typedef enum
{
    LIOT_SLEEP_SUCCESS             = LIOT_SUCCESS,                      /**< Sleep operation succeeded */
    LIOT_SLEEP_INVALID_PARAM       = (LIOT_COMPONENT_PM_SLEEP << 16) | 1000, /**< Invalid parameter error */
    LIOT_SLEEP_LOCK_CREATE_FAIL    = (LIOT_COMPONENT_PM_SLEEP << 16) | 1001, /**< Wake lock creation failed */
    LIOT_SLEEP_LOCK_DELETE_FAIL    = (LIOT_COMPONENT_PM_SLEEP << 16) | 1002, /**< Wake lock deletion failed */
    LIOT_SLEEP_LOCK_LOCK_FAIL      = (LIOT_COMPONENT_PM_SLEEP << 16) | 1003, /**< Wake lock locking failed */
    LIOT_SLEEP_LOCK_UNLOCK_FAIL    = (LIOT_COMPONENT_PM_SLEEP << 16) | 1004, /**< Wake lock unlocking failed */
    LIOT_SLEEP_LOCK_AUTOSLEEP_FAIL = (LIOT_COMPONENT_PM_SLEEP << 16) | 1005, /**< Auto-sleep failed */
    LIOT_SLEEP_PARAM_SAVE_FAIL     = (LIOT_COMPONENT_PM_SLEEP << 16) | 1006, /**< Parameter save failed */
} liot_sleep_errcode_e;

/**
 * @enum liot_sleep_depth_e
 * @brief Enumeration of sleep depths.
 */
typedef enum
{
    LIOT_SLP_ACTIVE_STATE = 0, /**< Active state */
    LIOT_SLP_IDLE_STATE,       /**< Idle state */
    LIOT_SLP_SLP1_STATE,       /**< Sleep 1 state */
    LIOT_SLP_SLP2_STATE,       /**< Sleep 2 state */
    LIOT_SLP_HIB_STATE,        /**< Hibernate state */
    LIOT_SLP_STATE_MAX         /**< Maximum sleep state */
} liot_sleep_depth_e;

/**
 * @enum liot_sleep_flag_e
 * @brief Enumeration of sleep flags.
 */
typedef enum
{
    LIOT_NOT_ALLOW_SLEEP = 0, /**< Not allow sleep */
    LIOT_ALLOW_SLEEP,         /**< Allow sleep */
} liot_sleep_flag_e;

/**
 * @struct liot_wakelock_info_s
 * @brief Structure for wake lock information.
 */
typedef struct
{
    UINT8 votehdl;            /**< Handle */
    char lockname[LIOT_LOCK_NAME_MAX_LEN]; /**< Wake lock name */
} liot_wakelock_info_s;

/**
 * @enum liot_power_errcode_e
 * @brief Enumeration of power management error codes.
 */
typedef enum
{
    LIOT_POWER_POWD_SUCCESS  = LIOT_SUCCESS, /**< Shutdown succeeded */
    LIOT_POWER_RESET_SUCCESS = LIOT_POWER_POWD_SUCCESS, /**< Reset succeeded */

    LIOT_POWER_FUNC_INIT_ERR = -1, /**< Function initialization error */

    LIOT_POWER_CFW_CTRL_ERR = 1 | LIOT_POWER_ERRCODE_BASE, /**< CFW control error */
    LIOT_POWER_CFW_CTRL_RSP_ERR, /**< CFW control response error */
    LIOT_POWER_CFW_RESET_BUSY, /**< CFW reset busy */

    LIOT_POWER_SEMAPHORE_CREATE_ERR = 5 | LIOT_POWER_ERRCODE_BASE, /**< Semaphore creation error */
    LIOT_POWER_SEMAPHORE_TIMEOUT_ERR, /**< Semaphore timeout error */

    LIOT_POWER_POWD_EXECUTE_ERR = 11 | LIOT_POWER_ERRCODE_BASE, /**< Shutdown execution error */
    LIOT_POWER_POWD_INVALID_PARAM_ERR, /**< Shutdown invalid parameter error */

    LIOT_POWER_RESET_EXECUTE_ERR = 21 | LIOT_POWER_ERRCODE_BASE, /**< Reset execution error */
    LIOT_POWER_RESET_INVALID_PARAM_ERR, /**< Reset invalid parameter error */

    LIOT_POWER_UP_REASON_GET_ERR = 31 | LIOT_POWER_ERRCODE_BASE, /**< Get power-on reason error */
    LIOT_POWER_UP_REASON_MEM_NULL_ERR, /**< Power-on reason memory null error */

    LIOT_POWER_KEY_CB_NULL_ERR = 41 | LIOT_POWER_ERRCODE_BASE, /**< Power key callback null error */
    LIOT_POWER_KEY_STATUS_GET_ERR, /**< Get power key status error */
    LIOT_POWER_KEY_MEM_NULL_ERR, /**< Power key memory null error */
    LIOT_POWER_KEY_SHUTDOWN_TIME_SET_ERR, /**< Set power key shutdown time error */

    LIOT_POWER_USB_DETECT_INVALID_PARAM = 51 | LIOT_POWER_ERRCODE_BASE, /**< USB detection invalid parameter error */
    LIOT_POWER_USB_DETECT_SAVE_NV_ERR, /**< USB detection save NV error */

    LIOT_POWER_KEY_PULL_SET_ERR = 61 | LIOT_POWER_ERRCODE_BASE, /**< Set power key pull-up/down error */
    LIOT_POWER_KEY_PULL_INVALID_PARAM_ERR, /**< Power key pull-up/down invalid parameter error */

    LIOT_POWER_KEY_INIT_SET_ERR = 71 | LIOT_POWER_ERRCODE_BASE, /**< Set power key initialization error */
    LIOT_POWER_KEY_INIT_INVALID_PARAM_ERR, /**< Power key initialization invalid parameter error */

} liot_power_errcode_e;

/**
 * @struct liot_powd_info_s
 * @brief Structure for shutdown information.
 */
typedef struct
{
    UINT8 shutdown_mode;  /**< Shutdown mode */
    UINT32 timeout;       /**< Timeout */
} liot_powd_info_s;

/**
 * @enum liot_powd_mode_e
 * @brief Enumeration of shutdown modes.
 */
typedef enum
{
    LIOT_POWD_IMMDLY,   /**< Immediate shutdown, do not turn off CFUN */
    LIOT_POWD_NORMAL    /**< Shutdown after turning off CFUN */
} liot_powd_mode_e;

/**
 * @enum liot_reset_mode_e
 * @brief Enumeration of reset modes.
 */
typedef enum
{
    LIOT_RESET_QUICK,   /**< Quick reset */
    LIOT_RESET_NORMAL   /**< Normal reset */
} liot_reset_mode_e;

/**
 * @enum liot_pwrkey_status_e
 * @brief Enumeration of power key statuses.
 */
typedef enum
{
    LIOT_PWRKEY_FREERELEASE = 0, /**< Power key released */
    LIOT_PWRKEY_PRESSED      /**< Power key pressed */
} liot_pwrkey_status_e;

/**
 * @enum liot_pwrup_reason_e
 * @brief Enumeration of power-on reasons.
 */
typedef enum
{
    LIOT_PWRUP_UNKNOWN,    /**< Unknown reason */
    LIOT_PWRUP_PWRKEY,     /**< Power-on by power key */
    LIOT_PWRUP_PIN_RESET,  /**< Power-on by pin reset */
    LIOT_PWRUP_ALARM,      /**< Power-on by alarm */
    LIOT_PWRUP_CHARGE,     /**< Power-on by charging */
    LIOT_PWRUP_WDG,        /**< Power-on by watchdog */
    LIOT_PWRUP_PSM_WAKEUP, /**< Power-on by PSM wake-up */
    LIOT_PWRUP_PANIC       /**< Power-on by panic reset */
} liot_pwrup_reason_e;

/**
 * @enum liot_pwrkey_pullmode_e
 * @brief Enumeration of power key pull-up/down modes.
 */
typedef enum
{
    LIOT_PWRKEY_PULL_HANG, // Force floating
    LIOT_PWRKEY_PULL_UP    // Force pull-up
} liot_pwrkey_pullmode_e;

/**
 * @enum liot_pwrkey_statusmode_e
 * @brief Enumeration of power key status modes. Default values are defined in powerKeyFuncInit().
 */
typedef enum
{
    LIOT_PWRKEY_RELEASE = 0,   /**< Key released */
    LIOT_PWRKEY_PRESS,         /**< Key pressed */
    LIOT_PWRKEY_LONGPRESS,     /**< Key long-pressed, time set by liot_pwrkey_shutdown_time_set() */
    LIOT_PWRKEY_REPEAT,        /**< Key repeatedly pressed */
} liot_pwrkey_statusmode_e;

/**
 * @enum liot_pwrkey_workmode_e
 * @brief Enumeration of power key work modes.
 */
typedef enum
{
    LIOT_PWRKEY_PWRON_MODE = 0,                /**< Power-on mode */
    LIOT_PWRKEY_WAKEUP_LOWACTIVE_MODE,         /**< Wake-up mode triggered by low level */
    LIOT_PWRKEY_WAKEUP_HIGHACTIVE_MODE,        /**< Wake-up mode triggered by high level */
    LIOT_PWRKEY_UNKNOW_MODE,                   /**< Unknown mode */
} liot_pwrkey_workmode_e;

/**
 * @typedef liot_enter_sleep_callback
 * @brief Type definition for the callback function when entering sleep.
 */
typedef void (*liot_enter_sleep_callback)(void);

/**
 * @typedef liot_exit_sleep_callback
 * @brief Type definition for the callback function when exiting sleep.
 */
typedef void (*liot_exit_sleep_callback)(void);

/**
 * @typedef liot_pwrkey_callback
 * @brief Type definition for the power key callback function.
 * @param status The power key status mode.
 */
typedef void (*liot_pwrkey_callback)(liot_pwrkey_statusmode_e status);

/**
 * @typedef liot_psm_enter_callback
 * @brief Type definition for the PSM enter callback function.
 * @param ctx Pointer to the context.
 * @note Not implemented yet.
 */
typedef void (*liot_psm_enter_callback)(void *ctx);

/**
 * @typedef liot_usr_deepslp_timer_cb
 * @brief Type definition for the user deep sleep timer callback function.
 * @param ctx Pointer to the context.
 */
typedef void (*liot_usr_deepslp_timer_cb)(void *ctx);

/**
 * @brief Enable or disable the auto-sleep function.
 * @param sleep_flag Sleep flag, allow or not allow sleep.
 * @return Sleep-related error code.
 */
liot_sleep_errcode_e liot_autosleep_enable(liot_sleep_flag_e sleep_flag);

/**
 * @brief Register the callback function for entering sleep.
 * @param cb Pointer to the callback function for entering sleep.
 * @return Sleep-related error code.
 */
liot_sleep_errcode_e liot_sleep_register_cb(liot_enter_sleep_callback cb);

/**
 * @brief Register the callback function for exiting sleep.
 * @param cb Pointer to the callback function for exiting sleep.
 * @return Sleep-related error code.
 */
liot_sleep_errcode_e liot_wakeup_register_cb(liot_exit_sleep_callback cb);

/**
 * @brief Set the UART sleep delay time, the shortest interval between wake-up from sleep and going back to sleep.
 * @param times Delay time, unit: seconds.
 * @return UART-related error code.
 */
liot_sleep_errcode_e liot_uart_set_sleep_delay_time(UINT8 times); // second

/**
 * @brief Get the UART sleep delay time.
 * @param times Pointer to the variable storing the delay time, unit: seconds.
 * @return UART-related error code.
 */
liot_sleep_errcode_e liot_uart_get_sleep_delay_time(UINT8 *times);

/**
 * @brief Create a wake lock.
 * @param lock_name The name of the wake lock.
 * @param name_size The length of the wake lock name.
 * @return The file descriptor of the wake lock, negative value if failed.
 */
int liot_lpm_wakelock_create(char *lock_name, int name_size);

/**
 * @brief Lock the wake lock.
 * @param wakelock_fd The file descriptor of the wake lock.
 * @return Sleep-related error code.
 */
liot_sleep_errcode_e liot_lpm_wakelock_lock(int wakelock_fd);

/**
 * @brief Unlock the wake lock.
 * @param wakelock_fd The file descriptor of the wake lock.
 * @return Sleep-related error code.
 */
liot_sleep_errcode_e liot_lpm_wakelock_unlock(int wakelock_fd);

/**
 * @brief Delete the wake lock.
 * @param wakelock_fd The file descriptor of the wake lock.
 * @return Sleep-related error code.
 */
liot_sleep_errcode_e liot_lpm_wakelock_delete(int wakelock_fd);

/**
 * @brief Enable or disable the extended auto-sleep function.
 * @param sleep_flag Sleep flag, allow or not allow sleep.
 * @param no_data_time No data time.
 * @param punish_time Punishment time.
 * @return Sleep-related error code.
 */
liot_sleep_errcode_e liot_autosleepex_enable(liot_sleep_flag_e sleep_flag, UINT8 no_data_time, UINT16 punish_time);

/**
 * @brief Enable or disable the UART wake-up function.
 * @param uart_wakeup_enable Whether to enable the UART wake-up function.
 * @return Sleep-related error code.
 */
liot_sleep_errcode_e liot_set_uart_wakeup_enable(BOOL uart_wakeup_enable);

/**
 * @brief Enable or disable the DTR wake-up function.
 * @param dtr_wakeup_enable Whether to enable the DTR wake-up function.
 * @return Sleep-related error code.
 */
liot_sleep_errcode_e liot_set_dtr_wakeup_enable(BOOL dtr_wakeup_enable);

/**
 * @brief Enable or disable the wake-up function of the specified pin.
 * @param wkpad The APmuWakeupPad_e wake-up pin.
 * @param wakeup_enable Whether to enable the wake-up function.
 * @return Sleep-related error code.
 */
liot_sleep_errcode_e liot_set_wakeup_enable(uint8_t wkpad, BOOL wakeup_enable);

/**
 * @brief Set the sleep depth.
 * @param sleep_level The sleep depth.
 * @return Sleep-related error code.
 */
liot_sleep_errcode_e liot_set_sleep_depth(liot_sleep_depth_e sleep_level);

/**
 * @brief Get the wake-up source.
 * @return The wake-up source enumeration value.
 */
liot_wakeup_src_e liot_get_wakeup_src(void);

/**
 * @brief Get the last sleep depth.
 * @return The sleep depth enumeration value.
 */
liot_sleep_depth_e liot_get_last_slp_depth(void);

/**
 * @brief Start the deep sleep timer.
 * @param nMs Timer time, unit: milliseconds.
 * @return Sleep-related error code.
 */
liot_sleep_errcode_e liot_deepslp_timer_start(UINT32 nMs);

/**
 * @brief Start the extended deep sleep timer.
 * @param timer_id Timer ID.
 * @param nMs Timer time, unit: milliseconds.
 * @return Sleep-related error code.
 */
liot_sleep_errcode_e liot_deepslp_timerex_start(uint8_t timer_id, uint32_t nMs);

/**
 * @brief Check if the deep sleep timer is running.
 * @return true if it is running, false otherwise.
 */
bool liot_deepslp_timer_is_running();

/**
 * @brief Check if the extended deep sleep timer is running.
 * @param timer_id Timer ID.
 * @return true if it is running, false otherwise.
 */
bool liot_deepslp_timerex_is_running(uint8_t timer_id);

/**
 * @brief Delete the deep sleep timer.
 * @return Sleep-related error code.
 */
liot_sleep_errcode_e liot_deepslp_timer_del();

/**
 * @brief Delete the extended deep sleep timer.
 * @param timer_id Timer ID.
 * @return Sleep-related error code.
 */
liot_sleep_errcode_e liot_deepslp_timerex_del(uint8_t timer_id);

/**
 * @brief Register the callback function for the deep sleep timer.
 * @param cb Pointer to the callback function for the deep sleep timer.
 * @return Sleep-related error code.
 */
liot_sleep_errcode_e liot_deepslp_timer_register_cb(liot_usr_deepslp_timer_cb cb);

/**
 * @brief Register the callback function for the extended deep sleep timer.
 * @param timer_id Timer ID.
 * @param cb Pointer to the callback function for the deep sleep timer.
 * @return Sleep-related error code.
 */
liot_sleep_errcode_e liot_deepslp_timerex_register_cb(uint8_t timer_id, liot_usr_deepslp_timer_cb cb);

/**
 * @brief Register the callback function for entering PSM.
 * @param cb Pointer to the callback function for entering PSM.
 * @param ctx Pointer to the context.
 * @return Sleep-related error code.
 */
liot_sleep_errcode_e liot_psm_register_enter_cb(liot_psm_enter_callback cb, void *ctx);

/**
 * @brief Enable or disable the PSM sleep function.
 * @param nSim SIM card number.
 * @param psm_enable Whether to enable the PSM sleep function.
 * @param periodic_TAU_time Periodic TAU time.
 * @param active_time Active time.
 * @return Sleep-related error code.
 */
liot_sleep_errcode_e liot_psm_sleep_enable(UINT8 nSim,
                                           BOOL psm_enable,
                                           const char *periodic_TAU_time,
                                           const char *active_time);
/**
 * @brief Get the PSM result.
 * @param liot_t3412 Pointer to the variable storing the T3412 time.
 * @param liot_t3324 Pointer to the variable storing the T3324 time.
 * @param liot_ext3412 Pointer to the variable storing the extended T3412 time.
 * @return Sleep-related error code.
 */
liot_sleep_errcode_e liot_get_psm_result(UINT32 *liot_t3412, UINT32 *liot_t3324, UINT32 *liot_ext3412);

/**
 * @brief Check the sleep vote status.
 * @return Sleep-related error code.
 */
liot_sleep_errcode_e liot_check_sleep_vote_status(void);

/**
 * @brief Perform the shutdown operation.
 * @param powd_mode Shutdown mode.
 * @return Power management-related error code.
 */
liot_power_errcode_e liot_power_down(liot_powd_mode_e powd_mode);

/**
 * @brief Perform the reset operation.
 * @param reset_mode Reset mode.
 * @return Power management-related error code.
 */
liot_power_errcode_e liot_power_reset(liot_reset_mode_e reset_mode);

/**
 * @brief Get the power key status.
 * @param pwrkey_status Pointer to the variable storing the power key status.
 * @return Power management-related error code.
 */
liot_power_errcode_e liot_get_pwrkey_status(UINT8 *pwrkey_status);

/**
 * @brief Register the power key callback function.
 * @param pwrkey_cb Pointer to the power key callback function.
 * @return Power management-related error code.
 */
liot_power_errcode_e liot_pwrkey_callback_register(liot_pwrkey_callback pwrkey_cb);

/**
 * @brief Set the power key shutdown time.
 * @param shutdown_time Shutdown time.
 * @return Power management-related error code.
 */
liot_power_errcode_e liot_pwrkey_shutdown_time_set(UINT32 shutdown_time);

/**
 * @brief Get the power-on reason.
 * @param pwrup_reason Pointer to the variable storing the power-on reason.
 * @return Power management-related error code, 0 indicates success, others are error codes.
 */
liot_power_errcode_e liot_get_powerup_reason(UINT8 *pwrup_reason);

/**
 * @brief Set the power key pull-up or floating state.
 * @param pwrkey_pull Power key pull-up/down mode.
 * @return Power management-related error code, 0 indicates success, others are error codes.
 */
liot_power_errcode_e liot_set_pwrkey_pull(liot_pwrkey_pullmode_e pwrkey_pull);

/**
 * @brief Set whether to enable the power key initialization.
 * @param pwrkeyEn Enable parameter, true or false.
 * @param workMode Power key work mode.
 * @return Power management-related error code, 0 indicates success, others are error codes.
 */
liot_power_errcode_e liot_set_pwrkey_Init(bool pwrkeyEn, liot_pwrkey_workmode_e workMode);

#ifdef __cplusplus
} /*"C" */
#endif

#endif