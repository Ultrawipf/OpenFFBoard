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
#include <workqueue.hpp>


using namespace cpp_freertos;


WorkItem::WorkItem(bool freeAfterComplete)
    : FreeItemAfterCompleted(freeAfterComplete)
{
}


WorkItem::~WorkItem()
{
}


bool WorkItem::FreeAfterRun()
{
    return FreeItemAfterCompleted;
}


WorkQueue::WorkQueue(   const char * const Name,
                        uint16_t StackDepth,
                        UBaseType_t Priority,
                        UBaseType_t maxWorkItems)
{
    //
    //  Build the Queue first, since the Thread is going to access 
    //  it as soon as it can, maybe before we leave this ctor.
    //
    WorkItemQueue = new Queue(maxWorkItems, sizeof(WorkItem *));
    ThreadComplete = new BinarySemaphore();
    WorkerThread = new CWorkerThread(Name, StackDepth, Priority, this);
    //
    //  Our ctor chain is complete, we can start.
    //
    WorkerThread->Start();
}


WorkQueue::WorkQueue(   uint16_t StackDepth,
                        UBaseType_t Priority,
                        UBaseType_t maxWorkItems)
{
    //
    //  Build the Queue first, since the Thread is going to access 
    //  it as soon as it can, maybe before we leave this ctor.
    //
    WorkItemQueue = new Queue(maxWorkItems, sizeof(WorkItem *));
    ThreadComplete = new BinarySemaphore();
    WorkerThread = new CWorkerThread(StackDepth, Priority, this);
    //
    //  Our ctor chain is complete, we can start.
    //
    WorkerThread->Start();
}


#if (INCLUDE_vTaskDelete == 1)

WorkQueue::~WorkQueue()
{
    //
    //  This dtor is tricky, because of the multiple objects in 
    //  play, and the multithreaded nature of this specific object.
    //

    //
    //  Note that we cannot flush the queue. If there are items 
    //  in the queue maked freeAfterComplete, we would leak the 
    //  memory. 
    //

    //
    //  Send a message that it's time to cleanup.
    //
    WorkItem *work = NULL;
    WorkItemQueue->Enqueue(&work);

    //
    //  Wait until the thread has run enough to signal that it's done.
    //
    ThreadComplete->Take();

    //
    //  Then delete the queue and thread. Order doesn't matter here.
    //
    delete WorkItemQueue;
    delete WorkerThread;
    delete ThreadComplete;
}

#endif


bool WorkQueue::QueueWork(WorkItem *work)
{
    return WorkItemQueue->Enqueue(&work);
}


WorkQueue::CWorkerThread::CWorkerThread(const char * const Name,
                                        uint16_t StackDepth,
                                        UBaseType_t Priority,
                                        WorkQueue *Parent)
    : Thread(Name, StackDepth, Priority), ParentWorkQueue(Parent)
{
}


WorkQueue::CWorkerThread::CWorkerThread(uint16_t StackDepth,
                                        UBaseType_t Priority,
                                        WorkQueue *Parent)
    : Thread(StackDepth, Priority), ParentWorkQueue(Parent)
{
}


WorkQueue::CWorkerThread::~CWorkerThread()
{
}


void WorkQueue::CWorkerThread::Run()
{
    while (true) {

        WorkItem *work;

        //
        //  Wait forever for work.
        //
        ParentWorkQueue->WorkItemQueue->Dequeue(&work);

        //
        //  If we dequeue a NULL item, its our sign to exit.
        //  We are being deconstructed.
        //
        if (work == NULL) {
            //
            //  Exit the task loop.
            //
            break;
        }

        //
        //  Else we have an item, run it.
        //
        work->Run();

        //
        //  If this was a dynamic, fire and forget item and we were 
        //  requested to clean it up, do so.
        //
        if (work->FreeAfterRun()) {
            delete work;
        }
    }

    //
    //  Signal the dtor that the thread is exiting.
    //
    ParentWorkQueue->ThreadComplete->Give();
}


