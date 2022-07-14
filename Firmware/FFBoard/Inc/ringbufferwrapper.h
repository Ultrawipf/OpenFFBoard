#ifndef __RING_BUFFER_WRAPPER_H__
#define __RING_BUFFER_WRAPPER_H__

#include <stdint.h>
#include <type_traits>

#include <ringbuffer.h>

class RingBufferWrapper {

public:

   /*
   * Warning: Size must be multiple of 2.
   */
   explicit RingBufferWrapper(uint8_t* data, uint32_t len) noexcept; 
   explicit RingBufferWrapper(uint32_t size) noexcept;
   virtual ~RingBufferWrapper() noexcept;

   bool isValid() noexcept;

   bool empty() noexcept;
   size_t length() noexcept;
   size_t freeSpace() noexcept;
   size_t capacity() noexcept;

   size_t appendOne(uint8_t data) noexcept;
   size_t appendMultiple(const uint8_t *data, size_t len) noexcept;

   size_t peekOne(uint8_t* data) noexcept;
   size_t getOne(uint8_t* data) noexcept;

   size_t getMultiple(uint8_t *dst, size_t len) noexcept;
   size_t peekMultiple(uint8_t *dst, size_t len) noexcept;

   size_t discardMultiple(size_t len) noexcept;
   void clean() noexcept;

   template<typename T>
   T get_as(bool* ok) noexcept;

   template<typename T>
   T peek_as(bool* ok) noexcept;

   template<typename T>
   size_t numElements() noexcept;

private:

   RingBuffer buffer;
   bool valid;
};

template<typename T>
T RingBufferWrapper::peek_as(bool* ok) noexcept 
{
   T data;
   // Only POD types can be trivially copied from
   // the ring buffer.
   if (!std::is_pod<T>::value) {
      *ok = false;
      return data;
   }

   if (length() < sizeof(T))
   {
      *ok = false;
      return data;
   }

   size_t len = peekMultiple(static_cast<uint8_t*>(&data), sizeof(T));
   /* 
    * This should be always true because we verified the
    * buffer length before reading.
    */ 
   *ok = len == sizeof(T);
   return data;
}

template<typename T>
T RingBufferWrapper::get_as(bool* ok) noexcept
{  
   T data = peek_as<T>(ok);
   if (*ok) {
      discardMultiple(sizeof(T));
   }
   return data;
}

template<typename T>
size_t RingBufferWrapper::numElements() noexcept
{  
   return length() / sizeof(T);
}
   

#endif
