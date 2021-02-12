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


#ifndef READ_WRITE_LOCK_HPP_
#define READ_WRITE_LOCK_HPP_


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
 *  This is the exception that is thrown if a ReadWriteLock constructor fails.
 */
class ReadWriteLockCreateException : public std::exception {

    public:
        /**
         *  Create the exception.
         */
        ReadWriteLockCreateException()
        {
            sprintf(errorString, "ReadWriteLock Constructor Failed");
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
 *  Abstract base class encapsulating a Reader/Writer lock.
 *
 *  These locks are based on mutexs and cannot be used in any way from
 *  ISR context. Likewise, these locks block indefinitely.
 *
 *  @note It is expected that an application will instantiate one of the
 *        derived classes and use that object for synchronization. It is
 *        not expected that a user or application will derive from these
 *        classes.
 */
class ReadWriteLock {

    /////////////////////////////////////////////////////////////////////////
    //
    //  Public API
    //
    /////////////////////////////////////////////////////////////////////////
    public:
        /**
         *  Constructor
         *
         *  @throws ReadWriteLockCreateException on failure.
         */
        ReadWriteLock();

        /**
         *  Destructor
         */
        virtual ~ReadWriteLock();

        /**
         *  Take the lock as a Reader.
         *  This allows multiple reader access.
         */
        virtual void ReaderLock() = 0;

        /**
         *  Unlock the Reader.
         */
        virtual void ReaderUnlock() = 0;

        /**
         *  Take the lock as a Writer.
         *  This allows only one thread access.
         */
        virtual void WriterLock() = 0;

        /**
         *  Unlock the Writer.
         */
        virtual void WriterUnlock() = 0;

    /////////////////////////////////////////////////////////////////////////
    //
    //  Protected API
    //  Not intended for use by application code.
    //
    /////////////////////////////////////////////////////////////////////////
    protected:
        /**
         *  How many active readers are there.
         */
        int ReadCount;

        /**
         *  Protect ReadCount.
         */
        SemaphoreHandle_t ReadLock;

        /**
         *  Protect this resource from multiple writer access, or
         *  from Reader access when a writer is changing something.
         */
        SemaphoreHandle_t ResourceLock;
};


/**
 *  Concrete derived class that implements a Reader/Writer lock
 *  that favors the Readers. That is, with enough aggressive readers,
 *  a Writer may starve.
 */
class ReadWriteLockPreferReader : public ReadWriteLock {

    /////////////////////////////////////////////////////////////////////////
    //
    //  Public API
    //
    /////////////////////////////////////////////////////////////////////////
    public:
        /**
         *  Take the lock as a Reader.
         *  This allows multiple reader access.
         */
        virtual void ReaderLock();

        /**
         *  Unlock the Reader.
         */
        virtual void ReaderUnlock();

        /**
         *  Take the lock as a Writer.
         *  This allows only one thread access.
         */
        virtual void WriterLock();

        /**
         *  Unlock the Writer.
         */
        virtual void WriterUnlock();
};


/**
 *  Concrete derived class that implements a Reader/Writer lock
 *  that favors the Writers. That is, with enough aggressive writers,
 *  a Reader may starve.
 */
class ReadWriteLockPreferWriter : public ReadWriteLock {

    /////////////////////////////////////////////////////////////////////////
    //
    //  Public API
    //
    /////////////////////////////////////////////////////////////////////////
    public:
        /**
         *  Our derived constructor.
         */
        ReadWriteLockPreferWriter();

        /**
         *  Our derived destructor.
         */
        virtual ~ReadWriteLockPreferWriter();

        /**
         *  Take the lock as a Reader.
         *  This allows multiple reader access.
         */
        virtual void ReaderLock();

        /**
         *  Unlock the Reader.
         */
        virtual void ReaderUnlock();

        /**
         *  Take the lock as a Writer.
         *  This allows only one thread access.
         */
        virtual void WriterLock();

        /**
         *  Unlock the Writer.
         */
        virtual void WriterUnlock();

    /////////////////////////////////////////////////////////////////////////
    //
    //  Private API
    //  The internals of this wrapper class.
    //
    /////////////////////////////////////////////////////////////////////////
    private:
        /**
         *  Number of Writers waiting for the Resource Lock, including any
         *  current Writer already holdign it.
         */
        int WriteCount;

        /**
         *  Protect WriteCount.
         */
        SemaphoreHandle_t WriteLock;

        /**
         *  Lock to stop reader threads from starving a Writer.
         */
        SemaphoreHandle_t BlockReadersLock;
};


}
#endif
