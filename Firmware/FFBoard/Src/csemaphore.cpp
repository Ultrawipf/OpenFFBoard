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
#include <semaphore.hpp>


using namespace cpp_freertos;


bool Semaphore::Take(TickType_t xBlockTime)
{
    BaseType_t success;

    success = xSemaphoreTake(handle, xBlockTime);

    return success == pdTRUE ? true : false;
}


bool Semaphore::TakeFromISR(BaseType_t *pxHigherPriorityTaskWoken)
{
    BaseType_t success;

    success = xSemaphoreTakeFromISR(handle, pxHigherPriorityTaskWoken);

    return success == pdTRUE ? true : false;
}


bool Semaphore::Give()
{
    BaseType_t success;

    success = xSemaphoreGive(handle);

    return success == pdTRUE ? true : false;
}


bool Semaphore::GiveFromISR(BaseType_t *pxHigherPriorityTaskWoken)
{
    BaseType_t success;

    success = xSemaphoreGiveFromISR(handle, pxHigherPriorityTaskWoken);

    return success == pdTRUE ? true : false;
}


Semaphore::Semaphore()
{
}


Semaphore::~Semaphore()
{
    vSemaphoreDelete(handle);
}


BinarySemaphore::BinarySemaphore(bool set)
{
    handle = xSemaphoreCreateBinary();

    if (handle == NULL) {
#ifndef CPP_FREERTOS_NO_EXCEPTIONS
        throw SemaphoreCreateException();
#else
        configASSERT(!"BinarySemaphore Constructor Failed");
#endif
    }

    if (set) {
        xSemaphoreGive(handle);
    }
}


CountingSemaphore::CountingSemaphore(UBaseType_t maxCount, UBaseType_t initialCount)
{
    if (maxCount == 0) {
#ifndef CPP_FREERTOS_NO_EXCEPTIONS
        throw SemaphoreCreateException("bad maxCount");
#else
        configASSERT(!"CountingSemaphore Constructor bad maxCount");
#endif
    }

    if (initialCount > maxCount) {
#ifndef CPP_FREERTOS_NO_EXCEPTIONS
        throw SemaphoreCreateException("bad initialCount");
#else
        configASSERT(!"CountingSemaphore Constructor bad initialCount");
#endif
    }

    handle = xSemaphoreCreateCounting(maxCount, initialCount);

    if (handle == NULL) {
#ifndef CPP_FREERTOS_NO_EXCEPTIONS
        throw SemaphoreCreateException();
#else
        configASSERT(!"CountingSemaphore Constructor Failed");
#endif
    }
}


