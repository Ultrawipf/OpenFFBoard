/*
 * Thread.cpp
 *
 *  Created on: Feb 12, 2021
 *      Author: Yannick
 */

#include "Thread.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "task.h"
Thread::Thread( uint32_t stackDepth, osPriority_t priority, const char* name = "" )
{
	xTaskCreate( task, name, stackDepth/4, this, (UBaseType_t)priority, (TaskHandle_t*)&this->taskHandle );
}

Thread::Thread(osThreadAttr_t attributes) : attributes(attributes){
	this->taskHandle = osThreadNew(task, this, &this->attributes);
}

Thread::~Thread() {
	vTaskDelete((TaskHandle_t)this->taskHandle);
}


/*
 * Static callback to pass to instance
 */
void Thread::task( void* _params ){
	Thread* p = static_cast<Thread*>( _params );
	p->runThread();
}

/*
 * Thread safe delay
 */
void Thread::delay(uint32_t time){
	osDelay(time);
}

osThreadId_t Thread::getThreadId(){
	return this->taskHandle;
}
