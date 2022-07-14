#ifndef __RING_BUFFER_H_
#define __RING_BUFFER_H_

// Uncomment if your platform does not have memcopy implementation.
// #define NO_MEM_COPY

#include <stddef.h>
#include <stdint.h>


#ifdef  __cplusplus
extern  "C" {
#endif

typedef struct {
   size_t tail;
   size_t head;
   size_t sizeMask;
   uint8_t *data;
}RingBuffer;

// Init Data Structure.
int ringBufferInit(RingBuffer *buffer, uint8_t *data, size_t len);

// Returns ring buffer len.
size_t ringBufferLen(const RingBuffer *buffer);

// Returns whether the ringbuferr is empty.
uint8_t ringBufferEmpty(const RingBuffer *buffer);

// Available space left in the ringbuffer.
size_t ringBufferFreeSpace(const RingBuffer *buffer);

// Maximum number of bytes this ring buffer can store.
size_t ringBufferMaxSize(const RingBuffer *buffer);

// Append one byte to the buffer.
void ringBufferAppendOne(RingBuffer *buffer, uint8_t data);

// Append multiple bytest to the buffer.
void ringBufferAppendMultiple(RingBuffer *buffer, const uint8_t *data, size_t len);

// Return the first element but don't remove it.
uint8_t ringBufferPeekOne(const RingBuffer *buffer);

// Return the first element and remove it from the buffer.
uint8_t ringBufferGetOne(RingBuffer *buffer);

// Return len number of bytes from the ring buffer.
void ringBufferGetMultiple(RingBuffer *buffer, uint8_t *dst, size_t len);

// Return len number of bytes without removing from the buffer.
void ringBufferPeekMultiple(const RingBuffer *buffer, uint8_t *dst, size_t len);

// Discard len bytes from the buffer.
void ringBufferDiscardMultiple(RingBuffer *buffer, size_t len);

// Clean the buffer to th initial state.
void ringBufferClear(RingBuffer *buffer);

static inline int isMultipleTwo(size_t len)
{
   if(!(len && !(len & (len - 1)))) {
      return 0;
   } else {
      return 1;
   }
}

#ifdef  __cplusplus
      }
#endif

#endif
