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


#ifndef CONDITION_VARIABLE_HPP_
#define CONDITION_VARIABLE_HPP_

#include <list>
#include "mutex.hpp"


/**
 *  Condition variables are an additon to the FreeRTOS C++ Wrapper
 *  classes. If you want to include them, you need to define the 
 *  following in your makefile or project.
 */
#ifdef CPP_FREERTOS_CONDITION_VARIABLES


namespace cpp_freertos {

//
//  Forward declaration. We need to prevent a circular dependency
//  between the Thread class and the ConditionVariable class.
//
class Thread;


/**
 *  Class implementation of condition variable based on
 *  FreeRTOS C++ Wrapper classes.
 *  
 *  A condition variable isn't really a variable. It's a list
 *  of threads.
 *
 *  The design here is that a Thread "waits", and a ConditionVariable
 *  "signals". This affects where the public interfaces reside.
 */
class ConditionVariable {

    /////////////////////////////////////////////////////////////////////////
    //
    //  Public API
    //
    /////////////////////////////////////////////////////////////////////////
    public:
    
        /**
         *  Constructor to create a condition variable.
         */
        ConditionVariable();

        /**
         *  Signal a thread waiting on this ConditionVariable.
         *  Signaling is implemented as FIFO.
         */
        void Signal();

        /**
         *  Signal all threads waiting on this ConditionVariable.
         */
        void Broadcast();


    /////////////////////////////////////////////////////////////////////////
    //
    //  Private API
    //  The internals of this wrapper class.
    //
    /////////////////////////////////////////////////////////////////////////
    private:

        /**
         *  Protect the internal ConditionVariable state.
         */
        MutexStandard Lock;

        /**
         *  Implementation of a wait list of Threads.
         */
        std::list<Thread *> WaitList;

        /**
         *  Internal helper function to queue a Thread to 
         *  this ConditionVariable's wait list.
         */
        void AddToWaitList(Thread *thread);

    /**
     *  The Thread class and the ConditionVariable class are interdependent.
     *  If we allow the Thread class to access the internals of the
     *  ConditionVariable, we can reduce the public interface which is a
     *  good thing.
     */
    friend class Thread;
};


}

#endif

#endif

