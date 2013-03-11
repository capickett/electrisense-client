/**
 * @file consumer.h
 * The data retrieval component of the carambola client
 *
 * The consumer is a realtime process designed to handle all the communication
 * with the Firefly's microprocessor. Its task is singular: to move any data
 * available in the micro's data buffer to a larger buffer located on the
 * Carambola. This buffer, described in buffer.h, is shared between the
 * consumer and relay such that the relay may take data in that buffer and
 * process it. Because of the memory constraints of the microprocessor, the
 * consumer must always be able to read data in the micro's buffer at a
 * consistent fast pace. For this reason, the consumer does little additional
 * processing. In the case of the relay process dying, the consumer will handle
 * restarting the relay and keeping the data safe on the large SD card storage
 * medium.
 *
 * @authors Larson, Patrick; Pickett, Cameron
 */

#ifndef _CONSUMER_CONSUMER_H
#define _CONSUMER_CONSUMER_H

/* This is the public header file, all interface related details belong here */

#include "../shared/buffer.h"

struct consumer_st {
  /* Any operational parameters go here */
  Buffer *buffers; /**< A pointer to two buffers that make the double buffer */
  CURL *curl; /**< Server notify curl */
  char *server_url;
  char *dump_path; /**< The path to the external buffer dump */
  int buf_idx; /**< The current buffer in use by the consumer */
  int data_fd; /**< A file descriptor for the source of data */
  int err_count; /**< A count of the times consumer has written to ext_fd */
  int verbose; /**< A flag to enable verbose console output */
};

/**
 * A handle used to store any operational parameters of the consumer.
 */
typedef struct consumer_st Consumer;

/**
 * Initializes the consumer and returns a handle to the configured consumer
 * instance.
 *
 * Returns NULL in the event of initialization failure.
 *
 * @param b A pointer to the shared double buffer.
 * @param data_source A string of a valid URI to the source of data for the
 * consumer to read from. 
 * @param ext_dump A string of a valid URI to the location the consumer will
 * use in the case that it needs to dump one or more buffers
 * @param verbose Enable verbose output from consumer
 *
 * @return A malloc'd handle to be used for all future calls to to the consumer
 * interface. The handle contains all configuration details necessary for the
 * relay to process data. In the event that the consumer is stoppped, it is the
 * caller's responsibility to free the Consumer handler by calling
 * #consumer_cleanup.
 */
Consumer* consumer_init(Buffer* b, char *server_url, char* data_source,
    char* ext_dump, int verbose);

/**
 * Perform one unit of work.
 *
 * This function is the main function called by the consumer driver in order to
 * perform its task of reading data from the micro. This function is meant
 * to be called in a loop. For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.c}
 * Consumer handle = consumer_init();
 * while(1) {
 *   if (consumer_process(handle) < 0)
 *     break;
 * }
 * consumer_cleanup(&handle);
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Only performing one "unit" of work allows for the driver to interrupt the
 * process for whatever reason, or cleanup in the event of a failure.
 * 
 * @param handle The handle containing all necessary configuration to perform
 * the consumer's task. Caller must call #consumer_init before this function.
 * @return 0 if successful, -1 if there is an error
 * @see #consumer_init
 */
int consumer_process(Consumer *handle);

/**
 * Frees the consumer handle and performs any additional cleanup required to
 * shut down the consumer. The specified handle will be NULL after this
 * function returns.
 *
 * @param handle The handle to be freed
 */
void consumer_cleanup(Consumer **handle);

#endif
