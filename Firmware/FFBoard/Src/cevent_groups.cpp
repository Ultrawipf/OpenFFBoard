/****************************************************************************
 *
 *  Copyright (c) 2017, Danilo Pucci Smokovitz (dnlps@hotmail.com)
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
#include <event_groups.hpp>
using namespace cpp_freertos;

EventGroup::EventGroup()
{
    handle = xEventGroupCreate();

    if (handle == NULL) {
#ifndef CPP_FREERTOS_NO_EXCEPTIONS
        throw EventGroupCreateException();
#else
        configASSERT(!"EventGroup Constructor Failed");
#endif
    }

}


#if( configSUPPORT_STATIC_ALLOCATION == 1 )

EventGroup::EventGroup(StaticEventGroup_t *pxEventGroupBuffer)
{
    handle = xEventGroupCreateStatic(pxEventGroupBuffer);

    if (handle == NULL) {
#ifndef CPP_FREERTOS_NO_EXCEPTIONS
        throw EventGroupCreateException();
#else
        configASSERT(!"EventGroup Constructor Failed");
#endif
    }
}

#endif /* configSUPPORT_STATIC_ALLOCATION */


EventGroup::~EventGroup()
{
    vEventGroupDelete(handle);
}


EventBits_t EventGroup::Sync(   const EventBits_t uxBitsToSet,
                                const EventBits_t uxBitsToWaitFor,
                                TickType_t xTicksToWait)
{

    return xEventGroupSync( handle,
                            uxBitsToSet,
                            uxBitsToWaitFor,
                            xTicksToWait);

}


EventBits_t EventGroup::WaitBits(   const EventBits_t uxBitsToWaitFor,
                                    bool xClearOnExit,
                                    bool xWaitForAllBits,
                                    TickType_t xTicksToWait)
{

    return xEventGroupWaitBits( handle,
                                uxBitsToWaitFor,
                                xClearOnExit ? pdTRUE : pdFALSE,
                                xWaitForAllBits ? pdTRUE : pdFALSE,
                                xTicksToWait);
}


EventBits_t EventGroup::ClearBits(const EventBits_t uxBitsToClear)
{
    return xEventGroupClearBits(handle, uxBitsToClear);
}


BaseType_t EventGroup::ClearBitsFromISR(const EventBits_t uxBitsToClear)
{
    return xEventGroupClearBitsFromISR(handle, uxBitsToClear);
}


EventBits_t EventGroup::GetBits()
{
    return xEventGroupGetBits(handle);
}


EventBits_t EventGroup::GetBitsFromISR()
{
    return xEventGroupGetBitsFromISR(handle);
}


EventBits_t EventGroup::SetBits(const EventBits_t uxBitsToSet)
{
    return xEventGroupSetBits(handle, uxBitsToSet);
}


#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( INCLUDE_xTimerPendFunctionCall == 1 ) && ( configUSE_TIMERS == 1 ) )

BaseType_t EventGroup::SetBitsFromISR(const EventBits_t uxBitsToSet, BaseType_t *pxHigherPriorityTaskWoken)
{
    return xEventGroupSetBitsFromISR(handle, uxBitsToSet, pxHigherPriorityTaskWoken);
}

#endif


