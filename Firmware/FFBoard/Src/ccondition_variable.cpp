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
#include <condition_variable.hpp>
#include "thread.hpp"


#ifdef CPP_FREERTOS_CONDITION_VARIABLES


using namespace cpp_freertos;


ConditionVariable::ConditionVariable()
    : Lock(), WaitList()
{
}


void ConditionVariable::AddToWaitList(Thread *thread)
{
    //
    //  Lock the internal condition variable state
    //
    Lock.Lock();

    //
    //  Add the thread to the condition variable wait list.
    //
    WaitList.push_back(thread);

    //
    //  Drop the internal condition variable lock.
    //
    Lock.Unlock();
}


void ConditionVariable::Signal()
{
    //
    //  Lock the internal condition variable state
    //
    Lock.Lock();

    if ( !WaitList.empty() ) {

        Thread *thr = WaitList.front();
        WaitList.pop_front();
        thr->Signal();
    }

    //
    //  Drop the internal condition variable lock.
    //
    Lock.Unlock();
}


void ConditionVariable::Broadcast()
{
    //
    //  Lock the internal condition variable state
    //
    Lock.Lock();

    while ( !WaitList.empty() ) {

        Thread *thr = WaitList.front();
        WaitList.pop_front();
        thr->Signal();
    }

    //
    //  Drop the internal condition variable lock.
    //
    Lock.Unlock();
}

#endif

