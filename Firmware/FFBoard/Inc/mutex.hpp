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


#ifndef MUTEX_HPP_
#define MUTEX_HPP_

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
 *  This is the exception that is thrown if a Mutex constructor fails.
 */
class MutexCreateException : public std::exception {

    public:
        /**
         *  Create the exception.
         */
        MutexCreateException()
        {
            sprintf(errorString, "Mutex Constructor Failed");
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
 *  Base wrapper class around FreeRTOS's implementation of mutexes.
 *
 *  By definition, Mutexes can @em NOT be used from ISR contexts.
 *
 *  @note It is expected that an application will instantiate one of the
 *        derived classes and use that object for synchronization. It is
 *        not expected that a user or application will derive from these
 *        classes.
 */
class Mutex {

    /////////////////////////////////////////////////////////////////////////
    //
    //  Public API
    //
    /////////////////////////////////////////////////////////////////////////
    public:
        /**
         *  Lock the Mutex.
         *
         *  Each type of Mutex implements it's own locking code as per the
         *  FreeRTOS API.
         *
         *  @param Timeout How long to wait to get the Lock until giving up.
         *  @return true if the Lock was acquired, false if it timed out.
         */
        virtual bool Lock(TickType_t Timeout = portMAX_DELAY) = 0;

        /**
         *  Unlock the Mutex.
         *
         *  @return true if the Lock was released, false if it failed. (Hint,
         *           if it fails, did you call Lock() first?)
         */
        virtual bool Unlock() = 0;

        /**
         *  Our destructor
         */
        virtual ~Mutex();

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
         *  This constructor should not be public.
         */
        Mutex();
};


/**
 *  Standard usage Mutex.
 *  By default calls to Lock these objects block forever, but this can be
 *  changed by simply passing in a argument to the Lock() method.
 *  These objects are not recursively acquirable. Calling Lock() twice from
 *  the same Thread (i.e. task) will deadlock.
 *
 *  @note Standard mutexes use less resources than recursive mutexes. You
 *        should typically use this type of Mutex, unless you have a strong
 *        need for a MutexRecursive mutex.
 */
class MutexStandard : public Mutex {

    /////////////////////////////////////////////////////////////////////////
    //
    //  Public API
    //
    /////////////////////////////////////////////////////////////////////////
    public:
        /**
         *  Create a standard, non-recursize Mutex.
         *
         *  @throws ThreadMutexException on failure.
         */
        MutexStandard();

        /**
         *  Lock the Mutex.
         *
         *  @param Timeout How long to wait to get the Lock until giving up.
         *  @return true if the Lock was acquired, false if it timed out.
         */
        virtual bool Lock(TickType_t Timeout = portMAX_DELAY);

        /**
         *  Unlock the Mutex.
         *
         *  @return true if the Lock was released, false if it failed. (Hint,
         *           if it fails, did you call Lock() first?)
         */
        virtual bool Unlock();
};


#if (configUSE_RECURSIVE_MUTEXES == 1)

/**
 *  Recursive usage Mutex.
 *
 *  By default calls to Lock these objects block forever, but this can be
 *  changed by simply passing in a argument to the Lock() method.
 *  These objects are recursively acquirable. Calling Lock() twice from
 *  the same Thread (i.e. task) works fine. The caller just needs to be sure to
 *  call Unlock() as many times as Lock().
 *
 *  @note Recursive mutexes use more resources than standard mutexes. You
 *        should be sure that you actually need this type of synchronization
 *        before using it.
 */
class MutexRecursive : public Mutex {

    /////////////////////////////////////////////////////////////////////////
    //
    //  Public API
    //
    /////////////////////////////////////////////////////////////////////////
    public:
        /**
         *  Create a recursize Mutex.
         *
         *  @throws ThreadMutexException on failure.
         */
        MutexRecursive();

        /**
         *  Lock the Mutex.
         *
         *  @param Timeout How long to wait to get the Lock until giving up.
         *  @return true if the Lock was acquired, false if it timed out.
         */
        virtual bool Lock(TickType_t Timeout = portMAX_DELAY);

        /**
         *  Unlock the Mutex.
         *
         *  @return true if the Lock was released, false if it failed. (Hint,
         *           if it fails, did you call Lock() first?)
         */
        virtual bool Unlock();
};

#endif


/**
 *  Synchronization helper class that leverages the C++ language to help
 *  prevent deadlocks.
 *  This is a C++11 feature that allows Mutex Locking and Unlocking to behave
 *  following an RAII style. The constructor of this helper object locks the
 *  Mutex. The destructor unlocks the Mutex. Since C++ guarantees that an
 *  object's desctuctor is always called when it goes out of scope, calls to
 *  Unlock become unnecessary and are in fact guaranteed as long as correct
 *  scoping is used.
 */
class LockGuard {

    /////////////////////////////////////////////////////////////////////////
    //
    //  Public API
    //
    /////////////////////////////////////////////////////////////////////////
    public:
        /**
         *  Create a LockGuard with a specific Mutex.
         *
         *  @post The Mutex will be locked.
         *  @note There is an infinite timeout for acquiring the Lock.
         */
        explicit LockGuard(Mutex& m);

        /**
         *  Destroy a LockGuard.
         *
         *  @post The Mutex will be unlocked.
         */
        ~LockGuard();

        /////////////////////////////////////////////////////////////////////////
        //
        //  Private API
        //
        /////////////////////////////////////////////////////////////////////////
    private:
        /**
         *  We do not want a copy constructor.
         */
        LockGuard(const LockGuard&);

        /**
         *  Reference to the Mutex we locked, so it can be unlocked
         *  in the destructor.
         */
        Mutex& mutex;
};


}
#endif
