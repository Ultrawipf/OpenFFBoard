/*
 * host_freertos.cpp
 *
 * Fake FreeRTOS kernel for the host unit tests. See support/HostRtos.h for the concept.
 */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "HostRtos.h"
#include "HostPlatform.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

struct HostTask {
	TaskFunction_t fn = nullptr;
	void *param = nullptr;
	std::string name;
	UBaseType_t priority = 0;
	uint32_t notifications = 0;
	bool suspended = false;
};

struct HostSemaphore {
	UBaseType_t count = 0;
	UBaseType_t maxCount = 1;
};

namespace {
std::vector<HostTask *> &tasks() {
	static std::vector<HostTask *> list;
	return list;
}
HostTask *currentTask = nullptr;
uint32_t delayBudgetMs = 1000;
uint32_t delayBudgetRemaining = 0;

bool isRegistered(HostTask *task) {
	return std::find(tasks().begin(), tasks().end(), task) != tasks().end();
}
} // namespace

// ---------------- Test control ----------------
namespace HostRtos {

void reset() {
	for (HostTask *task : tasks()) {
		task->notifications = 0;
	}
	delayBudgetMs = 1000;
	currentTask = nullptr;
}

bool runUntilBlocked(TaskHandle_t task) {
	if (!isRegistered(task) || currentTask != nullptr) {
		return false;
	}
	currentTask = task;
	delayBudgetRemaining = delayBudgetMs;
	try {
		task->fn(task->param);
	} catch (const TaskBlocked &) {
	}
	currentTask = nullptr;
	return true;
}

uint32_t runUntilIdle(uint32_t maxActivations) {
	uint32_t activations = 0;
	bool ranAny = true;
	while (ranAny && activations < maxActivations) {
		ranAny = false;
		// Tasks may be created or deleted while running. Iterate over a copy
		std::vector<HostTask *> snapshot = tasks();
		for (HostTask *task : snapshot) {
			if (isRegistered(task) && task->notifications > 0 && !task->suspended) {
				runUntilBlocked(task);
				activations++;
				ranAny = true;
			}
		}
	}
	return activations;
}

uint32_t pendingNotifications(TaskHandle_t task) {
	return isRegistered(task) ? task->notifications : 0;
}

size_t taskCount() {
	return tasks().size();
}

void setDelayBudgetMs(uint32_t ms) {
	delayBudgetMs = ms;
}

} // namespace HostRtos

// ---------------- Kernel API ----------------
extern "C" {

void *pvPortMalloc(size_t size) { return malloc(size); }
void vPortFree(void *ptr) { free(ptr); }
size_t xPortGetFreeHeapSize(void) { return 0x10000; }
size_t xPortGetMinimumEverFreeHeapSize(void) { return 0x8000; }

BaseType_t xTaskCreate(TaskFunction_t fn, const char *name, uint32_t, void *param, UBaseType_t prio, TaskHandle_t *handle) {
	HostTask *task = new HostTask();
	task->fn = fn;
	task->param = param;
	task->name = name ? name : "";
	task->priority = prio;
	tasks().push_back(task);
	if (handle) {
		*handle = task;
	}
	return pdPASS;
}

void vTaskDelete(TaskHandle_t task) {
	if (task == nullptr) {
		task = currentTask;
	}
	auto it = std::find(tasks().begin(), tasks().end(), task);
	if (it != tasks().end()) {
		tasks().erase(it);
		if (currentTask == task) {
			currentTask = nullptr;
		}
		delete task;
	}
}

void vTaskDelay(TickType_t ticks) {
	HostPlatform::advanceMs(ticks);
	if (currentTask != nullptr) {
		if (ticks >= delayBudgetRemaining) {
			throw HostRtos::TaskBlocked();
		}
		delayBudgetRemaining -= ticks;
	}
}

void vTaskDelayUntil(TickType_t *previousWakeTime, TickType_t increment) {
	TickType_t now = xTaskGetTickCount();
	TickType_t wake = *previousWakeTime + increment;
	*previousWakeTime = wake;
	if (wake > now) {
		vTaskDelay(wake - now);
	}
}

TickType_t xTaskGetTickCount(void) { return (TickType_t)(HostPlatform::nowUs() / 1000); }
TickType_t xTaskGetTickCountFromISR(void) { return xTaskGetTickCount(); }
void vTaskStartScheduler(void) {}
void vTaskEndScheduler(void) {}
void vTaskSuspend(TaskHandle_t task) {
	if (isRegistered(task)) task->suspended = true;
}
void vTaskResume(TaskHandle_t task) {
	if (isRegistered(task)) task->suspended = false;
}
BaseType_t xTaskResumeFromISR(TaskHandle_t task) {
	vTaskResume(task);
	return pdFALSE;
}
void vTaskSuspendAll(void) {}
BaseType_t xTaskResumeAll(void) { return pdFALSE; }
UBaseType_t uxTaskPriorityGet(TaskHandle_t task) { return isRegistered(task) ? task->priority : 0; }
UBaseType_t uxTaskPriorityGetFromISR(TaskHandle_t task) { return uxTaskPriorityGet(task); }
void vTaskPrioritySet(TaskHandle_t task, UBaseType_t prio) {
	if (isRegistered(task)) task->priority = prio;
}
char *pcTaskGetName(TaskHandle_t task) {
	static char empty[1] = {0};
	return isRegistered(task) ? task->name.data() : empty;
}
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return currentTask; }
BaseType_t xTaskGetSchedulerState(void) { return taskSCHEDULER_RUNNING; }
UBaseType_t uxTaskGetNumberOfTasks(void) { return tasks().size(); }
UBaseType_t uxTaskGetSystemState(TaskStatus_t *, UBaseType_t, uint32_t *) { return 0; }
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t) { return 0; }
void vTaskList(char *buf) {
	if (buf) *buf = 0;
}
void vTaskGetRunTimeStats(char *buf) {
	if (buf) *buf = 0;
}

