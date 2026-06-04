#include <string.h>
#include "cmsis_os2.h"
#include "liot_os.h"
#include "liot_log.h"

#define THREAD_STACK_SIZE_API_TEST      (10 * 1024)

void semaphoreTest(void)
{
    osSemaphoreId_t semaphoreId;

    printf("semaphoreTest\r\n");
    semaphoreId = osSemaphoreNew(1, 0, NULL);
    printf("Release semaphore.\r\n");
    osSemaphoreRelease(semaphoreId);
    printf("Wait semaphore.\r\n");
    osSemaphoreAcquire(semaphoreId, osWaitForever);
    printf("Received semaphore.\r\n\r\n");
}

void messageQueueTest(void)
{
    osMessageQueueId_t messageQueueId;
    uint32_t           testData  = 0x12;
    uint32_t           testData2 = 0;

    printf("messageQueueTest\r\n");
    messageQueueId = osMessageQueueNew(1, 4, NULL);
    printf("Put queue: 0x%X\r\n", testData);
    osMessageQueuePut(messageQueueId, &testData, 0, 0);
    printf("Wait queue.\r\n");
    osMessageQueueGet(messageQueueId, &testData2, 0, osWaitForever);
    printf("Received queue: 0x%X\r\n\r\n", testData2);
}

void ThreadApiTest(void *argument)
{
    uint32_t count = 0;

    osDelay(2000);
    semaphoreTest();
    messageQueueTest();

    while (1)
    {
        printf("osDelay %ds\r\n", count++);
        osDelay(1000 * count);
    }
}

void liot_cmsis_demo_thread(void *argv)
{
    osThreadAttr_t threadAttr;
    memset(&threadAttr, 0, sizeof(threadAttr));
    threadAttr.name       = "ThreadApiTest";
    threadAttr.stack_mem  = NULL;
    threadAttr.stack_size = THREAD_STACK_SIZE_API_TEST;
    threadAttr.priority   = osPriorityNormal;
    osThreadNew(ThreadApiTest, NULL, &threadAttr);

    liot_rtos_task_delete(NULL); // kill itself
}