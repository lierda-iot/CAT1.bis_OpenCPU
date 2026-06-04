#ifndef __LIOT_SLEEP_H__
#define __LIOT_SLEEP_H__

#include "liot_power.h"

#define LIOT_DEEPSLP_TIMER_COUNT        6

typedef enum {
    LIOT_SLEEP_MODE_NORMAL,      // normal mode
    LIOT_SLEEP_MODE_LOW,         // sleep mode 
    LIOT_SLEEP_MODE_DEEP_LOW,    // deep sleep mode 
}liot_sleep_mode_type_e;


typedef struct {    
    liot_sleep_mode_type_e mode;    //sleep mode
} LiotSleepModeCfg_t;

typedef enum {
    LIOT_DEEPSLP_TIMER_ID0 = 0,		// num 0/1: 2 AONTimer, without flash storage, 2.5 hour in 100Hz
	LIOT_DEEPSLP_TIMER_ID1,
	LIOT_DEEPSLP_TIMER_ID2,
	LIOT_DEEPSLP_TIMER_ID3,
	LIOT_DEEPSLP_TIMER_ID4,
	LIOT_DEEPSLP_TIMER_ID5,
    LIOT_DEEPSLP_TIMER_MAX_NUM,      
}Liot_SleepTimerID_e;

/**
 * @brief Set the power consumption mode for the module.
 *
 * This function configures the module to enter the specified power consumption mode.
 * In [LIOT_SLEEP_MODE_LOW]mode, peripherals lose power but AON can maintain output.
 * In [LIOT_SLEEP_MODE_DEEP_LOW] mode, RAM loses power and the system behaves as if the code restarts upon wakeup.
 *
 * @param cfg Configuration for the power mode.
 */
liot_sleep_errcode_e Liot_SleepSetMode(LiotSleepModeCfg_t *cfg);
/**
 * @brief Prototype for the callback function triggered when a deep sleep timer expires.
 *
 * This callback function is invoked when the deep sleep timer times out.
 *
 * @param id Identifier of the timer that triggered the callback.
 */
typedef void (*DeepSlpTimerCb_Func)(Liot_SleepTimerID_e timeid);

typedef struct
{
    bool is_wakeup;
    DeepSlpTimerCb_Func timecb;
}Liot_SleepDeepTimeCfg_t;

/**
 * @brief Start a low-power timer with a specified ID, timeout, and callback function.
 *
 * Starts a low-power timer with the given ID, timeout duration, and callback function. 
 * The timer needs to be restarted after expiration.
 *
 * @param timeid ID of the timer to start.
 * @param timeout Timeout duration in milliseconds.
 * @param cb Callback function to invoke upon timer expiration.
 * @return Error code indicating success or failure of the operation.
 */
liot_sleep_errcode_e Liot_SleepTimerStart(Liot_SleepTimerID_e timeid, uint32_t timeout, DeepSlpTimerCb_Func cb);
/**
 * @brief Stop a running low-power timer.
 *
 * Deletes the low-power timer with the specified ID.
 *
 * @param timeid ID of the timer to stop.
 * @return Error code indicating success or failure of the operation.
 */
liot_sleep_errcode_e Liot_SleepTimerStop(Liot_SleepTimerID_e timeid);
/**
 * @brief Check the status of a low-power timer.
 *
 * Checks whether the low-power timer with the specified ID is currently running.
 *
 * @param timeid ID of the timer to check.
 * @return Boolean value indicating whether the timer is active.
 */
bool Liot_SleepTimerCheck(Liot_SleepTimerID_e timeid);

/**
 * @brief Get the ID of the low-power timer that woke up the system.
 *
 * This API is primarily used in LIOT_SLEEP_MODE_DEEP_LOW deep low-power mode.
 * In this mode, the system RAM loses power during sleep, and upon wakeup, the RAM is reinitialized,
 * effectively causing the program to restart from the beginning.
 * To allow the application layer to identify which timer woke up the module,
 * this function should be called to retrieve the ID of the wakeup-source timer.
 * This API is not required in other sleep modes, as timer callbacks are normally triggered and can notify the application directly.
 *
 * @return the ID of the low-power timer that woke up the system.
 * 
 */
Liot_SleepTimerID_e Liot_SleepTimerGetID(void);
#endif //__LIOT_SLEEP_H__