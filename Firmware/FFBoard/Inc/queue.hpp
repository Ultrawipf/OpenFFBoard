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


#ifndef QUEUE_HPP_
#define QUEUE_HPP_

/**
 *  C++ exceptions are used by default when constructors fail.
 *  If you do not want this behavior, define the following in your makefile
 *  or project. Note that in most / all cases when a constructor fails,
 *  it's a fatal error. In the cases when you've defined this, the new 
 *  default behavior will be to issue a configASSERT() instead.
 */
#ifndef CPP_FREERTOS_NO_EXCEPTIONS
#include <exception>
#include <cstdio>
#include <string>
#ifdef CPP_FREERTOS_NO_CPP_STRINGS
#error "FreeRTOS-Addons require C++ Strings if you are using exceptions"
#endif
#endif
#include "FreeRTOS.h"
#include "queue.h"


namespace cpp_freertos {


#ifndef CPP_FREERTOS_NO_EXCEPTIONS
/**
 *  This is the exception that is thrown if a Queue constructor fails.
 */
class QueueCreateException : public std::exception {

    public:
        /**
         *  Create the exception.
         */
        QueueCreateException()
        {
            sprintf(errorString, "Queue Constructor Failed");
        }

        /**
         *  Create the exception.
         */
        explicit QueueCreateException(const char *info)
        {
            snprintf(errorString, sizeof(errorString),
                        "Queue Constructor Failed %s", info);
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
 *  Queue class wrapper for FreeRTOS queues. This class provides enqueue
 *  and dequeue operations.
 *
 *  @note It is expected that an application will instantiate this class or
 *        one of the derived classes and use that. It is not expected that
 *        a user or application will derive from these classes.
 */
class Queue {

    /////////////////////////////////////////////////////////////////////////
    //
    //  Public API
    //
    /////////////////////////////////////////////////////////////////////////
    public:
        /**
         *  Our constructor.
         *
         *  @throws QueueCreateException
         *  @param maxItems Maximum number of items this queue can hold.
         *  @param itemSize Size of an item in a queue.
         *  @note FreeRTOS queues use a memcpy / fixed size scheme for queues.
         */
        Queue(UBaseType_t maxItems, UBaseType_t itemSize);

        /**
         *  Our destructor.
         */
        virtual ~Queue();

        /**
         *  Add an item to the back of the queue.
         *
         *  @param item The item you are adding.
         *  @return true if the item was added, false if it was not.
         */
        virtual bool Enqueue(void *item);

        /**
         *  Add an item to the back of the queue.
         *
         *  @param item The item you are adding.
         *  @param Timeout How long to wait to add the item to the queue if
         *         the queue is currently full.
         *  @return true if the item was added, false if it was not.
         */
        virtual bool Enqueue(void *item, TickType_t Timeout);

        /**
         *  Remove an item from the front of the queue.
         *
         *  @param item Where the item you are removing will be returned to.
         *  @param Timeout How long to wait to remove an item if the queue
         *         is currently empty.
         *  @return true if an item was removed, false if no item was removed.
         */
        bool Dequeue(void *item, TickType_t Timeout = portMAX_DELAY);

        /**
         *  Make a copy of an item from the front of the queue. This will
         *  not remove it from the head of the queue.
         *
         *  @param item Where the item you are removing will be returned to.
         *  @param Timeout How long to wait to remove an item if the queue
         *         is currently empty.
         *  @return true if an item was copied, false if no item was copied.
         */
        bool Peek(void *item, TickType_t Timeout = portMAX_DELAY);

        /**
         *  Add an item to the back of the queue in ISR context.
         *
         *  @param item The item you are adding.
         *  @param pxHigherPriorityTaskWoken Did this operation result in a
         *         rescheduling event.
         *  @return true if the item was added, false if it was not.
         */
        virtual bool EnqueueFromISR(void *item, BaseType_t *pxHigherPriorityTaskWoken);

        /**
         *  Remove an item from the front of the queue in ISR context.
         *
         *  @param item Where the item you are removing will be returned to.
         *  @param pxHigherPriorityTaskWoken Did this operation result in a
         *         rescheduling event.
         *  @return true if an item was removed, false if no item was removed.
         */
        bool DequeueFromISR(void *item, BaseType_t *pxHigherPriorityTaskWoken);

        /**
         *  Make a copy of an item from the front of the queue. This will
         *  not remove it from the head of the queue.
         *
         *  @param item Where the item you are removing will be returned to.
         *  @return true if an item was copied, false if no item was copied.
         */
        bool PeekFromISR(void *item);

        /**
         *  Is the queue empty?
         *  @return true if the queue was empty when this was called, false if
         *  the queue was not empty.
         */
        bool IsEmpty();

        /**
         *  Is the queue full?
         *  @return true if the queue was full when this was called, false if
         *  the queue was not full.
         */
        bool IsFull();

        /**
         *  Remove all objects from the queue.
         */
        void Flush();

        /**
         *  How many items are currently in the queue.
         *  @return the number of items in the queue.
         */
        UBaseType_t NumItems();

        /**
         *  How many empty spaves are currently left in the queue.
         *  @return the number of remaining spaces.
         */
        UBaseType_t NumSpacesLeft();

    /////////////////////////////////////////////////////////////////////////
    //
    //  Protected API
    //  Not intended for use by application code.
    //
    /////////////////////////////////////////////////////////////////////////
    protected:
        /**
         *  FreeRTOS queue handle.
         */
        QueueHandle_t handle;
};


/**
 *  Enhanced queue class that implements a double ended queue (a "deque"),
 *  almost. Unlike the traditional CommSci version, there is no way to
 *  dequeue from the back. Practically, this most likely isn't a big deal.
 *
 *  @note It is expected that an application will instantiate this class or
 *        one of the derived classes and use that. It is not expected that
 *        a user or application will derive from these classes.
 */
class Deque : public Queue {

    /////////////////////////////////////////////////////////////////////////
    //
    //  Public API
    //
    /////////////////////////////////////////////////////////////////////////
    public:
        /**
         *  Our constructor.
         *
         *  @throws QueueCreateException
         *  @param maxItems Maximum number of items thsi queue can hold.
         *  @param itemSize Size of an item in a queue.
         *  @note FreeRTOS queues use a memcpy / fixed size scheme for queues.
         */
        Deque(UBaseType_t maxItems, UBaseType_t itemSize);

        /**
         *  Add an item to the front of the queue. This will result in
         *  the item being removed first, ahead of all of the items
         *  added by the base calss Dequeue() function.
         *
         *  @param item The item you are adding.
         *  @param Timeout How long to wait to add the item to the queue if
         *         the queue is currently full.
         *  @return true if the item was added, false if it was not.
         */
        bool EnqueueToFront(void *item, TickType_t Timeout = portMAX_DELAY);

        /**
         *  Add an item to the front of the queue. This will result in
         *  the item being removed first, ahead of all of the items
         *  added by the base calss Dequeue() function.
         *
         *  @param item The item you are adding.
         *  @param pxHigherPriorityTaskWoken Did this operation result in a
         *         rescheduling event.
         *  @return true if the item was added, false if it was not.
         */
        bool EnqueueToFrontFromISR(void *item, BaseType_t *pxHigherPriorityTaskWoken);
};


/**
 *  Binary queue with overwrite. This queue can only hold one item.
 *  If sucessive Enqueue operations are called, that item is overwritten
 *  with whatever the last item was.
 *
 *  @note It is expected that an application will instantiate this class or
 *        one of the derived classes and use that. It is not expected that
 *        a user or application will derive from these classes.
 */
class BinaryQueue : public Queue {

    /////////////////////////////////////////////////////////////////////////
    //
    //  Public API
    //
    /////////////////////////////////////////////////////////////////////////
    public:
        /**
         *  Our constructor.
         *
         *  @throws QueueCreateException
         *  @param itemSize Size of an item in a queue.
         *  @note FreeRTOS queues use a memcpy / fixed size scheme for queues.
         */
        explicit BinaryQueue(UBaseType_t itemSize);

         /**
          *  Add an item to the queue.
          *
          *  @param item The item you are adding.
          *  @return true always, because of overwrite.
          */
        virtual bool Enqueue(void *item);

         /**
          *  Add an item to the queue in ISR context.
          *
          *  @param item The item you are adding.
          *  @param pxHigherPriorityTaskWoken Did this operation result in a
          *         rescheduling event.
          *  @return true always, because of overwrite.
          */
        virtual bool EnqueueFromISR(void *item, BaseType_t *pxHigherPriorityTaskWoken);
};


}
#endif
