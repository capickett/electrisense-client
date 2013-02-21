/**
 * @file relay.c
 * Implementation of the network relay for the carambola client 
 * @see relay.h
 *
 * @authors Larson, Patrick; Pickett, Cameron
 */

#include "./relay.h"
#include "../shared/shared.h"

#define PAYLOAD_SIZE 1024

/**
 * Initializes the relay
 * @see relay.h
 */
Relay relay_init() {
  return NULL;
}

/**
 * Perform one unit of work
 * The following constitutes one unit of work:
 *   - Ensure buffer is not being backed up to SD card,
 *     and if it is, spawn a thread to handle it
 *   - Take up to PAYLOAD_SIZE bytes from the buffer
 *     and send it to a remote server
 * @see relay.h
 */
int relay_process(Relay handle) {
  return -1;
}

/**
 * Free the relay handle and clean up for shutdown
 * @see relay.h
 */
void relay_cleanup(Relay* handle) {
  free((void *)handle);
}
