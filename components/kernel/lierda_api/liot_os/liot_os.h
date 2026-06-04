/**
 * @File Name: liot_os.h
 * @brief
 * @Author : chenly email:ciot_iot_support@lierda.com
 * @Version : 1.0
 * @Creat Date : 2023-09-07
 *
 * @copyright Copyright (c) 2023 Lierda Science & Technology Group Co., Ltd.
 *
 */
#ifndef _LIOT_OS_H_
#define _LIOT_OS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "liot_osi_def.h"
#include "liot_type.h"

#define LIOT_APP_TASK_PRIORITY APP_PRIORITY_NORMAL

typedef int LiotOSStatus_t;
typedef void *liot_StaticTask_t;
typedef void *liot_task_t;
typedef void *liot_sem_t;
typedef void *liot_mutex_t;
typedef void *liot_queue_t;
typedef void *liot_timer_t;
typedef void *liot_flag_t;

// typedef int    LiotOSStatus_t;

#define osFlagsWaitAny 0x00000000U ///< Wait for any flag (default).
#define osFlagsWaitAll 0x00000001U ///< Wait for all flags.
#define osFlagsNoClear 0x00000002U ///< Do not clear flags which have been specified to wait for.

/*!
 * @brief Event flag waiting options.
 *
 * @details This enumeration defines the different ways a task can wait for event flags to be set.
 *          These options determine whether the task waits for any flag, all specified flags,
 *          or waits without clearing the flags after they are processed.
 */
typedef enum
{
    LIOT_FLAG_WAIT_ANY = 0, ///< Wait for any single bit in the event flag to be set (logical OR).
    LIOT_FLAG_WAIT_ALL,     ///< Wait for all specified bits in the event flag to be set (logical AND).
    LIOT_FLAG_NO_CLEAR,     ///< Wait for the event flags but do not clear them after the wait condition is met.
} liot_flag_wait_e;

/*!
 * @brief Event flag operation types.
 *
 * @details This enumeration defines the operations that can be performed when waiting for or setting event flags.
 *          It allows specifying whether to wait for all bits (AND), wait for any bit (OR),
 *          and whether the flags should be cleared after processing.
 */
typedef enum
{
    LIOT_FLAG_AND = 5, ///< Wait for all bits in the input event to be set. Do not clear the event flag after the event
                       ///< is processed.
    LIOT_FLAG_AND_CLEAR = 6, ///< Wait for all bits in the input event to be set. Clear the event flag after the event
                             ///< is processed.
    LIOT_FLAG_OR = 7, ///< Wait for any bit in the input event to be set. Do not clear the event flag after the task is
                      ///< finished processing the event.
    LIOT_FLAG_OR_CLEAR = 8 ///< Wait for any bit in the input event to be set. After the task is finished processing
                           ///< the event, clear the event flag.
} liot_flag_op_e;

/*!
 * @brief Wait timeout options.
 *
 * @details This enumeration defines the possible timeout values used when waiting on synchronization primitives
 *          such as semaphores, mutexes, and event flags. It supports infinite wait, no wait, or a specific time-out.
 */
typedef enum
{
    LIOT_WAIT_FOREVER = 0xFFFFFFFFUL,
    LIOT_NO_WAIT      = 0
} liot_wait_e;

/*!
 * @brief Timer types supported by the system.
 *
 * @details This enumeration defines the two types of timers that can be created: one-shot and periodic.
 *          - One-shot timer triggers once after the specified duration.
 *          - Periodic timer triggers repeatedly at fixed intervals.
 */
typedef enum
{
    LIOT_TimerOnce     = 0, ///< One-shot timer.
    LIOT_TimerPeriodic = 1  ///< Repeating timer.
} liot_timertype_e;

/*!
 * @brief Task states in the RTOS.
 *
 * @details This enumeration represents the various states a task can be in during its lifecycle.
 *          These states are useful for debugging and monitoring task behavior within the system.
 */
typedef enum
{
    LIOT_Running = 0, /* A task is querying the state of itself, so must be running. */
    LIOT_Ready,       /* The task being queried is in a read or pending ready list. */
    LIOT_Blocked,     /* The task being queried is in the Blocked state. */
    LIOT_Suspended,   /* The task being queried is in the Suspended state, or is in the Blocked state with an infinite
                         time out. */
    LIOT_Deleted,     /* The task being queried has been deleted, but its TCB has not yet been freed. */
    LIOT_Invalid      /* Used as an 'invalid state' value. */
} liot_task_state_e;

typedef struct
{
    liot_task_t xHandle;    /* The handle of the task to which the rest of the information in the structure relates. */
    const char *pcTaskName; /* A pointer to the task's name.  This value will be invalid if the task was deleted since
                               the structure was populated! */
    liot_task_state_e eCurrentState; /* The state in which the task existed when the structure was populated. */
    unsigned long uxCurrentPriority; /* The priority at which the task was running (may be inherited) when the structure
                                        was populated. */
    uint16 usStackHighWaterMark; /* The minimum amount of stack space that has remained for the task since the task was
                                    created.  The closer this value is to zero the closer the task has come to
                                    overflowing its stack. */
} liot_task_status_s;

/*!
 * @brief Time value structure for representing time in seconds and microseconds.
 *
 * @details This structure is used to store a time value specified in seconds and microseconds.
 *          It can be used for time-related operations, such as delays or timestamps.
 */
typedef struct
{
    uint32 sec;   ///< Seconds component of the time value.
    uint32 usec;  ///< Microseconds component of the time value.
} liot_timeval_s;

/*!
 * @brief CPU usage information structure.
 *
 * @details This structure is used to store CPU usage statistics, including the number of idle ticks,
 *          system ticks, CPU utilization percentage, and timestamps for measuring idle periods.
 */
