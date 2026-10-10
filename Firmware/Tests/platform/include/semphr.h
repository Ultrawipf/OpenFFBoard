/* Host test replacement for FreeRTOS semphr.h. See FreeRTOS.h */
#ifndef HOSTTEST_SEMPHR_H_
#define HOSTTEST_SEMPHR_H_
#include "FreeRTOS.h"
#include "queue.h"

#ifdef __cplusplus
extern "C" {
#endif

struct HostSemaphore; // defined in host_freertos.cpp
typedef struct HostSemaphore *SemaphoreHandle_t;

SemaphoreHandle_t xSemaphoreCreateBinary(void);
SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t maxCount, UBaseType_t initialCount);
SemaphoreHandle_t xSemaphoreCreateMutex(void);
SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void);
void vSemaphoreDelete(SemaphoreHandle_t sem);
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t ticksToWait);
BaseType_t xSemaphoreTakeFromISR(SemaphoreHandle_t sem, BaseType_t *higherPriorityTaskWoken);
BaseType_t xSemaphoreGive(SemaphoreHandle_t sem);
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t sem, BaseType_t *higherPriorityTaskWoken);
BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t sem, TickType_t ticksToWait);
BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t sem);
UBaseType_t uxSemaphoreGetCount(SemaphoreHandle_t sem);

#ifdef __cplusplus
}
#endif
#endif
