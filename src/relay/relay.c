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


static int readData(int fd, size_t size);

static int overload_process(Relay r);

/**
 * Initializes the relay
 * @see relay.h
 */
Relay relay_init(Buffer* b,
                 char* server_source,
                 char* backup_source,
                 int verbose) {
  Relay   r; /* Relay struct to create */
  int  s_fd; /* fd for server communication */
  int  b_fd; /* fd for SD card communication */

  if (verbose)
    printf("[R] Initializing relay...\n");
  if ((fd = open(server_source, O_RDWR)) < 0) {
    printf("[R] failed to establish communication with server");
    perror(server_source);
    return NULL;
  }
  if ((fd = open(backup_source, O_RDWR)) < 0) {
    printf("[R] failed to establish communication with backup");
    perror(backup_source);
    return NULL;
  }

  r = (Relay) malloc(sizeof(struct relay_st));
  r->buffers = b;
  r->server_fd = fd;
  r->verbose = verbose;
  if (verbose)
    printf("[R] Relay initialized!\n");
 
  return r;
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
int relay_process(Relay r) {
  /* Step 1: check sd card */
    /* Anything there? Spawn a thread */
    /* High error counter, more threads? */

  /* Step 2: check buffer */
    /* Anything there? send PAYLOAD_SIZE bytes to server */
  return -1;
}

/**
 * Free the relay handle and clean up for shutdown
 * @see relay.h
 */
void relay_cleanup(Relay* r) {
  if ((*r)->verbose)
    printf("[R] Consumer clean up...\n");

  if (close((*r)->server_fd) < 0) {
    printf("[R] ");
    perror("close");
    return;
  }

  if ((*r)->verbose) {
    free((void *)handle);
    printf("[R] Relay destroyed!\n");
  }
  free((void *)handle);
  *r = NULL;
}


static int readData(int fd, size_t size) {
  return -1;
}

static int overload_process(Relay r) {
  return -1;
}