typedef struct
{
    uint total_idle_tick;   ///< Total number of ticks the CPU was in an idle state.
    uint sys_tick_old;      ///< Previous system tick count before entering idle measurement.
    uint cpu_using;         ///< CPU utilization percentage calculated based on active and idle time.
    uint idle_in_tick;      ///< System tick count when the CPU entered the idle state.
    uint idle_out_tick;     ///< System tick count when the CPU exited the idle state.
} liot_cpu_using_info_s;

#define liot_is_timer_init(timer) ((*timer != NULL) ? (true) : (false))

/*========================================================================
 *	function Definition
 *========================================================================*/

/*!
 * @brief Creates a new task and adds it to the list of tasks that are ready to run.
 *
 * @details This function allocates memory for the task control block (TCB) and its stack,
 *          initializes the task with the provided parameters, and schedules it for execution.
 *          The task will start running when the scheduler selects it based on its priority
 *          and state.
 *
 * @param taskRef       [out] Pointer to a variable that will hold the reference to the created task.
 * @param stackSize     [in]  The size of the stack (in bytes) allocated for this task.
 * @param priority      [in]  The priority at which the task will execute. Valid range: 1 ~ 30.
 * @param taskName      [in]  A descriptive name for the task. Used for debugging purposes.
 * @param taskStart     [in]  The function that implements the task behavior. This function should never return.
 * @param argv          [in]  Argument passed to the task entry function when it starts executing.
 * @param ...           [in]  Optional additional arguments (implementation-specific).
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Task was successfully created.
 *         - Other: Error code indicating failure to create the task.
 */ 
extern LiotOSStatus_t liot_rtos_task_create(liot_task_t *taskRef,      /* OS task reference */
                                            uint32 stackSize,          /* number of bytes in task stack area */
                                            uint8 priority,            /* task priority, 1 ~ 30 */
                                            char *taskName,            /* task name */
                                            void (*taskStart)(void *), /* pointer to task entry point */
                                            void *argv,                /* task entry argument pointer */
                                            ...);

/*!
 * @brief Creates a new task using statically allocated memory and adds it to the list of ready tasks.
 *
 * @details This function uses user-provided memory for both the task stack and the task control block (TCB),
 *          allowing better control over memory allocation in systems where dynamic memory allocation is not desired.
 *          The task will be initialized with the given parameters and scheduled for execution based on its priority.
 *
 * @param taskRef       [out] Pointer to a variable that will hold the reference to the created task.
 * @param stackSize     [in]  The size of the provided stack (in bytes).
 * @param priority      [in]  The priority at which the task will execute. Valid range: 1 ~ 30.
 * @param taskName      [in]  A descriptive name for the task. Used for debugging purposes.
 * @param taskStart     [in]  The function that implements the task behavior. This function should never return.
 * @param stackMem      [in]  Pointer to the pre-allocated memory to be used as the task’s runtime stack.
 * @param StaticTask    [out] Pointer to a `StaticTask_t` structure to be used as the task control block (TCB).
 * @param argv          [in]  Argument passed to the task entry function when it starts executing.
 * @param ...           [in]  Optional additional arguments (implementation-specific).
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Task was successfully created.
 *         - Other: Error code indicating failure to create the task.
 */
extern LiotOSStatus_t liot_rtos_task_create_static(liot_task_t *taskRef,
                                                   uint32 stackSize,
                                                   uint8 priority,
                                                   char *taskName,
                                                   void (*taskStart)(void *),
                                                   void *stackMem,
                                                   void *StaticTask,
                                                   void *argv,
                                                   ...);

/*!
 * @brief Creates a new task with default settings and adds it to the list of tasks that are ready to run.
 *
 * @details This function allocates memory for the task control block (TCB) and its stack,
 *          initializes the task with default parameters such as priority, name, and stack size,
 *          and schedules it for execution based on its priority. It is typically used when
 *          a simplified task creation interface is desired.
 *
 * @param taskRef       [out] Pointer to a variable that will hold the reference to the created task.
 * @param stackSize     [in]  The size of the stack (in bytes) allocated for this task.
 * @param priority      [in]  The priority at which the task will execute. Valid range: 1 ~ 30.
 * @param taskName      [in]  A descriptive name for the task. Used for debugging purposes.
 * @param taskStart     [in]  The function that implements the task behavior. This function should never return.
 * @param argv          [in]  Argument passed to the task entry function when it starts executing.
 * @param ...           [in]  Optional additional arguments (implementation-specific).
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Task was successfully created.
 *         - Other: Error code indicating failure to create the task.
 */
extern LiotOSStatus_t liot_rtos_task_create_default(liot_task_t *taskRef,      /* OS task reference */
                                                    uint32 stackSize,          /* number of bytes in task stack area */
                                                    uint8 priority,            /* task priority */
                                                    char *taskName,            /* task name */
                                                    void (*taskStart)(void *), /* pointer to task entry point */
                                                    void *argv,                /* task entry argument pointer */
                                                    ...);


/*!
 * @brief Deletes a task and frees its associated resources.
 *
 * @details This function removes the specified task from the RTOS scheduler,
 *          frees the memory allocated for the task's stack and TCB (if dynamically allocated),
 *          and ensures that the task no longer executes.
 *          If the task is currently running, it will be deleted after it stops executing.
 *
 * @param taskRef [in] OS task reference to the task to be deleted.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Task was successfully deleted.
 *         - Other: Error code indicating failure to delete the task.
 */
extern LiotOSStatus_t liot_rtos_task_delete(liot_task_t taskRef);

/*!
 * @brief Suspends the execution of a specified task.
 *
 * @details This function puts the specified task into the 'Suspended' state,
 *          preventing it from being scheduled until it is resumed using `liot_rtos_task_resume`.
 *          A suspended task retains all its state information and can be resumed at any time.
 *
 * @param taskRef [in] OS task reference to the task to be suspended.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Task was successfully suspended.
 *         - Other: Error code indicating failure to suspend the task.
 */
extern LiotOSStatus_t liot_rtos_task_suspend(liot_task_t taskRef);

