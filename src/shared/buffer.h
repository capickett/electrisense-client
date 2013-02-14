/**
 * @file buffer.h
 * Definition for the shared buffer used by the consumer and relay.
 *
 * The memory shared between the consumer and relay consists of a double buffer
 * with status fields for each buffer. To begin, both buffers are set to be
 * empty. The consumer will then fill begin filling an empty buffer. When the
 * buffer is full, the #ST_BUFFER_EMPTY flag shall be unset for that buffer.
 * This must happen *after* any interaction with the buffer is complete, or else
 * a race condition could occur. The relay will find any full buffer and begin
 * to empty it. When this buffer is empty, the buffer empty flag shall be reset
 * for that buffer. Again, this must happen *after* any interaction with the
 * buffer is complete, or else a race condition could occur.
 */

#ifndef _SHARED_BUFFER_H
#define _SHARED_BUFFER_H

/** The size of each buffer, including the status header. */
#define __BUFFER_SIZE 102400

/**
 * A buffer with status header. 
 *
 * The first byte contains the status flags associated with the buffer. At this
 * moment, the only flag available is #ST_BUFFER_EMPTY, which signifies where
 * the buffer is empty and ready for new data or not.
 */
struct buffer_st {
  char status;
  char data[__BUFFER_SIZE-1];
};

typedef struct buffer Buffer;

/** Buffer empty flag. If set, the buffer is empty and ready to be filled.  */
#define ST_BUFFER_EMPTY 1

#endif
