/* Host test replacement for FreeRTOS event_groups.h. Event groups are not used by the tested sources yet. */
#ifndef HOSTTEST_EVENT_GROUPS_H_
#define HOSTTEST_EVENT_GROUPS_H_
#include "FreeRTOS.h"
typedef void *EventGroupHandle_t;
typedef uint32_t EventBits_t;
#endif