/*!
 * @brief Resumes the execution of a previously suspended task.
 *
 * @details This function moves the specified task from the 'Suspended' state back to the 'Ready' state,
 *          allowing it to be scheduled and executed by the RTOS scheduler.
 *
 * @param taskRef [in] OS task reference to the task to be resumed.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Task was successfully resumed.
 *         - Other: Error code indicating failure to resume the task.
 */
extern LiotOSStatus_t liot_rtos_task_resume(liot_task_t taskRef);

/*!
 * @brief Forces the current task to yield its execution and allow other tasks of the same priority to run.
 *
 * @details This function causes the calling task to voluntarily give up the CPU,
 *          allowing the RTOS scheduler to select another ready task of the same or higher priority
 *          to execute. It is typically used in scenarios where a task wants to ensure fair CPU time sharing
 *          among tasks at the same priority level.
 *
 * @note This function has no effect if there are no other ready tasks with equal or higher priority.
 *
 * @return None
 */
extern void liot_rtos_task_yield(void);

/*!
 * @brief Gets a reference to the currently running task.
 *
 * @details This function returns a handle to the task that is currently executing.
 *          It can be used for further task management operations such as suspending,
 *          resuming, or querying the status of the current task.
 *
 * @param taskRef [out] Pointer to store the reference of the current task.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Successfully retrieved the current task reference.
 *         - Other: Error code indicating failure.
 */
extern LiotOSStatus_t liot_rtos_task_get_current_ref(liot_task_t *taskRef);

/*!
 * @brief Changes the priority of a specified task.
 *
 * @details This function modifies the scheduling priority of the given task.
 *          The new priority must be within the valid range (1 ~ 30).
 *          If the task is in the Ready state, it will be reinserted into the ready list at its new priority.
 *
 * @param taskRef      [in]  OS task reference.
 * @param new_priority [in]  New priority value (valid range: 1 ~ 30).
 * @param old_priority [out] Optional pointer to store the previous priority value.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Priority was successfully changed.
 *         - Other: Error code indicating failure.
 */
extern LiotOSStatus_t liot_rtos_task_change_priority(liot_task_t taskRef, uint8 new_priority, uint8 *old_priority);

/*!
 * @brief Retrieves the current status information of a specified task.
 *
 * @details This function fills a `liot_task_status_s` structure with detailed runtime
 *          information about the specified task, including its current state,
 *          priority, and stack usage.
 *
 * @param task_ref [in]  OS task reference.
 * @param status   [out] Pointer to a `liot_task_status_s` structure to receive task status.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Task status was successfully retrieved.
 *         - Other: Error code indicating failure.
 */
extern LiotOSStatus_t liot_rtos_task_get_status(liot_task_t task_ref, liot_task_status_s *status);

/*!
 * @brief Delays the execution of the current task for the specified number of milliseconds.
 *
 * @details This function puts the calling task into the Blocked state for the given time,
 *          allowing other tasks to run during this period.
 *
 * @param ms [in] Delay time in milliseconds.
 *
 * @return None
 */
extern void liot_rtos_task_sleep_ms(uint32 ms);

/*!
 * @brief Delays the execution of the current task for the specified number of seconds.
 *
 * @details This function puts the calling task into the Blocked state for the given time,
 *          allowing other tasks to run during this period.
 *
 * @param s [in] Delay time in seconds.
 *
 * @return None
 */
extern void liot_rtos_task_sleep_s(uint32 s);

/*!
 * @brief Gets the unused stack space of a specified task.
 *
 * @details This function calculates the amount of free stack space remaining for the given task.
 *          A low value may indicate a potential stack overflow risk.
 *
 * @param task_ref [in] OS task reference.
 *
 * @return uint32_t
 *         - The unused stack space in bytes.
 */
extern uint32_t liot_rtos_task_get_stack_space(liot_task_t task_ref);

/*!
 * @brief Enters a critical section by disabling interrupts.
 *
 * @details This function disables all interrupts to protect a critical section of code
 *          from being preempted by interrupt service routines (ISRs). It should always
 *          be paired with a call to `liot_rtos_exit_critical`.
 *
 * @note This function is intended for use in normal task context, not within ISRs.
 *
 * @return None
 */
extern void liot_rtos_enter_critical(void);

/*!
 * @brief Enters a critical section from an interrupt service routine (ISR).
 *
 * @details This function disables interrupts and returns the previous interrupt mask state.
 *          It is safe to use within ISR contexts. Must be paired with a corresponding
 *          call to `liot_rtos_exit_critical_from_isr` with the saved mask value.
 *
 * @return uint32_t
 *         The interrupt mask status before entering the critical section.
 */
extern uint32_t liot_rtos_enter_critical_from_isr(void);

/*!
 * @brief Exits a previously entered critical section by re-enabling interrupts.
 *
 * @details This function restores the interrupt enable state after a matching call to
 *          `liot_rtos_enter_critical`. Must not be called from an ISR.
 *
 * @note This function must be used in pairs with `liot_rtos_enter_critical`.
 *
 * @return None
 */
extern void liot_rtos_exit_critical(void);

/*!
 * @brief Exits a previously entered critical section from an ISR context.
 *
 * @details This function restores the interrupt enable state using the provided mask,
 *          which was obtained from a prior call to `liot_rtos_enter_critical_from_isr`.
 *
 * @param isrm [in] The interrupt mask status obtained from `liot_rtos_enter_critical_from_isr`.
 *
 * @return None
 */
extern void liot_rtos_exit_critical_from_isr(uint32_t isrm);

/*!
 * @brief Obtains the total running time of the system since startup.
 *
 * @details This function returns the duration (in seconds) that the system has been running.
 *          It is useful for measuring uptime or timing events in a system-level context.
 *
 * @return uint32_t
 *         - System running time in seconds.
 */
extern uint32_t liot_rtos_get_running_time(void);