BaseType_t xTaskNotifyGive(TaskHandle_t task) {
	if (isRegistered(task)) {
		task->notifications++;
	}
	return pdPASS;
}

void vTaskNotifyGiveFromISR(TaskHandle_t task, BaseType_t *higherPriorityTaskWoken) {
	xTaskNotifyGive(task);
	if (higherPriorityTaskWoken) {
		*higherPriorityTaskWoken = pdFALSE;
	}
}

uint32_t ulTaskNotifyTake(BaseType_t clearOnExit, TickType_t) {
	if (currentTask == nullptr || currentTask->notifications == 0) {
		// Nothing pending. The task would block here
		throw HostRtos::TaskBlocked();
	}
	uint32_t count = currentTask->notifications;
	currentTask->notifications = clearOnExit ? 0 : count - 1;
	return count;
}

// ---------------- Semaphores ----------------
SemaphoreHandle_t xSemaphoreCreateBinary(void) { return new HostSemaphore{0, 1}; }
SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t maxCount, UBaseType_t initialCount) { return new HostSemaphore{initialCount, maxCount}; }
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return new HostSemaphore{1, 1}; }
SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void) { return new HostSemaphore{0xffff, 0xffff}; }
void vSemaphoreDelete(SemaphoreHandle_t sem) { delete sem; }

BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t ticksToWait) {
	if (sem->count > 0) {
		sem->count--;
		return pdTRUE;
	}
	if (currentTask != nullptr && ticksToWait == portMAX_DELAY) {
		throw HostRtos::TaskBlocked(); // Would wait forever
	}
	return pdFALSE;
}
BaseType_t xSemaphoreTakeFromISR(SemaphoreHandle_t sem, BaseType_t *higherPriorityTaskWoken) {
	if (higherPriorityTaskWoken) *higherPriorityTaskWoken = pdFALSE;
	if (sem->count > 0) {
		sem->count--;
		return pdTRUE;
	}
	return pdFALSE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t sem) {
	if (sem->count < sem->maxCount) {
		sem->count++;
		return pdTRUE;
	}
	return pdFALSE;
}
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t sem, BaseType_t *higherPriorityTaskWoken) {
	if (higherPriorityTaskWoken) *higherPriorityTaskWoken = pdFALSE;
	return xSemaphoreGive(sem);
}
BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t sem, TickType_t ticksToWait) { return xSemaphoreTake(sem, ticksToWait); }
BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t sem) { return xSemaphoreGive(sem); }
UBaseType_t uxSemaphoreGetCount(SemaphoreHandle_t sem) { return sem->count; }

} // extern "C"
