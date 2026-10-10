/* Host test replacement for FreeRTOS task.h. See FreeRTOS.h */
#ifndef HOSTTEST_TASK_H_
#define HOSTTEST_TASK_H_
#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

struct HostTask; // defined in host_freertos.cpp
typedef struct HostTask *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);

typedef enum { eRunning = 0, eReady, eBlocked, eSuspended, eDeleted, eInvalid } eTaskState;
typedef enum { eNoAction = 0, eSetBits, eIncrement, eSetValueWithOverwrite, eSetValueWithoutOverwrite } eNotifyAction;

typedef struct xTASK_STATUS {
	TaskHandle_t xHandle;
	const char *pcTaskName;
	UBaseType_t xTaskNumber;
	eTaskState eCurrentState;
	UBaseType_t uxCurrentPriority;
	UBaseType_t uxBasePriority;
	uint32_t ulRunTimeCounter;
	StackType_t *pxStackBase;
	uint16_t usStackHighWaterMark;
} TaskStatus_t;

#define taskSCHEDULER_SUSPENDED ((BaseType_t)0)
#define taskSCHEDULER_NOT_STARTED ((BaseType_t)1)
#define taskSCHEDULER_RUNNING ((BaseType_t)2)

BaseType_t xTaskCreate(TaskFunction_t fn, const char *name, uint32_t stackDepth, void *param, UBaseType_t prio, TaskHandle_t *handle);
void vTaskDelete(TaskHandle_t task);
void vTaskDelay(TickType_t ticks);
void vTaskDelayUntil(TickType_t *previousWakeTime, TickType_t increment);
TickType_t xTaskGetTickCount(void);
TickType_t xTaskGetTickCountFromISR(void);
void vTaskStartScheduler(void);
void vTaskEndScheduler(void);
void vTaskSuspend(TaskHandle_t task);
void vTaskResume(TaskHandle_t task);
BaseType_t xTaskResumeFromISR(TaskHandle_t task);
void vTaskSuspendAll(void);
BaseType_t xTaskResumeAll(void);
UBaseType_t uxTaskPriorityGet(TaskHandle_t task);
UBaseType_t uxTaskPriorityGetFromISR(TaskHandle_t task);
void vTaskPrioritySet(TaskHandle_t task, UBaseType_t prio);
char *pcTaskGetName(TaskHandle_t task);
TaskHandle_t xTaskGetCurrentTaskHandle(void);
BaseType_t xTaskGetSchedulerState(void);
UBaseType_t uxTaskGetNumberOfTasks(void);
UBaseType_t uxTaskGetSystemState(TaskStatus_t *array, UBaseType_t size, uint32_t *totalRunTime);
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t task);
void vTaskList(char *buf);
void vTaskGetRunTimeStats(char *buf);

BaseType_t xTaskNotifyGive(TaskHandle_t task);
void vTaskNotifyGiveFromISR(TaskHandle_t task, BaseType_t *higherPriorityTaskWoken);
uint32_t ulTaskNotifyTake(BaseType_t clearOnExit, TickType_t ticksToWait);

#define taskYIELD() ((void)0)
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
#define taskENTER_CRITICAL_FROM_ISR() ((BaseType_t)0)
#define taskEXIT_CRITICAL_FROM_ISR(x) ((void)(x))
#define taskDISABLE_INTERRUPTS() ((void)0)
#define taskENABLE_INTERRUPTS() ((void)0)

#ifdef __cplusplus
}
#endif
#endif
