/* Host test replacement for FreeRTOS timers.h. Software timers are not used by the tested sources yet. */
#ifndef HOSTTEST_TIMERS_H_
#define HOSTTEST_TIMERS_H_
#include "FreeRTOS.h"
typedef void *TimerHandle_t;
#endif
