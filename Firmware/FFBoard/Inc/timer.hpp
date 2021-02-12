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


#ifndef TIMER_HPP_
#define TIMER_HPP_


/**
 *  C++ exceptions are used by default when constructors fail.
 *  If you do not want this behavior, define the following in your makefile
 *  or project. Note that in most / all cases when a constructor fails,
 *  it's a fatal error. In the cases when you've defined this, the new 
 *  default behavior will be to issue a configASSERT() instead.
 */
#ifndef CPP_FREERTOS_NO_EXCEPTIONS
#include <exception>
#include <string>
#include <cstdio>
#ifdef CPP_FREERTOS_NO_CPP_STRINGS
#error "FreeRTOS-Addons require C++ Strings if you are using exceptions"
#endif
#endif
#include "FreeRTOS.h"
#include "timers.h"


namespace cpp_freertos {


#ifndef CPP_FREERTOS_NO_EXCEPTIONS
/**
 *  This is the exception that is thrown if a Thread constructor fails.
 */
class TimerCreateException  : public std::exception {

    public:
        /**
         *  Create the exception.
         */
        TimerCreateException()
        {
            sprintf(errorString, "Timer Constructor Failed");
        }

        /**
         *  Get what happened as a string.
         *  We are overriding the base implementation here.
         */
        virtual const char *what() const throw()
        {
            return errorString;
        }

    private:
        /**
         *  A text string representing what failed.
         */
        char errorString[40];
};
#endif


/**
 *  Wrapper class around FreeRTOS's implementation of a timer.
 *
 *  This is an abstract base class.
 *  To use this, you need to subclass it. All of your timers should
 *  be derived from the Timer class. Then implement the virtual Run
 *  function. This is a similar design to Java threading.
 */
class Timer {

    /////////////////////////////////////////////////////////////////////////
    //
    //  Public API
    //
    /////////////////////////////////////////////////////////////////////////
    public:
        /**
         *  Construct a named timer.
         *  Timers are not active after they are created, you need to
         *  activate them via Start, Reset, etc.
         *
         *  @throws TimerCreateException
         *  @param TimerName Name of the timer for debug.
         *  @param PeriodInTicks When does the timer expire and run your Run()
         *         method.
         *  @param Periodic true if the timer expires every PeriodInTicks.
         *         false if this is a one shot timer.
         */
        Timer(  const char * const TimerName,
                TickType_t PeriodInTicks,
                bool Periodic = true
                );

        /**
         *  Construct an unnamed timer.
         *  Timers are not active after they are created, you need to
         *  activate them via Start, Reset, etc.
         *
         *  @throws TimerCreateException
         *  @param PeriodInTicks When does the timer expire and run your Run()
         *         method.
         *  @param Periodic true if the timer expires every PeriodInTicks.
         *         false if this is a one shot timer.
         */
        Timer(  TickType_t PeriodInTicks,
                bool Periodic = true
                );

        /**
         *  Destructor
         */
        virtual ~Timer();

        /**
         *  Is the timer currently active?
         *
         *  @return true if the timer is active, false otherwise.
         */
        bool IsActive();

        /**
         *  Start a timer. This changes the state to active.
         *
         *  @param CmdTimeout How long to wait to send this command to the
         *         timer code.
         *  @returns true if this command will be sent to the timer code,
         *           false if it will not (i.e. timeout).
         */
        bool Start(TickType_t CmdTimeout = portMAX_DELAY);

        /**
         *  Start a timer from ISR context. This changes the state to active.
         *
         *  @param pxHigherPriorityTaskWoken Did this operation result in a
         *         rescheduling event.
         *  @returns true if this command will be sent to the timer code,
         *           false if it will not (i.e. timeout).
         */
        bool StartFromISR(BaseType_t *pxHigherPriorityTaskWoken);

        /**
         *  Stop a timer. This changes the state to inactive.
         *
         *  @param CmdTimeout How long to wait to send this command to the
         *         timer code.
         *  @returns true if this command will be sent to the timer code,
         *           false if it will not (i.e. timeout).
         */
        bool Stop(TickType_t CmdTimeout = portMAX_DELAY);

        /**
         *  Stop a timer from ISR context. This changes the state to inactive.
         *
         *  @param pxHigherPriorityTaskWoken Did this operation result in a
         *         rescheduling event.
         *  @returns true if this command will be sent to the timer code,
         *           false if it will not (i.e. timeout).
         */
        bool StopFromISR(BaseType_t *pxHigherPriorityTaskWoken);

        /**
         *  Reset a timer. This changes the state to active.
         *
         *  @param CmdTimeout How long to wait to send this command to the
         *         timer code.
         *  @returns true if this command will be sent to the timer code,
         *           false if it will not (i.e. timeout).
         */
        bool Reset(TickType_t CmdTimeout = portMAX_DELAY);

        /**
         *  Reset a timer from ISR context. This changes the state to active.
         *
         *  @param pxHigherPriorityTaskWoken Did this operation result in a
         *         rescheduling event.
         *  @returns true if this command will be sent to the timer code,
         *           false if it will not (i.e. timeout).
         */
        bool ResetFromISR(BaseType_t *pxHigherPriorityTaskWoken);

        /**
         *  Change a timer's period.
         *
         *  @param NewPeriod The period in ticks.
         *  @param CmdTimeout How long to wait to send this command to the
         *         timer code.
         *  @returns true if this command will be sent to the timer code,
         *           false if it will not (i.e. timeout).
         */
        bool SetPeriod( TickType_t NewPeriod,
                        TickType_t CmdTimeout = portMAX_DELAY);

        /**
         *  Change a timer's period from ISR context.
         *
         *  @param NewPeriod The period in ticks.
         *  @param pxHigherPriorityTaskWoken Did this operation result in a
         *         rescheduling event.
         *  @returns true if this command will be sent to the timer code,
         *           false if it will not (i.e. timeout).
         */
        bool SetPeriodFromISR(  TickType_t NewPeriod,
                                BaseType_t *pxHigherPriorityTaskWoken);

#if (INCLUDE_xTimerGetTimerDaemonTaskHandle == 1)
        /**
         *  If you need it, obtain the task handle of the FreeRTOS
         *  task that is running the timers.
         *
         *  @return Task handle of the FreeRTOS timer task.
         */
        static TaskHandle_t GetTimerDaemonHandle();
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
         *  Implementation of your actual timer code.
         *  You must override this function.
         */
        virtual void Run() = 0;

    /////////////////////////////////////////////////////////////////////////
    //
    //  Private API
    //  The internals of this wrapper class.
    //
    /////////////////////////////////////////////////////////////////////////
    private:
        /**
         *  Reference to the underlying timer handle.
         */
        TimerHandle_t handle;

        /**
         *  Adapter function that allows you to write a class
         *  specific Run() function that interfaces with FreeRTOS.
         *  Look at the implementation of the constructors and this
         *  code to see how the interface between C and C++ is performed.
         */
        static void TimerCallbackFunctionAdapter(TimerHandle_t xTimer);
};


}
#endif
