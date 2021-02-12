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


#ifndef TICKS_HPP_
#define TICKS_HPP_

#include "FreeRTOS.h"
#include "task.h"


namespace cpp_freertos {


/**
 *  Class encapsulating FreeRTOS idea of a tick.
 */
class Ticks {

    public:
        /**
         *  Get the current tick count.
         *
         *  @return Current tick count.
         */
        static inline TickType_t GetTicks()
        {
            return xTaskGetTickCount();
        }

        /**
         *  Get the current tick count from ISR context.
         *
         *  @return Current tick count.
         */
        static inline TickType_t GetTicksFromISR()
        {
            return xTaskGetTickCountFromISR();
        }

        /**
         *  Convert from ticks to ms.
         *
         *  @param ticks ticks to convert.
         *  @return milliseconds.
         */
        static inline TickType_t TicksToMs(TickType_t ticks)
        {
            return ticks * portTICK_PERIOD_MS;
        }

        /**
         *  Convert from ms to ticks.
         *
         *  @param milliseconds milliseconds to convert.
         *  @return ticks
         */
        static inline TickType_t MsToTicks(TickType_t milliseconds)
        {
            return milliseconds / portTICK_PERIOD_MS;
        }

        /**
         *  Convert from seconds to ticks.
         *
         *  @param seconds seconds to convert.
         *  @return ticks
         */
        static inline TickType_t SecondsToTicks(TickType_t seconds)
        {
            return (seconds * 1000) / portTICK_PERIOD_MS;
        }
};


}
#endif

