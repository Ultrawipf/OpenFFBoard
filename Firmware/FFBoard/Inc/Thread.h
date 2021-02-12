/*
 * Thread.h
 *
 *  Created on: Feb 12, 2021
 *      Author: Yannick
 */

#ifndef SRC_THREAD_H_
#define SRC_THREAD_H_

#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "task.h"

class Thread {
public:

	Thread(uint32_t stack_size, osPriority_t priority, const char* name);
	//Thread(unsigned portSHORT stack_size, UBaseType_t priority, const char* name);
	Thread(osThreadAttr_t attributes);

	virtual ~Thread();
	virtual void runThread() = 0;
	void delay(uint32_t time);

	osThreadId_t getThreadId();

protected:
	/*const char                   *name;   ///< name of the thread
  uint32_t                 attr_bits;   ///< attribute bits
  void                      *cb_mem;    ///< memory for control block
  uint32_t                   cb_size;   ///< size of provided memory for control block
  void                   *stack_mem;    ///< memory for stack
  uint32_t                stack_size;   ///< size of stack
  osPriority_t              priority;   ///< initial thread priority (default: osPriorityNormal)
  TZ_ModuleId_t            tz_module;   ///< TrustZone module identifier
  uint32_t                  reserved;   ///< reserved (must be 0)
  */
	static void task( void* _params );
	osThreadAttr_t attributes = {0};
	osThreadId_t taskHandle;
	TaskHandle_t taskHandle2;

};

#endif /* SRC_THREAD_H_ */
