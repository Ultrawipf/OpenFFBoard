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


#ifndef SEMAPHORE_HPP_
#define SEMAPHORE_HPP_

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
#include "semphr.h"


namespace cpp_freertos {


#ifndef CPP_FREERTOS_NO_EXCEPTIONS
/**
 *  This is the exception that is thrown if a Semaphore constructor fails.
 */
class SemaphoreCreateException : public std::exception {

    public:
        /**
         *  Create the exception.
         */
        SemaphoreCreateException()
        {
            sprintf(errorString, "Semaphore Constructor Failed");
        }

        /**
         *  Create the exception.
         */
        explicit SemaphoreCreateException(const char *info)
        {
            snprintf(errorString, sizeof(errorString),
                        "Semaphore Constructor Failed %s", info);
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
 *
 *  Base wrapper class around FreeRTOS's implementation of semaphores.
 *
 *  It is not expected that an application will derive from this class.
 *
 *  Note that we distinguish between Semaphore, Binary Semaphores,
 *  Counting Semaphores, and Mutexes. Mutexes, while implemented as a kind
 *  of semaphore in FreeRTOS, are conceptually very different in use and
 *  behavior from semaphores. We acknowledge this difference in the class
 *  heirarchy, implementing mutextes as a completely different class heirarchy.
 */
class Semaphore {

    /////////////////////////////////////////////////////////////////////////
    //
    //  Public API
    //
    /////////////////////////////////////////////////////////////////////////
    public:
        /**
         *  Aquire (take) a semaphore.
         *
         *  Example of blocking indefinitely:
         *      aSemaphore.Take();
         *
         *  Example of blocking for 100 ticks:
         *      aSemaphore.Take(100);
         *
         *  @param Timeout How long to wait to get the Lock until giving up.
         *  @return true if the Semaphore was acquired, false if it timed out.
         */
        bool Take(TickType_t Timeout = portMAX_DELAY);

        /**
         *  Release (give) a semaphore.
         *
         *  @return true if the Semaphore was released, false if it failed.
         */
        bool Give();

        /**
         *  Aquire (take) a semaphore from ISR context.
         *
         *  @param pxHigherPriorityTaskWoken Did this operation result in a
         *         rescheduling event.
         *  @return true if the Semaphore was acquired, false if it timed out.
         */
        bool TakeFromISR(BaseType_t *pxHigherPriorityTaskWoken);

        /**
         *  Release (give) a semaphore from ISR context.
         *
         *  @param pxHigherPriorityTaskWoken Did this operation result in a
         *         rescheduling event.
         *  @return true if the Semaphore was released, false if it failed.
         */
        bool GiveFromISR(BaseType_t *pxHigherPriorityTaskWoken);

        /**
         *  Our destructor
         */
        virtual ~Semaphore();

    /////////////////////////////////////////////////////////////////////////
    //
    //  Protected API
    //  Not intended for use by application code.
    //
    /////////////////////////////////////////////////////////////////////////
    protected:
        /**
         *  FreeRTOS semaphore handle.
         */
        SemaphoreHandle_t handle;

        /**
         *  We do not want a Semaphore ctor. This class should never be
         *  directly created, this is a base class only.
         */
        Semaphore();
};


/**
 *  Wrapper class for Binary Semaphores.
 */
class BinarySemaphore : public Semaphore {

    /////////////////////////////////////////////////////////////////////////
    //
    //  Public API
    //
    /////////////////////////////////////////////////////////////////////////
    public:
        /**
         *  Constructor to create a binary semaphore.
         *
         *  @param set Is this semaphore "full" or not?
         *  @throws SemaphoreCreateException on failure.
         *  @return Instance of a BinarySemaphore.
         */
        explicit BinarySemaphore(bool set = false);
};


/**
 *  Wrapper class for Counting Semaphores.
 */
class CountingSemaphore : public Semaphore {

    /////////////////////////////////////////////////////////////////////////
    //
    //  Public API
    //
    /////////////////////////////////////////////////////////////////////////
    public:
        /**
         *  Constructor to create a counting semaphore.
         *  This ctor throws a SemaphoreCreateException on failure.
         *
         *  @param maxCount Must be greater than 0.
         *  @param initialCount Must not be greater than maxCount.
         *  @throws SemaphoreCreateException on failure.
         *  @return Instance of a CountingSemaphore.
         */
        CountingSemaphore(UBaseType_t maxCount, UBaseType_t initialCount);
};


}
#endif
