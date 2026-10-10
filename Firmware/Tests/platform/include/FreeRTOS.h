/*
 * Host test replacement for the FreeRTOS kernel API.
 *
 * There is no scheduler on the host. Tasks are only registered and are executed
 * explicitly and deterministically by the tests (see support/HostRtos.h).
 * The implementation is in platform/src/host_freertos.cpp
 */
#ifndef HOSTTEST_FREERTOS_H_
#define HOSTTEST_FREERTOS_H_

#include <stdint.h>
#include <stddef.h>
#include "FreeRTOSConfig.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef long BaseType_t;
typedef unsigned long UBaseType_t;
typedef uint32_t TickType_t;
typedef uint32_t StackType_t;

#define pdFALSE ((BaseType_t)0)
#define pdTRUE ((BaseType_t)1)
#define pdPASS (pdTRUE)
#define pdFAIL (pdFALSE)
#define portMAX_DELAY ((TickType_t)0xffffffffUL)
#define portTICK_PERIOD_MS ((TickType_t)1)
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define portYIELD_FROM_ISR(x) ((void)(x))
#define portEND_SWITCHING_ISR(x) ((void)(x))
#define configASSERT(x) ((void)(x))

void *pvPortMalloc(size_t size);
void vPortFree(void *ptr);
size_t xPortGetFreeHeapSize(void);
size_t xPortGetMinimumEverFreeHeapSize(void);

#ifdef __cplusplus
}
#endif

#endif