/*!
 * @brief Creates a new binary semaphore and initializes it with the specified count.
 *
 * @details This function allocates and initializes a binary semaphore.
 *          The semaphore can be used to synchronize tasks or between tasks and ISRs.
 *
 * @param semaRef      [out] Pointer to store the reference of the created semaphore.
 * @param initialCount [in]  Initial count of the semaphore. Must be <= max count (default for binary: 1).
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Semaphore was successfully created.
 *         - Other: Error code indicating failure.
 */
extern LiotOSStatus_t liot_rtos_semaphore_create(liot_sem_t *semaRef, uint32 initialCount);

/*!
 * @brief Creates a new counting semaphore with customizable max count.
 *
 * @details This function allows creation of a counting semaphore with user-defined maximum value,
 *          enabling more flexible synchronization mechanisms than a binary semaphore.
 *
 * @param semaRef      [out] Pointer to store the reference of the created semaphore.
 * @param initialCount [in]  Initial count of the semaphore.
 * @param max_cnt      [in]  Maximum allowed count of the semaphore.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Semaphore was successfully created.
 *         - Other: Error code indicating failure.
 */
extern LiotOSStatus_t liot_rtos_semaphore_create_ex(liot_sem_t *semaRef, uint32 initialCount, uint32 max_cnt);

/*!
 * @brief Waits for a semaphore to become available.
 *
 * @details This function blocks the calling task until the semaphore becomes available
 *          or the timeout expires. If the semaphore is already available, it is taken immediately.
 *
 * @param semaRef [in] OS semaphore reference.
 * @param timeout [in] Timeout value in ticks. Use `LIOT_WAIT_FOREVER` to wait indefinitely.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Semaphore was successfully acquired.
 *         - Other: Error code indicating failure (e.g., timeout or invalid handle).
 */
extern LiotOSStatus_t liot_rtos_semaphore_wait(liot_sem_t semaRef, uint32 timeout);

/*!
 * @brief Releases a semaphore, incrementing its count.
 *
 * @details This function increases the semaphore's count, allowing another waiting task to proceed.
 *          It can be safely called from both task and ISR contexts.
 *
 * @param semaRef [in] OS semaphore reference.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Semaphore was successfully released.
 *         - Other: Error code indicating failure.
 */
extern LiotOSStatus_t liot_rtos_semaphore_release(liot_sem_t semaRef);

/*!
 * @brief Gets the current count of a semaphore.
 *
 * @details This function retrieves the current number of available resources represented by the semaphore.
 *
 * @param semaRef [in]  OS semaphore reference.
 * @param cntPtr  [out] Pointer to store the current semaphore count.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Count was successfully retrieved.
 *         - Other: Error code indicating failure.
 */
extern LiotOSStatus_t liot_rtos_semaphore_get_cnt(liot_sem_t semaRef, uint32 *cntPtr);

/*!
 * @brief Deletes a semaphore and frees associated resources.
 *
 * @details This function removes the semaphore from the system and releases any memory allocated for it.
 *          After deletion, the semaphore reference becomes invalid.
 *
 * @param semaRef [in] OS semaphore reference.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Semaphore was successfully deleted.
 *         - Other: Error code indicating failure.
 */
extern LiotOSStatus_t liot_rtos_semaphore_delete(liot_sem_t semaRef);

/*!
 * @brief Creates and initializes a new mutex.
 *
 * @details This function allocates and initializes a mutex that can be used for mutual exclusion
 *          between tasks or between tasks and ISRs (if supported by the underlying RTOS).
 *          The mutex is created in an unlocked state.
 *
 * @param mutexRef [out] Pointer to store the reference of the created mutex.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Mutex was successfully created.
 *         - Other: Error code indicating failure.
 */
extern LiotOSStatus_t liot_rtos_mutex_create(liot_mutex_t *mutexRef);

/*!
 * @brief Acquires ownership of the specified mutex with a timeout.
 *
 * @details This function blocks the calling task until the mutex becomes available,
 *          or the timeout expires. If the mutex is already held by another task,
 *          the calling task will wait.
 *
 * @param mutexRef [in] OS mutex reference.
 * @param timeout  [in] Timeout value in ticks. Use `LIOT_WAIT_FOREVER` to wait indefinitely.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Mutex was successfully acquired.
 *         - Other: Error code indicating failure (e.g., timeout or invalid handle).
 */
extern LiotOSStatus_t liot_rtos_mutex_lock(liot_mutex_t mutexRef, uint32 timeout);

/*!
 * @brief Attempts to acquire ownership of the specified mutex without blocking.
 *
 * @details This function tries to lock the mutex immediately. If the mutex is not available,
 *          it returns immediately with an error instead of waiting.
 *
 * @param mutexRef [in] OS mutex reference.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Mutex was successfully acquired.
 *         - Other: Error code indicating that the mutex is not available.
 */
extern LiotOSStatus_t liot_rtos_mutex_try_lock(liot_mutex_t mutexRef);

/*!
 * @brief Releases the ownership of the specified mutex.
 *
 * @details This function unlocks the mutex so that other tasks can acquire it.
 *          It must only be called by the task that currently owns the mutex.
 *
 * @param mutexRef [in] OS mutex reference.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Mutex was successfully released.
 *         - Other: Error code indicating failure (e.g., not owned by caller).
 */
extern LiotOSStatus_t liot_rtos_mutex_unlock(liot_mutex_t mutexRef);

/*!
 * @brief Deletes a mutex and frees associated resources.
 *
 * @details This function removes the mutex from the system and releases any memory allocated for it.
 *          After deletion, the mutex reference becomes invalid and should no longer be used.
 *
 * @param mutexRef [in] OS mutex reference.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Mutex was successfully deleted.
 *         - Other: Error code indicating failure.
 */
extern LiotOSStatus_t liot_rtos_mutex_delete(liot_mutex_t mutexRef);

