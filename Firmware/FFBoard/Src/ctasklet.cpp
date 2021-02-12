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
#include "tasklet.hpp"


using namespace cpp_freertos;


Tasklet::Tasklet()
{
    DtorLock = xSemaphoreCreateBinary();

    if (DtorLock == NULL) {
#ifndef CPP_FREERTOS_NO_EXCEPTIONS
        throw TaskletCreateException("Create DtorLock Failed");
#else
        configASSERT(!"Tasklet Constructor Failed");
#endif
    }

    xSemaphoreGive(DtorLock);
}


Tasklet::~Tasklet()
{
}


void Tasklet::CheckForSafeDelete()
{
    xSemaphoreTake(DtorLock, portMAX_DELAY);
    vSemaphoreDelete(DtorLock);
}


void Tasklet::TaskletAdapterFunction(void *reference, uint32_t parameter)
{
    Tasklet *tasklet = static_cast<Tasklet *>(reference);
    tasklet->Run(parameter);
    xSemaphoreGive(tasklet->DtorLock);
}


bool Tasklet::Schedule( uint32_t parameter,
                        TickType_t CmdTimeout)
{
    BaseType_t rc;

    xSemaphoreTake(DtorLock, portMAX_DELAY);

    rc = xTimerPendFunctionCall(TaskletAdapterFunction,
                                this,
                                parameter,
                                CmdTimeout);

    if (rc == pdPASS) {
        return true;
    }
    else {
        xSemaphoreGive(DtorLock);
        return false;
    }
}


bool Tasklet::ScheduleFromISR(  uint32_t parameter,
                                BaseType_t *pxHigherPriorityTaskWoken)
{
    BaseType_t rc;

    rc = xSemaphoreTakeFromISR(DtorLock, pxHigherPriorityTaskWoken);

    if (rc != pdTRUE) {
        return false;
    }
    
    rc = xTimerPendFunctionCallFromISR( TaskletAdapterFunction,
                                        this,
                                        parameter,
                                        pxHigherPriorityTaskWoken);

    if (rc == pdPASS) {
        return true;
    }
    else {
        xSemaphoreGive(DtorLock);
        return false;
    }
}

