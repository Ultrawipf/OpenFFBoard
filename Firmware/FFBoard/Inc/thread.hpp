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


#ifndef THREAD_HPP_
#define THREAD_HPP_

/**
 *  The default in the C++ Wrapper classes is to use the C++ string class. 
 *  If you do not want this, define the following in your makefile or 
 *  project, and the Thread class will default to using character arrays
 *  instead of C++ strings.
 *
 *  @note If you define this, you also must define CPP_FREERTOS_NO_EXCEPTIONS.
 *  Some classes throw exceptions if they cannot be constructed, and the 
 *  exceptions they throw depend on C++ strings.
 */
#ifndef CPP_FREERTOS_NO_CPP_STRINGS
#include <string>
#endif
#include "FreeRTOS.h"
#include "task.h"
#include "mutex.hpp"
#include "semaphore.hpp"
#include "condition_variable.hpp"

namespace cpp_freertos {


/**
 *  Wrapper class around FreeRTOS's implementation of a task.
 *
 *  This is an abstract base class.
 *  To use this, you need to subclass it. All of your threads should
 *  be derived from the Thread class. Then implement the virtual Run
 *  function. This is a similar design to Java threading.
 *
 *  By default, we leverage C++ strings for the Thread Name. If this
 *  is not desirable, define CPP_FREERTOS_NO_CPP_STRINGS and the class
 *  will fall back to C character arrays.
 */
class Thread {

    /////////////////////////////////////////////////////////////////////////
    //
    //  Public API
    //  Available from anywhere. Many of these require a Thread reference
    //  if they are operating on a thread.
    //
    /////////////////////////////////////////////////////////////////////////
    public:
        /**
         *  Constructor to create a named thread.
         *
         *  @param Name Name of the thread. Only useful for debugging.
         *  @param StackDepth Number of "words" allocated for the Thread stack.
         *  @param Priority FreeRTOS priority of this Thread.
         */
#ifndef CPP_FREERTOS_NO_CPP_STRINGS
        Thread( const std::string Name,
                uint16_t StackDepth,
                UBaseType_t Priority);
#else
        Thread( const char *Name,
                uint16_t StackDepth,
                UBaseType_t Priority);
#endif

        /**
         *  Constructor to create an unnamed thread.
         *
         *  @param StackDepth Number of "words" allocated for the Thread stack.
         *  @param Priority FreeRTOS priority of this Thread.
         */
        Thread( uint16_t StackDepth,
                UBaseType_t Priority);

        /**
         *  Starts a thread.
         *
         *  This is the API call that actually starts the thread running. 
         *  It creates a backing FreeRTOS task. By separating object creation 
         *  from starting the Thread, it solves the pure virtual fuction call 
         *  failure case. If we attempt to automatically call xTaskCreate 
         *  from the base class constructor, in certain conditions the task 
         *  starts to run "before" the derived class is constructed! So we 
         *  don't do that anymore.
         *
         *  This may be called from your ctor once you have completed 
         *  your objects construction (so as the last step). 
         *
         *  This should only be called once ever! 
         */
        bool Start();

        /**
         *  Our destructor. This must exist even if FreeRTOS is
         *  configured to disallow task deletion.
         */
        virtual ~Thread();

        /**
         *  Accessor to get the thread's backing task handle.
         *  There is no setter, on purpose.
         *
         *  @return FreeRTOS task handle.
         */
        inline TaskHandle_t GetHandle()
        {
            return handle;
        }

        /**
         *  Yield the scheduler.
         */
        static inline void Yield()
        {
            taskYIELD();
        }

        /**
         *  Start the scheduler.
         *
         *  @note You need to use this call. Do NOT directly call 
         *  vTaskStartScheduler while using this library.
         */
        static inline void StartScheduler()
        {
            SchedulerActive = true;
            vTaskStartScheduler();
        }

        /**
         *  End the scheduler.
         *
         *  @note Please see the FreeRTOS documentation regarding constraints
         *  with the implementation of this.
         *
         *  @note You need to use this call. Do NOT directly call 
         *  vTaskEndScheduler while using this library.
         */
        static inline void EndScheduler()
        {
            vTaskEndScheduler();
            SchedulerActive = false;
        }

#if (INCLUDE_vTaskSuspend == 1)
        /**
         *  Suspend this thread.
         *
         *  @note While a Thread can Suspend() itself, it cannot Resume() 
         *  itself, becauseit's suspended.
         */
        inline void Suspend()
        {
            vTaskSuspend(GetHandle());
        }

        /**
         *  Resume a specific thread.
         */
        inline void Resume()
        {
            vTaskResume(GetHandle());
        }
#endif

#if (INCLUDE_xTaskResumeFromISR == 1)
        /**
         *  Resume a specific thread from ISR context.
         */
        inline void ResumeFromISR()
        {
            xTaskResumeFromISR(GetHandle());
        }
#endif

#if (INCLUDE_uxTaskPriorityGet == 1)
        /**
         *  Get the priority of this Thread.
         *
         *  @return Priority at the time this was called.
         */
        inline UBaseType_t GetPriority()
        {
            return (uxTaskPriorityGet(GetHandle()));
        }

        /**
         *  Get the priority of this Thread from ISR context.
         *
         *  @return Priority at the time this was called.
         */
        inline UBaseType_t GetPriorityFromISR()
        {
            return (uxTaskPriorityGetFromISR(GetHandle()));
        }
#endif


#if (INCLUDE_vTaskPrioritySet == 1)
        /**
         *  Set the priority of this thread.
         *
         *  @param NewPriority The thread's new priority.
         */
        inline void SetPriority(UBaseType_t NewPriority)
        {
            Priority = NewPriority;
            vTaskPrioritySet(GetHandle(), NewPriority);
        }
#endif

