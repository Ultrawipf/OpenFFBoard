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
#include "queue.hpp"


using namespace cpp_freertos;


Queue::Queue(UBaseType_t maxItems, UBaseType_t itemSize)
{
    handle = xQueueCreate(maxItems, itemSize);

    if (handle == NULL) {
#ifndef CPP_FREERTOS_NO_EXCEPTIONS
        throw QueueCreateException();
#else
        configASSERT(!"Queue Constructor Failed");
#endif
    }
}


Queue::~Queue()
{
    vQueueDelete(handle);
}


bool Queue::Enqueue(void *item)
{
    BaseType_t success;

    success = xQueueSendToBack(handle, item, portMAX_DELAY);

    return success == pdTRUE ? true : false;
}


bool Queue::Enqueue(void *item, TickType_t Timeout)
{
    BaseType_t success;

    success = xQueueSendToBack(handle, item, Timeout);

    return success == pdTRUE ? true : false;
}


bool Queue::Dequeue(void *item, TickType_t Timeout)
{
    BaseType_t success;

    success = xQueueReceive(handle, item, Timeout);

    return success == pdTRUE ? true : false;
}


bool Queue::Peek(void *item, TickType_t Timeout)
{
    BaseType_t success;

    success = xQueuePeek(handle, item, Timeout);

    return success == pdTRUE ? true : false;
}


bool Queue::EnqueueFromISR(void *item, BaseType_t *pxHigherPriorityTaskWoken)
{
    BaseType_t success;

    success = xQueueSendToBackFromISR(handle, item, pxHigherPriorityTaskWoken);

    return success == pdTRUE ? true : false;
}


bool Queue::DequeueFromISR(void *item, BaseType_t *pxHigherPriorityTaskWoken)
{
    BaseType_t success;

    success = xQueueReceiveFromISR(handle, item, pxHigherPriorityTaskWoken);

    return success == pdTRUE ? true : false;
}


bool Queue::PeekFromISR(void *item)
{
    BaseType_t success;

    success = xQueuePeekFromISR(handle, item);

    return success == pdTRUE ? true : false;
}


bool Queue::IsEmpty()
{
    UBaseType_t cnt = uxQueueMessagesWaiting(handle);

    return cnt == 0 ? true : false;
}


bool Queue::IsFull()
{
    UBaseType_t cnt = uxQueueSpacesAvailable(handle);

    return cnt == 0 ? true : false;
}


void Queue::Flush()
{
    xQueueReset(handle);
}


UBaseType_t Queue::NumItems()
{
    return uxQueueMessagesWaiting(handle);
}


UBaseType_t Queue::NumSpacesLeft()
{
    return uxQueueSpacesAvailable(handle);
}


Deque::Deque(UBaseType_t maxItems, UBaseType_t itemSize)
    : Queue(maxItems, itemSize)
{
}


bool Deque::EnqueueToFront(void *item, TickType_t Timeout)
{
    BaseType_t success;

    success = xQueueSendToFront(handle, item, Timeout);

    return success == pdTRUE ? true : false;
}


bool Deque::EnqueueToFrontFromISR(void *item, BaseType_t *pxHigherPriorityTaskWoken)
{
    BaseType_t success;

    success = xQueueSendToFrontFromISR(handle, item, pxHigherPriorityTaskWoken);

    return success == pdTRUE ? true : false;
}


BinaryQueue::BinaryQueue(UBaseType_t itemSize)
    : Queue(1, itemSize)
{
}


bool BinaryQueue::Enqueue(void *item)
{
    (void)xQueueOverwrite(handle, item);
    return true;
}


bool BinaryQueue::EnqueueFromISR(void *item, BaseType_t *pxHigherPriorityTaskWoken)
{
    (void)xQueueOverwriteFromISR(handle, item, pxHigherPriorityTaskWoken);
    return true;
}
