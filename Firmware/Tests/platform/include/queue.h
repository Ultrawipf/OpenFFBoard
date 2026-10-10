/* Host test replacement for FreeRTOS queue.h. Queues are not used by the tested sources yet. */
#ifndef HOSTTEST_QUEUE_H_
#define HOSTTEST_QUEUE_H_
#include "FreeRTOS.h"
typedef void *QueueHandle_t;
#endif
