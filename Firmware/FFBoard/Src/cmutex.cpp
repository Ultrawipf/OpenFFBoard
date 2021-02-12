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

#include "FreeRTOS.h"
#include "mutex.hpp"


using namespace cpp_freertos;


Mutex::Mutex()
{
}


Mutex::~Mutex()
{
    vSemaphoreDelete(handle);
}


MutexStandard::MutexStandard()
{
    handle = xSemaphoreCreateMutex();

    if (handle == NULL) {
#ifndef CPP_FREERTOS_NO_EXCEPTIONS
        throw MutexCreateException();
#else
        configASSERT(!"Mutex Constructor Failed");
#endif
    }
}


bool MutexStandard::Lock(TickType_t Timeout)
{
    BaseType_t success = xSemaphoreTake(handle, Timeout);
    return success == pdTRUE ? true : false;
}


bool MutexStandard::Unlock()
{
    BaseType_t success = xSemaphoreGive(handle);
    return success == pdTRUE ? true : false;
}


#if (configUSE_RECURSIVE_MUTEXES == 1)

MutexRecursive::MutexRecursive()
{
    handle = xSemaphoreCreateRecursiveMutex();

    if (handle == NULL) {
#ifndef CPP_FREERTOS_NO_EXCEPTIONS
        throw MutexCreateException();
#else
        configASSERT(!"Mutex Constructor Failed");
#endif
    }
}


bool MutexRecursive::Lock(TickType_t Timeout)
{
    BaseType_t success = xSemaphoreTakeRecursive(handle, Timeout);
    return success == pdTRUE ? true : false;
}


bool MutexRecursive::Unlock()
{
    BaseType_t success = xSemaphoreGiveRecursive(handle);
    return success == pdTRUE ? true : false;
}

#endif


LockGuard::LockGuard(Mutex& m)
    : mutex(m)
{
    mutex.Lock();
}


LockGuard::~LockGuard()
{
    mutex.Unlock();
}
