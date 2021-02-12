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


#ifndef WORK_QUEUE_HPP_
#define WORK_QUEUE_HPP_

#include "thread.hpp"
#include "queue.hpp"
#include "semaphore.hpp"


namespace cpp_freertos {


#define DEFAULT_MAX_WORK_ITEMS          10
#define DEFAULT_WORK_QUEUE_STACK_SIZE   (configMINIMAL_STACK_SIZE * 2)
#define DEFAULT_WORK_QUEUE_PRIORITY     (tskIDLE_PRIORITY + 1)


/**
 *  This class encapsulates the idea of a discrete, non-repeating task.
 *  Create a WorkItem when there is something you need to do on a different
 *  Thread, but doesn't have to happen periodically. This is a great 
 *  construct for one off fire and forget tasks.
 *
 *  This is an abstract base class.
 *  To use this, you need to subclass it. All of your WorkItems should
 *  be derived from this class. Then implement the virtual Run
 *  function. This is a similar design to Java threading.
 */
class WorkItem {

    /////////////////////////////////////////////////////////////////////////
    //
    //  Public API
    //
    /////////////////////////////////////////////////////////////////////////
    public:

        /**
         *  Our constructor.
         *
         *  @param freeAfterComplete If you pass in a true, you are 
         *  requesing the WorkQueue itself to delete this WorkItem after
         *  it has run it. 
         *  @note Only set freeAfterComplete = true if:
         *  1) You dynamically allocated it (i.e. used "new")
         *  2) After you call QueueWork() you promise never to touch 
         *     this object again. 
         */
        WorkItem(bool freeAfterComplete = false);

        /**
         *  Our destructor.
         */
        virtual ~WorkItem();
        
        /**
         *  Allows a client to decide if this WorkItem is marked
         *  for automatic deletion.
         */
        bool FreeAfterRun();

        /**
         *  Implementation of your actual WorkItem function.
         *  You must override this function.
         */
        virtual void Run() = 0;

    /////////////////////////////////////////////////////////////////////////
    //
    //  Private API
    //  The internals of this wrapper class.
    //
    /////////////////////////////////////////////////////////////////////////
    private:
        /**
         *  Designates whether this WorkItem should be deleted 
         *  after the WorkQueue has run it.
         */
        const bool FreeItemAfterCompleted;
};


/**
 *  This class is the "engine" for WorkItems. Create one or more WorkQueues
 *  to accept WorkItems. WorkQueues pull WorkItems off of a FIFO queue and 
 *  run them sequentially.
 */
class WorkQueue {

    /////////////////////////////////////////////////////////////////////////
    //
    //  Public API
    //
    /////////////////////////////////////////////////////////////////////////
    public:
        /**
         *  Constructor to create a named WorkQueue.
         *
         *  @throws ThreadCreateException, QueueCreateException, 
         *          SemaphoreCreateException
         *  @param Name Name of the thread internal to the WorkQueue. 
         *         Only useful for debugging.
         *  @param StackDepth Number of "words" allocated for the Thread stack.
         *  @param Priority FreeRTOS priority of this Thread.
         *  @param MaxWorkItems Maximum number of WorkItems this WorkQueue can hold.
         */
        WorkQueue(  const char * const Name,
                    uint16_t StackDepth = DEFAULT_WORK_QUEUE_STACK_SIZE,
                    UBaseType_t Priority = DEFAULT_WORK_QUEUE_PRIORITY,
                    UBaseType_t MaxWorkItems = DEFAULT_MAX_WORK_ITEMS);

        /**
         *  Constructor to create an unnamed WorkQueue.
         *
         *  @throws ThreadCreateException, QueueCreateException, 
         *          SemaphoreCreateException
         *  @param StackDepth Number of "words" allocated for the Thread stack.
         *  @param Priority FreeRTOS priority of this Thread.
         *  @param MaxWorkItems Maximum number of WorkItems this WorkQueue can hold.
         */
        WorkQueue(  uint16_t StackDepth = DEFAULT_WORK_QUEUE_STACK_SIZE,
                    UBaseType_t Priority = DEFAULT_WORK_QUEUE_PRIORITY,
                    UBaseType_t MaxWorkItems = DEFAULT_MAX_WORK_ITEMS);

#if (INCLUDE_vTaskDelete == 1)
        /**
         *  Our destructor.
         *
         *  @note Given the multithreaded nature of this class, the dtor 
         *  may block until the underlying Thread has had a chance to 
         *  clean up.
         */
        ~WorkQueue();
#else
//
//  If we are using C++11 or later, take advantage of the 
//  newer features to find bugs.
//
#if __cplusplus >= 201103L
        /**
         *  If we can't delete a task, it makes no sense to have a
         *  destructor.
         */
        ~WorkQueue() = delete;
#endif
#endif

        /**
         *  Send a WorkItem off to be executed.
         *
         *  @param work Pointer to a WorkItem.
         *  @return true if it was queued, false otherwise.
         *  @note This function may block if the WorkQueue is presently full.
         */ 
        bool QueueWork(WorkItem *work);

    /////////////////////////////////////////////////////////////////////////
    //
    //  Private API
    //  The internals of this class.
    //
    /////////////////////////////////////////////////////////////////////////
    private:

        /**
         *  An internal derived Thread class, in which we do our real work.
         */
        class CWorkerThread : public Thread {

            public:
                CWorkerThread(  const char * const Name,
                                uint16_t StackDepth,
                                UBaseType_t Priority,
                                WorkQueue *Parent);

                CWorkerThread(  uint16_t StackDepth,
                                UBaseType_t Priority,
                                WorkQueue *Parent);

                virtual ~CWorkerThread();

            protected:
                virtual void Run();

            private:
                const WorkQueue *ParentWorkQueue;
        };
        
        /**
         *  Pointer to our WorkerThread.
         */
        CWorkerThread *WorkerThread;

        /**
         *  Pointer to our work queue itself.
         */
        Queue *WorkItemQueue;

        /**
         *  Semaphore to support deconstruction without race conditions.
         */
        BinarySemaphore *ThreadComplete;
};


}
#endif


