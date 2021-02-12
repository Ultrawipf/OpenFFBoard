/****************************************************************************
 *
 *  Copyright (c) 2017, Michael Becker (michael.f.becker@gmail.com)
 *
 *  This file is part of the FreeRTOS Add-ons project.
 *
 *  Source Code:
 *  https://github.com/michaelbecker/freertos-addons
 *
 *  Project Page:
 *  http://michaelbecker.github.io/freertos-addons/
 *
 *  On-line Documentation:
 *  http://michaelbecker.github.io/freertos-addons/docs/html/index.html
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files
 *  (the "Software"), to deal in the Software without restriction, including
 *  without limitation the rights to use, copy, modify, merge, publish,
 *  distribute, sublicense, and/or sell copies of the Software, and to
 *  permit persons to whom the Software is furnished to do so,subject to the
 *  following conditions:
 *
 *  + The above copyright notice and this permission notice shall be included
 *    in all copies or substantial portions of the Software.
 *  + Credit is appreciated, but not required, if you find this project
 *    useful enough to include in your application, product, device, etc.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 *  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 *  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 *  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 *  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 ***************************************************************************/


#ifndef CRITICAL_HPP_
#define CRITICAL_HPP_

#include "FreeRTOS.h"
#include "task.h"


namespace cpp_freertos {


/**
 *  Wrapper class around various critical section type
 *  synchronization mechanisms within FreeRTOS.
 */
class CriticalSection {

    /////////////////////////////////////////////////////////////////////////
    //
    //  Public API
    //  Available from anywhere.
    //
    /////////////////////////////////////////////////////////////////////////
    public:
        /**
         *  Disable context switches as well as maskable interrupts.
         */
        static inline void Enter()
        {
            taskENTER_CRITICAL();
        }

        /**
         *  Re-enable context switches.
         */
        static inline void Exit()
        {
            taskEXIT_CRITICAL();
        }

        /**
         *  Disable context switches as well as maskable interrupts
         *  from an interrupt context.
         *
         *  @return Opaque representation of interrupt mask state.
         *  This must be passed back to the corresponding call to 
         *  ExitFromISR().
         *
         *  @note See the following for further details:
         *  http://www.freertos.org/taskENTER_CRITICAL_FROM_ISR_taskEXIT_CRITICAL_FROM_ISR.html
         */
        static inline BaseType_t EnterFromISR()
        {
            return taskENTER_CRITICAL_FROM_ISR();
        }

        /**
         *  Re-enable context switches from an interrupt context.
         *
         *  @param savedInterruptStatus This should be the value you 
         *  received from calling EnterFromISR().
         *
         *  @note See the following for further details:
         *  http://www.freertos.org/taskENTER_CRITICAL_FROM_ISR_taskEXIT_CRITICAL_FROM_ISR.html
         */
        static inline void ExitFromISR(BaseType_t savedInterruptStatus)
        {
            taskEXIT_CRITICAL_FROM_ISR(savedInterruptStatus);
        }

        /**
         *  Disable all maskable interrupts.
         */
        static inline void DisableInterrupts()
        {
            taskDISABLE_INTERRUPTS();
        }

        /**
         *  Enable all maskable interrupts.
         */
        static inline void EnableInterrupts()
        {
            taskENABLE_INTERRUPTS();
        }

        /**
         *  Suspend the scheduler without disabling interrupts.
         */
        static inline void SuspendScheduler()
        {
            vTaskSuspendAll();
        }

        /**
         *  Re-enable the scheduler.
         */
        static inline void ResumeScheduler()
        {
            xTaskResumeAll();
        }
};


}
#endif