/*!
 * @brief Creates and initializes a new message queue.
 *
 * @details This function allocates memory for a message queue with fixed-size messages.
 *          The queue can be used to pass data between tasks or from ISRs to tasks.
 *
 * @param msgQRef   [out] Pointer to store the reference of the created message queue.
 * @param maxSize   [in]  Maximum size (in bytes) of each message that can be stored in the queue.
 * @param maxNumber [in]  Maximum number of messages that the queue can hold.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Queue was successfully created.
 *         - Other: Error code indicating failure.
 */
extern LiotOSStatus_t liot_rtos_queue_create(liot_queue_t *msgQRef, uint32 maxSize, uint32 maxNumber);

/*!
 * @brief Statically creates and initializes a message queue using user-provided memory.
 *
 * @details This function allows creation of a queue without dynamic memory allocation.
 *          It is useful in systems where static memory management is preferred.
 *
 * @param msgQRef    [out] Pointer to store the reference of the created message queue.
 * @param maxSize    [in]  Maximum size (in bytes) of each message.
 * @param maxNumber  [in]  Maximum number of messages the queue can hold.
 * @param queueName  [in]  Name of the queue (for debugging).
 * @param queueSize  [in]  Size of the provided buffer in bytes (must be >= maxSize * maxNumber).
 * @param queueMem   [in]  Pre-allocated memory buffer for queue storage.
 * @param StaticQueue[in]  Pre-allocated memory for the queue control block.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Queue was successfully created.
 *         - Other: Error code indicating failure.
 */
extern LiotOSStatus_t liot_rtos_queue_create_static(liot_queue_t *msgQRef,
                                                    uint32 maxSize,
                                                    uint32 maxNumber,
                                                    char *queueName,
                                                    uint32 queueSize,
                                                    void *queueMem,
                                                    void *StaticQueue);

/*!
 * @brief Waits for a message to become available in the queue.
 *
 * @details This function blocks the calling task until a message is received or the timeout expires.
 *
 * @param msgQRef [in]  Message queue reference.
 * @param recvMsg [out] Pointer to buffer where the received message will be copied.
 * @param size    [in]  Size of the message buffer.
 * @param timeout [in]  Timeout value in ticks. Use `LIOT_WAIT_FOREVER` to wait indefinitely.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Message was successfully received.
 *         - Other: Error code indicating failure (e.g., timeout or invalid handle).
 */
extern LiotOSStatus_t liot_rtos_queue_wait(liot_queue_t msgQRef, /* message queue reference */
                                           uint8 *recvMsg,       /* pointer to the message received */
                                           uint32 size,          /* size of the message */
                                           uint32 timeout        /* LIOT_WAIT_FOREVER, LIOT_NO_WAIT, or timeout */
);

/*!
 * @brief Sends a message to the queue and optionally waits if the queue is full.
 *
 * @details This function sends a message to the queue from a task context.
 *          If the queue is full, it can optionally wait until space becomes available.
 *
 * @param msgQRef [in] Message queue reference.
 * @param size    [in] Size of the message.
 * @param msgPtr  [in] Pointer to the message data to send.
 * @param timeout [in] Timeout value in ticks. Use `LIOT_WAIT_FOREVER` to wait indefinitely.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Message was successfully sent.
 *         - Other: Error code indicating failure.
 */
extern LiotOSStatus_t liot_rtos_queue_release(liot_queue_t msgQRef, /* message queue reference */
                                              uint32 size,          /* size of the message */
                                              uint8 *msgPtr,        /* start address of the data to be sent */
                                              uint32 timeout        /* LIOT_WAIT_FOREVER, LIOT_NO_WAIT, or timeout */
);

/*!
 * @brief Sends a message to the queue from an interrupt service routine (ISR).
 *
 * @details This function is safe to call from an ISR context.
 *          It does not block and cannot wait for space in the queue.
 *
 * @param msgQRef [in] Message queue reference.
 * @param size    [in] Size of the message.
 * @param msgPtr  [in] Pointer to the message data to send.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Message was successfully sent.
 *         - Other: Error code indicating failure (e.g., queue full).
 */
extern LiotOSStatus_t liot_rtos_queue_release_isr(liot_queue_t msgQRef, uint32 size, uint8 *msgPtr);

/*!
 * @brief Gets the current number of messages in the queue.
 *
 * @details This function retrieves the number of messages currently present in the queue.
 *
 * @param msgQRef [in]  Message queue reference.
 * @param cntPtr  [out] Pointer to store the current message count.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Count was successfully retrieved.
 *         - Other: Error code indicating failure.
 */
extern LiotOSStatus_t liot_rtos_queue_get_cnt(liot_queue_t msgQRef, uint32 *cntPtr);

/*!
 * @brief Deletes a message queue and frees associated resources.
 *
 * @details This function removes the queue from the system and releases any memory allocated for it.
 *          After deletion, the queue reference becomes invalid and should no longer be used.
 *
 * @param msgQRef [in] Message queue reference.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Queue was successfully deleted.
 *         - Other: Error code indicating failure.
 */
extern LiotOSStatus_t liot_rtos_queue_delete(liot_queue_t msgQRef);

/*!
 * @brief Resets the message queue to its initial empty state.
 *
 * @details This function removes all messages currently in the queue and resets the queue's internal state.
 *
 * @param msgQRef [in] Message queue reference.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Queue was successfully reset.
 *         - Other: Error code indicating failure.
 */
extern LiotOSStatus_t liot_rtos_queue_reset(liot_queue_t msgQRef);

/*!
 * @brief Gets the number of available slots in the message queue.
 *
 * @details This function returns how many more messages can be added to the queue before it becomes full.
 *
 * @param msgQRef [in] Message queue reference.
 *
 * @return uint32_t
 *         - Number of available slots in the queue.
 */
extern uint32_t liot_rtos_queue_get_space(liot_queue_t msgQRef);

