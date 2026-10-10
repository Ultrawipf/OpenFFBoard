/*
 * Host test replacement for the target FreeRTOSConfig.h
 * Only the switches evaluated by the firmware sources and the cpp_freertos wrappers are defined.
 */
#ifndef HOSTTEST_FREERTOSCONFIG_H_
#define HOSTTEST_FREERTOSCONFIG_H_

#define configUSE_TASK_NOTIFICATIONS 1
#define configUSE_MUTEXES 1
#define configUSE_RECURSIVE_MUTEXES 1
#define configUSE_COUNTING_SEMAPHORES 1
#define configUSE_TIMERS 1
#define configTICK_RATE_HZ 1000
#define configMINIMAL_STACK_SIZE 128
#define configMAX_PRIORITIES 56
#define configMAX_TASK_NAME_LEN 16

#define INCLUDE_vTaskDelay 1
#define INCLUDE_vTaskDelayUntil 1
#define INCLUDE_vTaskDelete 1
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_xTaskResumeFromISR 1
#define INCLUDE_uxTaskPriorityGet 1
#define INCLUDE_vTaskPrioritySet 1

#endif
