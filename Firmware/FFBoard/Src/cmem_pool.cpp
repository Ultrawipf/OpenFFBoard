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
#include <stdlib.h>
#include "mem_pool.hpp"


using namespace cpp_freertos;


void MemoryPool::CalculateValidAlignment()
{
    /**
     *  Guarantee that the alignment is the size of a pointer.
     */
    if (Alignment < (int)sizeof(unsigned char *)) {
        Alignment = (int)sizeof(unsigned char *);
    }

    int alignmentBit = 0x1;
    int i;

    for (i = 0; i < 31; i++) {
        if (Alignment == alignmentBit) {
            break;
        }
        alignmentBit <<= 1;
    }

    if (i >= 31) {
#ifndef CPP_FREERTOS_NO_EXCEPTIONS
        throw MemoryPoolBadAlignmentException();
#else
        configASSERT(!"MemoryPool Bad Alignment");
#endif
    }
}


void MemoryPool::CalculateItemSize()
{
    if (ItemSize <= Alignment) {

        ItemSize = Alignment;
    }
    else {

        int alignmentCount = ItemSize / Alignment;
        if (ItemSize % Alignment != 0) {
            alignmentCount++;
        }

        ItemSize = alignmentCount * Alignment;
    }
}


MemoryPool::MemoryPool( int itemSize,
                        int itemCount,
                        int alignment)
    : ItemSize(itemSize),
      Alignment(alignment)
{
    CalculateValidAlignment();

    CalculateItemSize();

    unsigned char *address = (unsigned char *)malloc(ItemSize * itemCount);

    if (address == NULL) {
#ifndef CPP_FREERTOS_NO_EXCEPTIONS
        throw MemoryPoolMallocException();
#else
        configASSERT(!"MemoryPool malloc Failed");
#endif
    }

    for (int i = 0; i < itemCount; i++) {
        FreeItems.push_back(address);
        address += ItemSize;
    }

    Lock = new MutexStandard();
}


MemoryPool::MemoryPool( int itemSize,
                        void *preallocatedMemory,
                        int preallocatedMemorySize,
                        int alignment)
    : ItemSize(itemSize),
      Alignment(alignment)
{
    CalculateValidAlignment();

    CalculateItemSize();

    unsigned char *address = (unsigned char *)preallocatedMemory;

    while (preallocatedMemorySize >= ItemSize) {

        FreeItems.push_back(address);
        address += ItemSize;
        preallocatedMemorySize -= ItemSize;
    }

    Lock = new MutexStandard();
}


void *MemoryPool::Allocate()
{
    LockGuard guard(*Lock);

    if (FreeItems.empty())
        return NULL;

    void *item = FreeItems.front();
    FreeItems.pop_front();

    return item;
}


void MemoryPool::Free(void *item)
{
    LockGuard guard(*Lock);
    FreeItems.push_back(item);
}


void MemoryPool::AddMemory(int itemCount)
{
    unsigned char *address = (unsigned char *)malloc(ItemSize * itemCount);

    if (address == NULL) {
#ifndef CPP_FREERTOS_NO_EXCEPTIONS
        throw MemoryPoolMallocException();
#else
        configASSERT(!"MemoryPool AddMemory Failed");
#endif
    }

    for (int i = 0; i < itemCount; i++) {

        LockGuard guard(*Lock);

        FreeItems.push_back(address);
        address += ItemSize;
    }
}


void MemoryPool::AddMemory( void *preallocatedMemory,
                            int preallocatedMemorySize)
{
    unsigned char *address = (unsigned char *)preallocatedMemory;

    while (preallocatedMemorySize >= ItemSize) {

        LockGuard guard(*Lock);

        FreeItems.push_back(address);
        address += ItemSize;
        preallocatedMemorySize -= ItemSize;
    }
}