/*!
 * @brief Creates and initializes a new timer.
 *
 * @details This function allocates and initializes a timer object that can be used to generate callbacks
 *          after a specified time interval. Timers can be one-shot or periodic.
 *
 * @param timerRef       [out] Pointer to store the reference of the created timer.
 * @param cyclicalEn     [in]  Type of timer: one-shot (`LIOT_TimerOnce`) or periodic (`LIOT_TimerPeriodic`).
 * @param callBackRoutine[in]  Callback function that will be invoked when the timer expires.
 * @param timerArgc      [in]  Argument to pass to the callback function.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Timer was successfully created.
 *         - Other: Error code indicating failure.
 */
extern LiotOSStatus_t liot_rtos_timer_create(liot_timer_t *timerRef, // OS supplied timer reference
                                             liot_timertype_e cyclicalEn,
                                             void (*callBackRoutine)(void *), // timer call-back routine
                                             void *timerArgc // argument to be passed to call-back on expiration
);

/*!
 * @brief Starts or restarts a timer with the specified timeout value.
 *
 * @details This function starts a previously created timer. If the timer is already running,
 *          it will be stopped and restarted with the new timeout value.
 *
 * @param timerRef [in] OS timer reference.
 * @param setTime  [in] Timeout value in ticks before the timer expires.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Timer started successfully.
 *         - Other: Error code indicating failure.
 */
extern LiotOSStatus_t liot_rtos_timer_start(liot_timer_t timerRef, uint32 setTime);

/*!
 * @brief Starts or restarts a timer from an interrupt service routine (ISR).
 *
 * @details This function is safe to call from an ISR context.
 *          It will not block and schedules the timer for activation.
 *
 * @param timerRef [in] OS timer reference.
 * @param setTime  [in] Timeout value in ticks before the timer expires.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Timer started successfully.
 *         - Other: Error code indicating failure.
 */
extern LiotOSStatus_t liot_rtos_timer_start_isr(liot_timer_t timerRef, uint32 setTime);


/*!
 * @brief Checks whether the specified timer is currently active.
 *
 * @details This function returns whether the timer is currently running.
 *
 * @param timerRef [in] OS timer reference.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Timer is running.
 *         - Other: Timer is not running or invalid handle.
 */
extern LiotOSStatus_t liot_rtos_timer_is_running(liot_timer_t timerRef);


/*!
 * @brief Starts or restarts a timer with microsecond precision.
 *
 * @details This function starts a previously created timer using a timeout value specified in microseconds.
 *          If the timer is already running, it will be stopped and restarted with the new timeout value.
 *
 * @param timerRef    [in] OS timer reference.
 * @param setTime_us  [in] Timeout value in microseconds.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Timer started successfully.
 *         - Other: Error code indicating failure.
 */
LiotOSStatus_t liot_rtos_timer_start_us(liot_timer_t timerRef, uint32 setTime_us);

/*!
 * @brief Stops a running timer.
 *
 * @details This function stops the specified timer if it is currently active.
 *          The timer can be restarted later using `liot_rtos_timer_start`.
 *
 * @param timerRef [in] OS timer reference.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Timer was successfully stopped.
 *         - Other: Error code indicating failure.
 */
extern LiotOSStatus_t liot_rtos_timer_stop(liot_timer_t timerRef);


/*!
 * @brief Stops a running timer from an interrupt service routine (ISR).
 *
 * @details This function is safe to call from an ISR context.
 *          It stops the specified timer without blocking.
 *
 * @param timerRef [in] OS timer reference.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Timer was successfully stopped.
 *         - Other: Error code indicating failure.
 */
extern LiotOSStatus_t liot_rtos_timer_stop_isr(liot_timer_t timerRef);

/*!
 * @brief Deletes a timer and frees associated resources.
 *
 * @details This function removes the timer from the system and releases any memory allocated for it.
 *          After deletion, the timer reference becomes invalid and must not be used again.
 *
 * @param timerRef [in] OS timer reference.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Timer was successfully deleted.
 *         - Other: Error code indicating failure.
 */
extern LiotOSStatus_t liot_rtos_timer_delete(liot_timer_t timerRef);

/*!
 * @brief Gets the current system tick count.
 *
 * @details This function returns the number of ticks that have occurred since the system was started.
 *          It is useful for measuring time intervals or implementing custom timing logic.
 *
 * @return uint32_t
 *         - Current system tick count.
 */
extern uint32 liot_rtos_get_system_tick(void);

/*!
 * @brief Allocates a block of memory dynamically from the heap.
 *
 * @details This function allocates a block of memory of the specified size from the heap.
 *          It should be paired with a call to `liot_rtos_free` when the memory is no longer needed.
 *
 * @param size [in] Size of the memory block to allocate (in bytes).
 *
 * @return void*
 *         - Pointer to the allocated memory block.
 *         - NULL if the allocation failed.
 */
extern void *liot_rtos_malloc(size_t size);

/*!
 * @brief Reallocates a previously allocated memory block to a new size.
 *
 * @details This function changes the size of a memory block that was previously allocated using `liot_rtos_malloc` or `liot_rtos_realloc`.
 *          The contents of the original block are preserved up to the minimum of the old and new sizes.
 *
 * @param ptr  [in] Pointer to the previously allocated memory block.
 * @param size [in] New size for the memory block (in bytes).
 *
 * @return void*
 *         - Pointer to the reallocated memory block.
 *         - NULL if the operation failed (original block remains unchanged).
 */
extern void *liot_rtos_realloc(void *ptr, size_t size);

/*!
 * @brief Allocates and initializes a block of memory dynamically from the heap.
 *
 * @details This function allocates a block of memory for an array of `n` elements,
 *          each of size `Size`, and initializes all bytes in the allocated storage
 *          to zero. It should be paired with a call to `liot_rtos_free` when the
 *          memory is no longer needed.
 *
 * @param n    [in] Number of elements to allocate.
 * @param Size [in] Size of each element (in bytes).
 *
 * @return void*
 *         - Pointer to the allocated and zero-initialized memory block.
 *         - NULL if the allocation failed.
 */
