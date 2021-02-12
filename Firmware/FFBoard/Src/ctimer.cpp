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
#include "timer.hpp"


using namespace cpp_freertos;


Timer::Timer(   const char * const TimerName,
                TickType_t PeriodInTicks,
                bool Periodic
                )
{
    handle = xTimerCreate(  TimerName,
                            PeriodInTicks,
                            Periodic ? pdTRUE : pdFALSE,
                            this,
                            TimerCallbackFunctionAdapter);

    if (handle == NULL) {
#ifndef CPP_FREERTOS_NO_EXCEPTIONS
        throw TimerCreateException();
#else
        configASSERT(!"Timer Constructor Failed");
#endif
    }
}


Timer::Timer(   TickType_t PeriodInTicks,
                bool Periodic
                )
{
    handle = xTimerCreate(  "Default",
                            PeriodInTicks,
                            Periodic ? pdTRUE : pdFALSE,
                            this,
                            TimerCallbackFunctionAdapter);

    if (handle == NULL) {
#ifndef CPP_FREERTOS_NO_EXCEPTIONS
        throw TimerCreateException();
#else
        configASSERT(!"Timer Constructor Failed");
#endif
    }
}


Timer::~Timer()
{
    xTimerDelete(handle, portMAX_DELAY);
}


bool Timer::IsActive()
{
    return xTimerIsTimerActive(handle) == pdFALSE ? false : true;
}


bool Timer::Start(TickType_t CmdTimeout)
{
    return xTimerStart(handle, CmdTimeout) == pdFALSE ? false : true;
}


bool Timer::StartFromISR(BaseType_t *pxHigherPriorityTaskWoken)
{
    return xTimerStartFromISR(handle, pxHigherPriorityTaskWoken) == pdFALSE
            ? false : true;
}


bool Timer::Stop(TickType_t CmdTimeout)
{
    return xTimerStop(handle, CmdTimeout) == pdFALSE ? false : true;
}


bool Timer::StopFromISR(BaseType_t *pxHigherPriorityTaskWoken)
{
    return xTimerStopFromISR(handle, pxHigherPriorityTaskWoken) == pdFALSE
            ? false : true;
}


bool Timer::Reset(TickType_t CmdTimeout)
{
    return xTimerReset(handle, CmdTimeout) == pdFALSE ? false : true;
}


bool Timer::ResetFromISR(BaseType_t *pxHigherPriorityTaskWoken)
{
    return xTimerResetFromISR(handle, pxHigherPriorityTaskWoken) == pdFALSE
            ? false : true;
}


bool Timer::SetPeriod(  TickType_t NewPeriod,
                        TickType_t CmdTimeout)
{
    return xTimerChangePeriod(handle, NewPeriod, CmdTimeout) == pdFALSE
            ? false : true;
}


bool Timer::SetPeriodFromISR(   TickType_t NewPeriod,
                                BaseType_t *pxHigherPriorityTaskWoken)
{
    return xTimerChangePeriodFromISR(   handle, NewPeriod,
                                        pxHigherPriorityTaskWoken) == pdFALSE
            ? false : true;
}


#if (INCLUDE_xTimerGetTimerDaemonTaskHandle == 1)

TaskHandle_t Timer::GetTimerDaemonHandle()
{
    return xTimerGetTimerDaemonTaskHandle();
}

#endif


void Timer::TimerCallbackFunctionAdapter(TimerHandle_t xTimer)
{
    Timer *timer = static_cast<Timer *>(pvTimerGetTimerID(xTimer));
    timer->Run();
}
