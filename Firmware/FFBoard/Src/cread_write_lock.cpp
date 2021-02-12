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
#include <read_write_lock.hpp>


using namespace cpp_freertos;


ReadWriteLock::ReadWriteLock()
    : ReadCount(0)
{
    ReadLock = xSemaphoreCreateMutex();
    if (ReadLock == NULL) {
#ifndef CPP_FREERTOS_NO_EXCEPTIONS
        throw ReadWriteLockCreateException();
#else
        configASSERT(!"ReadWriteLock Constructor Failed");
#endif
    }

    //
    //  ResourceLock CANNOT be a mutex. In FreeRTOS, as in most OS's,
    //  a thread is not allowed to unlock another thread's mutex. But
    //  the very nature of a Reader Lock allows an arbitrary ordering 
    //  of unlocks when multiple threads hold the reader lock. 
    //  Semaphores are not subject to this constraint.
    //
    ResourceLock = xSemaphoreCreateBinary();
    if (ResourceLock == NULL) {
        vSemaphoreDelete(ReadLock);
#ifndef CPP_FREERTOS_NO_EXCEPTIONS
        throw ReadWriteLockCreateException();
#else
        configASSERT(!"ReadWriteLock Constructor Failed");
#endif
    }

    //
    //  Initialize it as "full", so it behaves similar to a mutex.
    //
    xSemaphoreGive(ResourceLock);
}


ReadWriteLock::~ReadWriteLock()
{
    vSemaphoreDelete(ReadLock);
    vSemaphoreDelete(ResourceLock);
}


void ReadWriteLockPreferReader::ReaderLock()
{
    xSemaphoreTake(ReadLock, portMAX_DELAY);

    ReadCount++;
    if (ReadCount == 1) {
        xSemaphoreTake(ResourceLock, portMAX_DELAY);
    }

    xSemaphoreGive(ReadLock);
}


void ReadWriteLockPreferReader::ReaderUnlock()
{
    xSemaphoreTake(ReadLock, portMAX_DELAY);

    ReadCount--;
    if (ReadCount == 0) {
        xSemaphoreGive(ResourceLock);
    }

    xSemaphoreGive(ReadLock);
}


void ReadWriteLockPreferReader::WriterLock()
{
    xSemaphoreTake(ResourceLock, portMAX_DELAY);
}


void ReadWriteLockPreferReader::WriterUnlock()
{
    xSemaphoreGive(ResourceLock);
}


ReadWriteLockPreferWriter::ReadWriteLockPreferWriter()
    : ReadWriteLock(),
      WriteCount(0)
{
    WriteLock = xSemaphoreCreateMutex();
    if (WriteLock == NULL) {
#ifndef CPP_FREERTOS_NO_EXCEPTIONS
        throw ReadWriteLockCreateException();
#else
        configASSERT(!"ReadWriteLockPreferWriter Constructor Failed");
#endif
    }

    //
    //  BlockReadersLock CANNOT be a mutex. In FreeRTOS, as in most OS's,
    //  a thread is not allowed to unlock another thread's mutex. But
    //  the very nature of a Reader Lock allows an arbitrary ordering 
    //  of unlocks when multiple threads hold the reader lock. 
    //  Semaphores are not subject to this constraint.
    //
    BlockReadersLock = xSemaphoreCreateBinary();
    if (BlockReadersLock == NULL) {
        vSemaphoreDelete(WriteLock);
#ifndef CPP_FREERTOS_NO_EXCEPTIONS
        throw ReadWriteLockCreateException();
#else
        configASSERT(!"ReadWriteLockPreferWriter Constructor Failed");
#endif
    }

    //
    //  Initialize it as "full", so it behaves similar to a mutex.
    //
    xSemaphoreGive(BlockReadersLock);
}


ReadWriteLockPreferWriter::~ReadWriteLockPreferWriter()
{
    vSemaphoreDelete(WriteLock);
    vSemaphoreDelete(BlockReadersLock);
}


void ReadWriteLockPreferWriter::ReaderLock()
{
    xSemaphoreTake(BlockReadersLock, portMAX_DELAY);
    xSemaphoreTake(ReadLock, portMAX_DELAY);

    ReadCount++;
    if (ReadCount == 1) {
        xSemaphoreTake(ResourceLock, portMAX_DELAY);
    }

    xSemaphoreGive(ReadLock);
    xSemaphoreGive(BlockReadersLock);
}


void ReadWriteLockPreferWriter::ReaderUnlock()
{
    xSemaphoreTake(ReadLock, portMAX_DELAY);

    ReadCount--;
    if (ReadCount == 0) {
        xSemaphoreGive(ResourceLock);
    }

    xSemaphoreGive(ReadLock);
}


void ReadWriteLockPreferWriter::WriterLock()
{
    xSemaphoreTake(WriteLock, portMAX_DELAY);

    WriteCount++;
    if (WriteCount == 1) {
        xSemaphoreTake(BlockReadersLock, portMAX_DELAY);
    }

    xSemaphoreGive(WriteLock);

    xSemaphoreTake(ResourceLock, portMAX_DELAY);
}


void ReadWriteLockPreferWriter::WriterUnlock()
{
    xSemaphoreGive(ResourceLock);

    xSemaphoreTake(WriteLock, portMAX_DELAY);

    WriteCount--;
    if (WriteCount == 0) {
        xSemaphoreGive(BlockReadersLock);
    }

    xSemaphoreGive(WriteLock);
}
