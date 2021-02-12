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


#ifndef TASKLET_HPP_
#define TASKLET_HPP_


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
#include "semphr.h"


namespace cpp_freertos {


#ifndef CPP_FREERTOS_NO_EXCEPTIONS
/**
 *  This is the exception that is thrown if a Tasklet constructor fails.
 */
class TaskletCreateException : public std::exception {

    public:
        /**
         *  Create the exception.
         */
        TaskletCreateException()
        {
            sprintf(errorString, "Tasklet Constructor Failed");
        }

        /**
         *  Create the exception.
         */
        explicit TaskletCreateException(const char *info)
        {
            snprintf(errorString, sizeof(errorString),
                        "Tasklet Constructor Failed %s", info);
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
        char errorString[80];
};
#endif


/**
 *  A FreeRTOS wrapper for its concept of a Pended Function.
 *  In Linux, one permutation of this would be a Tasklet, or
 *  bottom half processing from an ISR.
 *
 *  This is an abstract base class.
 *  To use this, you need to subclass it. All of your Tasklets should
 *  be derived from the Tasklet class. Then implement the virtual Run
 *  function. This is a similar design to Java threading.
 */
class Tasklet {

    /////////////////////////////////////////////////////////////////////////
    //
    //  Public API
    //
    /////////////////////////////////////////////////////////////////////////
    public:
        /**
         *  Constructor
         *  @note Do not construct inside an ISR! This includes creating 
         *  local instances of this object.
         */
        Tasklet();

        /**
         *  Destructor
         *  @note Do not delete inside an ISR! This includes the automatic 
         *  deletion of local instances of this object when leaving scope.
         */
        virtual ~Tasklet();

        /**
         *  Schedule this Tasklet to run.
         *
         *  @param parameter Value passed to your Run method.
         *  @param CmdTimeout How long to wait to send this command to the
         *         timer daemon.
         *  @returns true if this command will be sent to the timer daemon,
         *           false if it will not (i.e. timeout).
         */
        bool Schedule(  uint32_t parameter,
                        TickType_t CmdTimeout = portMAX_DELAY);

        /**
         *  Schedule this Tasklet to run from ISR context.
         *  This allows FreeRTOS ISRs to defer processing from the ISR
         *  into a task context.
         *
         *  @param parameter Value passed to your Run method.
         *  @param pxHigherPriorityTaskWoken Did this operation result in a
         *         rescheduling event.
         *  @returns true if this command will be sent to the timer daemon,
         *           false if it will not (i.e. timeout).
         */
        bool ScheduleFromISR(   uint32_t parameter,
                                BaseType_t *pxHigherPriorityTaskWoken);

    /////////////////////////////////////////////////////////////////////////
    //
    //  Protected API
    //  Available from inside your Tasklet implementation.
    //  You should make sure that you are only calling these methods
    //  from within your Run() method, or that your Run() method is on the
    //  callstack.
    //
    /////////////////////////////////////////////////////////////////////////
    protected:
        /**
         *  Implementation of your actual tasklet code.
         *  You must override this function.
         *
         *  @param parameter Value passed to you from the Schedule() methods.
         */
        virtual void Run(uint32_t parameter) = 0;

        /**
         *  You must call this in your dtor, to synchronize between 
         *  being called and being deleted.
         */
        void CheckForSafeDelete();

    /////////////////////////////////////////////////////////////////////////
    //
    //  Private API
    //  The internals of this wrapper class.
    //
    /////////////////////////////////////////////////////////////////////////
    private:
        /**
         *  Adapter function that allows you to write a class
         *  specific Run() function that interfaces with FreeRTOS.
         *  Look at the implementation of the constructors and this
         *  code to see how the interface between C and C++ is performed.
         */
        static void TaskletAdapterFunction(void *ref, uint32_t parameter);

        /**
         *  Protect against accidental deletion before we were executed.
         */
        SemaphoreHandle_t DtorLock;
};

}
#endif