        /**
         *  Get the name of this thread.
         *
         *  @return a C++ string with the name of the task.
         */
#ifndef CPP_FREERTOS_NO_CPP_STRINGS
        inline std::string GetName()
        {
            return Name;
        }
#else
        inline char* GetName()
        {
            return pcTaskGetName(handle);
        }
#endif

    /////////////////////////////////////////////////////////////////////////
    //
    //  Protected API
    //  Available from inside your Thread implementation.
    //  You should make sure that you are only calling these methods
    //  from within your Run() method, or that your Run() method is on the
    //  callstack.
    //
    /////////////////////////////////////////////////////////////////////////
    protected:
        /**
         *  Implementation of your actual thread code.
         *  You must override this function.
         *  @note If INCLUDE_vTaskDelete is defined, then you may return from
         *  your Run method. This will cause the task to be deleted from
         *  FreeRTOS, however you are still responsible to delete the
         *  task object. If this is not defined, then retuning from your Run()
         *  method will result in an assert.
         */
        virtual void Run() = 0;

#if (INCLUDE_vTaskDelete == 1)
        /**
         *  Called on exit from your Run() routine. 
         *  
         *  It is optional whether you implement this or not.
         *
         *  If you allow your Thread to exit its Run method, 
         *  implementing a Cleanup method allows you to call 
         *  your Thread's destructor. If you decide to call delete 
         *  in your Cleanup function, be aware that additional 
         *  derived classes shouldn't call delete. 
         */ 
        virtual void Cleanup();
#else
        /**
         *  If we can't delete a task, it makes no sense to have a
         *  Cleanup routine.
         */
#endif

#if (INCLUDE_vTaskDelay == 1)
        /**
         *  Delay this thread for at least Delay ticks.
         *
         *  @param Delay How long to delay the thread.
         */
        void inline Delay(const TickType_t Delay)
        {
            vTaskDelay(Delay);
        }
#endif

#if (INCLUDE_vTaskDelayUntil == 1)
        /**
         *  Delay this thread for Period ticks, taking into account
         *  the execution time of the thread.
         *
         *  This FreeRTOS permutation of delay can be used by periodic
         *  tasks to ensure a constant execution frequency.
         *
         *  @param Period How long to delay the thread.
         */
        void DelayUntil(const TickType_t Period);

        /**
         *  If you need to adjust or reset the period of the
         *  DelayUntil method.
         */
        void ResetDelayUntil();
#endif


#ifdef CPP_FREERTOS_CONDITION_VARIABLES

        /**
         *  Have this thread wait on a condition variable.
         *
         *  @note Threads wait, while ConditionVariables signal.
         *
         *  @param Cv The condition variable associated with the Wait.
         *  @param CvLock The required condition variable lock. The
         *  Lock must be held before calling Wait.
         *  @param Timeout Allows you to specify a timeout on the Wait,
         *  if desired.
         *
         *  @return true if the condition variable was signaled,
         *  false if it timed out.
         */
        bool Wait(  ConditionVariable &Cv,
                    Mutex &CvLock,
                    TickType_t Timeout = portMAX_DELAY);

#endif


    /////////////////////////////////////////////////////////////////////////
    //
    //  Private API
    //  The internals of this wrapper class.
    //
    /////////////////////////////////////////////////////////////////////////
    private:
        /**
         *  Reference to the underlying task handle for this thread.
         *  Can be obtained from GetHandle().
         */
        TaskHandle_t handle;

        /**
         *  We need to track whether the scheduler is active or not.
         */
        static volatile bool SchedulerActive;

        /**
         *  The name of this thread.
         */
#ifndef CPP_FREERTOS_NO_CPP_STRINGS
        const std::string Name;
#else
        char Name[configMAX_TASK_NAME_LEN];
#endif

        /**
         *  Stack depth of this Thread, in words.
         */
        const uint16_t StackDepth;

        /**
         *  A saved / cached copy of what the Thread's priority is.
         */
        UBaseType_t Priority;

        /**
         *  Flag whether or not the Thread was started.
         */
        bool ThreadStarted;
        
        /**
         *  Make sure no one calls Start more than once.
         */
        static MutexStandard StartGuardLock;

        /**
         *  Adapter function that allows you to write a class
         *  specific Run() function that interfaces with FreeRTOS.
         *  Look at the implementation of the constructors and this
         *  code to see how the interface between C and C++ is performed.
         */
        static void TaskFunctionAdapter(void *pvParameters);

#if (INCLUDE_vTaskDelayUntil == 1)
        /**
         *  Flag denoting if we've setup delay until yet.
         */
        bool delayUntilInitialized;

        /**
         *  Book keeping value for delay until.
         */
        TickType_t delayUntilPreviousWakeTime;
#endif

#ifdef CPP_FREERTOS_CONDITION_VARIABLES

        /**
         *  How we wait and signal the thread when using condition variables.
         *  Because a semaphore maintains state, this solves the race
         *  condition between dropping the CvLock and waiting.
         */
        BinarySemaphore ThreadWaitSem;

        /**
         *  Internal helper function to signal this thread.
         */
        inline void Signal()
        {
            ThreadWaitSem.Give();
        }

    /**
     *  The Thread class and the ConditionVariable class are interdependent.
     *  If we allow the ConditionVariable class to access the internals of 
     *  the Thread class, we can reduce the public interface, which is a
     *  good thing.
     */
    friend class ConditionVariable;

#endif

};


}
#endif

