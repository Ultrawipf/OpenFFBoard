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


#ifndef MEM_POOL_HPP_
#define MEM_POOL_HPP_

/**
 *  C++ exceptions are used by default when constructors fail.
 *  If you do not want this behavior, define the following in your makefile
 *  or project. Note that in most / all cases when a constructor fails,
 *  it's a fatal error. In the cases when you've defined this, the new
 *  default behavior will be to issue a configASSERT() instead.
 */
#ifndef CPP_FREERTOS_NO_EXCEPTIONS
#include <exception>
#include <string>
#include <cstdio>
#ifdef CPP_FREERTOS_NO_CPP_STRINGS
#error "FreeRTOS-Addons require C++ Strings if you are using exceptions"
#endif
#endif
#include <list>
#include "FreeRTOS.h"
#include "mutex.hpp"

namespace cpp_freertos {


#ifndef CPP_FREERTOS_NO_EXCEPTIONS
/**
 *  This is the exception that is thrown if a MemoryPool malloc fails.
 */
class MemoryPoolMallocException : public std::exception {

    public:
        /**
         *  Create the exception.
         */
        MemoryPoolMallocException()
        {
            sprintf(errorString, "MemoryPool malloc Failed");
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

/**
 *  This is the exception that is thrown if the Alignment argument
 *  is invalid.
 */
class MemoryPoolBadAlignmentException : public std::exception {

    public:
        /**
         *  Create the exception.
         */
        MemoryPoolBadAlignmentException()
        {
            sprintf(errorString, "MemoryPool Bad Alignment");
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
 *  Memory Pools are fixed size allocations to prevent fragmentation.
 *
 *  This is a new feature to FreeRTOS Wrappers and is not in and of
 *  itself a wrapper.
 *
 *  Memory Pools are thread safe, but cannot be used in ISR context.
 *  The OS must be running, because these use Mutexes to protect internal
 *  data structures.
 */
class MemoryPool {

    /////////////////////////////////////////////////////////////////////////
    //
    //  Public API
    //
    /////////////////////////////////////////////////////////////////////////
    public:

        /**
         *  Constructor to create a Memory Pool.
         *
         *  This constructor uses the system malloc to actually obtain
         *  the memory.
         *
         *  @param itemSize How big is each item you want to allocate.
         *  @param itemCount How many items max do you want to allocate
         *      at once.
         *  @param Alignment Power of 2 value denoting on which address boundary the
         *      memory will be aligned to. Must be at least sizeof(unsigned char *).
         *  @throws MemoryPoolMallocException on failure.
         *  @throws MemoryPoolBadAlignmentException on failure.
         */
        MemoryPool( int itemSize,
                    int itemCount,
                    int alignment);

        /**
         *  Constructor to create a Memory Pool.
         *
         *  This constructor uses memory you pass in to actually create
         *  the pool. This constructor does not throw.
         *
         *  @param itemSize How big is each item you want to allocate.
         *  @param preallocatedMemory Pointer to the preallocated memory
         *  you are dedicating to this pool.
         *  @param preallocatedMemorySize How big is the buffer you are
         *  passing in.
         *  @param Alignment Power of 2 value denoting on which address boundary the
         *      memory will be aligned to. Must be at least sizeof(unsigned char *).
         *  @throws MemoryPoolBadAlignmentException on failure.
         */
        MemoryPool( int itemSize,
                    void *preallocatedMemory,
                    int preallocatedMemorySize,
                    int alignment);

        /**
         *  Allows you to add memory to a MemoryPool.
         *
         *  Items will be the same size as you initially asked for.
         *
         *  @param itemCount How many more items max do you want to allocate
         *  @throws MemoryPoolMallocException on failure.
         */
        void AddMemory(int itemCount);

        /**
         *  Allows you to add memory to a MemoryPool.
         *
         *  Items will be the same size as you initially asked for.
         *
         *  @param preallocatedMemory Pointer to the preallocated memory
         *  you are dedicating to this pool.
         *  @param preallocatedMemorySize How big is the buffer you are
         *  passing in.
         */
        void AddMemory( void *preallocatedMemory,
                        int preallocatedMemorySize);

        /**
         *  Allocate an item from the pool.
         *
         *  @return Pointer of the memory or NULL if the pool is empty.
         */
        void *Allocate();

        /**
         *  Returns the item back to it's pool.
         *
         *  @note There is no checking that the item is actually
         *  valid to be returned to this pool.
         */
        void Free(void *item);

    /////////////////////////////////////////////////////////////////////////
    //
    //  Private API
    //  The internals of this class.
    //
    /////////////////////////////////////////////////////////////////////////
    private:

        /**
         *  Standard Mutex to allow thread safety.
         */
        Mutex *Lock;

        /**
         *  Save the item size for additions.
         */
        int ItemSize;

        /**
         *  The overall alignment of an item.
         */
        int Alignment;

        /**
         *  All of the real work is done with STL lists.
         */
        std::list<void *>FreeItems;

        /**
         *  Adjusts and validates the alignment argument
         *  passed in the ctor.
         */
        void CalculateValidAlignment();

        /**
         *  Calculate the true item size, based on alignment.
         */
        void CalculateItemSize();

//
//  If we are using C++11 or later, take advantage of the
//  newer features to find bugs.
//
#if __cplusplus >= 201103L
        /**
         *  To correctly delete a Memory Pool, we'd have to guarantee that
         *  all allocations had been returned to us. We side step this issue
         *  as well as all the associated overhead with supporting this by
         *  not allowing destructors.
         */
        ~MemoryPool() = delete;
#else
        /**
         *  To correctly delete a Memory Pool, we'd have to guarantee that
         *  all allocations had been returned to us. We side step this issue
         *  by making the destructor private so it can't be accessed.
         */
        ~MemoryPool();
#endif


};


}

#endif