extern void *liot_rtos_calloc(size_t n, size_t Size);

/*!
 * @brief Frees a previously allocated memory block.
 *
 * @details This function deallocates a memory block that was previously allocated using `liot_rtos_malloc` or `liot_rtos_realloc`.
 *          The memory becomes available for future allocations.
 *
 * @param ptr [in] Pointer to the memory block to be freed.
 *
 * @return None
 */
extern void liot_rtos_free(void *ptr);

/*!
 * @brief Sets one or more event flags for a specific thread.
 *
 * @details This function sets the specified flags for the given thread.
 *          These flags can be used for synchronization between tasks or ISRs.
 *
 * @param thread [in] Thread reference.
 * @param flag   [in] Bitmask of flags to set.
 *
 * @return int
 *         - 0: Operation succeeded.
 *         - Non-zero: Error code indicating failure.
 */
extern int liot_rtos_thread_flag_set(liot_task_t *thread, uint32 flag);

/*!
 * @brief Clears one or more event flags for the current thread.
 *
 * @details This function clears the specified flags for the current thread.
 *
 * @param flag [in] Bitmask of flags to clear.
 *
 * @return int
 *         - 0: Operation succeeded.
 *         - Non-zero: Error code indicating failure.
 */
extern int liot_rtos_thread_flag_clear(uint32 flag);

/*!
 * @brief Waits for one or more event flags to be set for the current thread.
 *
 * @details This function blocks the calling thread until the specified flags are set,
 *          or the timeout expires. Returns the flags that were actually set.
 *
 * @param flags   [in] Bitmask of flags to wait for.
 * @param timeout [in] Timeout value in ticks. Use `LIOT_WAIT_FOREVER` to wait indefinitely.
 *
 * @return uint32_t
 *         - Bitmask of flags that were set.
 */
extern uint32 liot_rtos_thread_flag_wait(uint32 flags, uint timeout);

/*!
 * @brief Checks whether the specified task is still running or has been deleted.
 *
 * @details This function checks if the given task reference is valid and the task is still alive.
 *
 * @param taskRef [in] Task reference to check.
 *
 * @return bool
 *         - true: Task is alive.
 *         - false: Task is not alive or reference is invalid.
 */
extern bool liot_rtos_is_alive(liot_task_t taskRef);

/*!
 * @brief Generates a pseudo-random number (32-bit unsigned integer).
 *
 * @details This function generates a 32-bit pseudo-random number using the internal random number generator.
 *          It is typically used in non-security-sensitive scenarios where randomness is required,
 *          such as test cases, simulations, or general-purpose algorithms.
 *
 * @return uint32_t
 *         - A 32-bit pseudo-random number.
 */
extern uint32 liot_rtos_rand();

/*!
 * @brief Gets the total size of the heap memory (in bytes).
 *
 * @details This function returns the total configured size of the heap memory available for dynamic allocation.
 *          It helps developers understand the memory configuration and ensure that the heap size meets application needs.
 *
 * @return size_t
 *         - Total heap size in bytes.
 */
extern size_t liot_xPortGetTotalHeapSize(void);

/*!
 * @brief Gets the current amount of free heap memory (in bytes).
 *
 * @details This function returns the amount of currently available free memory in the heap.
 *          Developers should be cautious about memory fragmentation — frequent allocations of varying sizes may result
 *          in insufficient contiguous memory even when total free memory appears sufficient.
 *
 * @return size_t
 *         - Current available free heap size in bytes.
 */
extern size_t liot_xPortGetFreeHeapSize(void);

/*!
 * @brief Gets the minimum ever recorded free heap size since system startup.
 *
 * @details This function returns the smallest amount of free heap memory that has been available since the system started.
 *          It is useful for analyzing memory usage patterns and identifying potential memory exhaustion points.
 *
 * @return size_t
 *         - Minimum ever free heap size in bytes.
 */
extern size_t liot_xPortGetMinimumEverFreeHeapSize(void);

/*!
 * @brief Gets the size of the largest contiguous free memory block in the heap.
 *
 * @details This function returns the size of the largest continuous block of free memory currently available in the heap.
 *          It is particularly useful for diagnosing memory fragmentation issues and determining whether large memory allocations are feasible.
 *
 * @return size_t
 *         - Size of the largest contiguous free memory block in bytes.
 */
extern size_t liot_xPortGetMaximumFreeBlockSize(void);

/*!
 * @brief Creates and initializes an event flags object.
 *
 * @details This function creates a new event flags object that can be used to manage synchronization between tasks or ISRs.
 *
 * @param flagRef [out] Pointer to store the reference of the created event flags object.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Event flags object was successfully created.
 *         - Other: Error code indicating failure.
 */
extern LiotOSStatus_t liot_rtos_flag_create(liot_flag_t *flagRef);

/*!
 * @brief Gets the current value of the event flags.
 *
 * @details This function retrieves the current state (bit values) of all event flags in the specified event group.
 *
 * @param flagRef [in] Event flags ID obtained from `liot_rtos_flag_create`.
 *
 * @return uint32_t
 *         - The current value of the bits in the event group.
 */
extern uint32_t liot_rtos_flag_get(liot_flag_t flagRef);

/*!
 * @brief Waits for one or more event flags to become set.
 *
 * @details This function blocks the calling task until the specified condition on event flags is met,
 *          or the timeout expires. Supports multiple wait modes like AND/OR logic and optional clear operations.
 *
 * @param flagRef    [in]  Event flags ID obtained from `liot_rtos_flag_create`.
 * @param mask       [in]  Bitmask specifying which flags to wait for.
 * @param operation  [in]  Operation type: LIOT_FLAG_AND, LIOT_FLAG_OR, LIOT_FLAG_AND_CLEAR, LIOT_FLAG_OR_CLEAR.
 * @param flag       [out] Pointer to store the current value of flags after the wait completes.
 * @param timeout    [in]  Timeout value in ticks. Use `LIOT_WAIT_FOREVER` to wait indefinitely.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: One or more of the requested flags were set within the timeout period.
 *         - Other: Error code indicating timeout, invalid handle, or operation failure.
 */
