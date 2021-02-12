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


#ifndef TICK_HOOK_HPP_
#define TICK_HOOK_HPP_

#include "FreeRTOS.h"
#include "task.h"
#include <list>

#if ( configUSE_TICK_HOOK == 1 )

/**
 *  FreeRTOS expects this function to exist and requires it to be 
 *  named as such with the following signature.
 */
extern "C" void vApplicationTickHook(void);

namespace cpp_freertos {

/**
 *  Wrapper class for Tick hooks, functions you want to run within 
 *  the tick ISR. 
 *
 *  This is an abstract base class.
 *  To use this, you need to subclass it. All of your tick functions 
 *  should be derived from this class. Then implement the virtual Run
 *  function. 
 *
 *  You can register multiple hooks with this class. The order of 
 *  execution should not be assumed. All tick hooks will execute 
 *  every tick.
 */    
class TickHook {

    /////////////////////////////////////////////////////////////////////////
    //
    //  Public API
    //  Available from anywhere. 
    //
    /////////////////////////////////////////////////////////////////////////
    public:
        /**
         *  Constructor.
         */
        TickHook();

        /**
         *  Destructor
         */
        virtual ~TickHook();

        /**
         *  After this is called your Run routine will execute in the 
         *  Tick ISR. This registration cannot be done in the base class 
         *  constructor. Once your object is fully constructed, you "may"
         *  call this in your derived class's constructor.
         *  @note Immedately after you call this function, your TickHook
         *  Run() method will run, perhaps before you even return from this 
         *  call. You "must" be ready to run before you call Register().
         */
        void Register();
        
        /**
         *  Disable the tick hook from running, without removing it 
         *  from the tick hook list.
         */
        void Disable();

        /**
         *  Enable this Idle Hook to run. This call is not necessary
         *  if you haven't called Disable.
         */
        void Enable();
        

    /////////////////////////////////////////////////////////////////////////
    //
    //  Protected API
    //
    /////////////////////////////////////////////////////////////////////////
    protected:
        /**
         *  Implementation of your actual Tick Hook code.
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
         *  List of Tick Hook callbacks that are executed in the 
         *  Tick ISR.
         */
        static std::list<TickHook *>Callbacks;

        /**
         *  Should the tick hook run?
         */
        bool Enabled;

    /**
     *  Allow the global vApplicationTickHook() function access
     *  to the internals of this class. This simplifies the overall
     *  design.
     */
    friend void ::vApplicationTickHook();
};



}
#endif
#endif