extern LiotOSStatus_t liot_rtos_flag_wait(
    liot_flag_t flagRef, UINT32 mask, liot_flag_op_e operation, UINT32 *flag, UINT32 timeout);

/*!
 * @brief Sets the specified event flags.
 *
 * @details This function sets one or more event flags in the specified event group.
 *          It can be safely called from both task and ISR context.
 *
 * @param flagRef    [in] Event flags ID obtained from `liot_rtos_flag_create`.
 * @param mask       [in] Bitmask specifying which flags to set.
 * @param operation  [in] Reserved parameter; should always be set to a valid default value.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Flags were successfully set.
 *         - Other: Error code indicating failure.
 */
extern LiotOSStatus_t liot_rtos_flag_release(liot_flag_t flagRef, UINT32 mask, liot_flag_op_e operation);

/*!
 * @brief Clears the specified event flags.
 *
 * @details This function clears (resets) one or more flags in the specified event group.
 *
 * @param flagRef [in] Event flags ID obtained from `liot_rtos_flag_create`.
 * @param mask    [in] Bitmask specifying which flags to clear.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Flags were successfully cleared.
 *         - Other: Error code indicating failure.
 */
extern LiotOSStatus_t liot_rtos_flag_clear(liot_flag_t flagRef, UINT32 mask);

/*!
 * @brief Deletes an event flags object and frees associated resources.
 *
 * @details This function removes the event flags object from the system and releases any memory allocated for it.
 *          After deletion, the event flags reference becomes invalid and must not be used again.
 *
 * @param flagRef [in] Event flags ID obtained from `liot_rtos_flag_create`.
 *
 * @return LiotOSStatus_t
 *         - LIOT_OSI_SUCCESS: Event flags object was successfully deleted.
 *         - Other: Error code indicating failure.
 */
extern LiotOSStatus_t liot_rtos_flag_delete(liot_flag_t flagRef);

/*lierda start:add EC718 enable psram by zhaoliechang in 20231008*/
/*
 * psram API
 */
#if defined (PSRAM_FEATURE_ENABLE) && (PSRAM_EXIST==1)

/*!
 * @brief Allocates memory in PSRAM dynamically, applicable to EC718PM series chips.
 *
 * @details This function is used to dynamically allocate a block of memory in PSRAM.
 *          It behaves similarly to standard `malloc`, but targets external PSRAM.
 *
 * @param size [in] Size of the memory block to allocate (in bytes).
 *
 * @return void*
 *         - Pointer to the allocated memory block.
 *         - NULL if the allocation failed.
 */
void *liot_rtos_psram_malloc(size_t size);

/*!
 * @brief Reallocates memory in PSRAM dynamically, applicable to EC718PM series chips.
 *
 * @details This function changes the size of an already allocated memory block in PSRAM.
 *          It may move the memory block to a new location if necessary.
 *
 * @param ptr  [in] Pointer to the previously allocated memory block.
 * @param size [in] New size for the memory block (in bytes).
 *
 * @return void*
 *         - Pointer to the reallocated memory block.
 *         - NULL if the operation failed (original memory block is still valid).
 */
void *liot_rtos_psram_realloc(void *ptr, size_t size);

/*!
 * @brief Frees memory previously allocated in PSRAM, applicable to EC718PM series chips.
 *
 * @details This function deallocates a memory block that was previously allocated
 *          using `liot_rtos_psram_malloc` or `liot_rtos_psram_realloc`.
 *
 * @param ptr [in] Pointer to the memory block to be freed.
 *
 * @return None
 */
void liot_rtos_psram_free(void *ptr);

/*!
 * @brief Gets the total size of PSRAM heap memory, applicable to EC718PM series chips.
 *
 * @details This function returns the total size (in bytes) of the PSRAM heap.
 *
 * @return size_t
 *         - Total size of PSRAM heap memory in bytes.
 */
size_t liot_psram_xPortGetTotalHeapSize(void);

/*!
 * @brief Gets the current free memory size in PSRAM, applicable to EC718PM series chips.
 *
 * @details This function returns the amount of currently available free memory in PSRAM.
 *
 * @return size_t
 *         - Current available free memory size in bytes.
 */
size_t liot_psram_xPortGetFreeHeapSize(void);

/*!
 * @brief Gets the minimum ever free memory size in PSRAM, applicable to EC718PM series chips.
 *
 * @details This function returns the smallest amount of free memory that has been available in PSRAM since startup.
 *          Useful for memory usage analysis and debugging.
 *
 * @return size_t
 *         - Minimum ever free memory size in bytes.
 */
size_t liot_psram_xPortGetMinimumEverFreeHeapSize(void);

/*!
 * @brief Gets the size of the largest free memory block in PSRAM, applicable to EC718PM series chips.
 *
 * @details This function returns the size of the largest contiguous free block in PSRAM.
 *          It can be used to analyze memory fragmentation.
 *
 * @return size_t
 *         - Size of the largest free memory block in bytes.
 */
size_t liot_psram_xPortGetMaximumFreeBlockSize(void);

#endif
/*lierda end:add EC718 enable psram by zhaoliechang in 20231008*/

/*!
 * @brief Generate a true random number (32-bit unsigned integer).
 *
 * @details This function is used to generate a random number, suitable for security-sensitive
 *          application scenarios such as key generation, nonce values, etc.
 *
 * @return uint32
 *         - Returns a 32-bit true random number.
 */
uint32 liot_true_rand(void);

void __Liot_SetAppSdkversion(char * appversion);

#ifdef __cplusplus
} /*"C" */
#endif

#endif