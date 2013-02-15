/**
 * @file consumer.c
 *
 * Consumer implementation
 * @authors Larson, Patrick; Pickett, Cameron
 */

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "consumer.h"


Consumer consumer_init(Buffer* b, char* data_source, int verbose) {
  Consumer c; /* Consumer struct to create */
  int     fd; /* fd for the data_source */

  if (verbose)
    printf("[C] Initializing consumer...\n");
  if ((fd = open(data_source, O_RDWR | O_NOCTTY | O_SYNC)) < 0) {
    printf("[C] ");
    perror(data_source);
    return NULL;
  }
  if (verbose)
    printf("[C] Data source opened. fd = %d\n", fd);

  c = (Consumer) malloc(sizeof(consumer_st));
  c->buffers = b;
  c->data_fd = fd;
  c->verbose = verbose;
  if (verbose)
    printf("[C] Consumer initialized!\n");
}

int consumer_process(Consumer c) {
  /* Step 1: read from data source */
  
  /* Step 2: check if buffer can fit */
    /* Can fit? Fill buffer */
    /* Partially? Fill buffer then... */
    /* Can't fit? Check if other buffer is empty */
      /* Empty? switch buffer, start filling */
      /* Not empty? Write to SD, incremement error counter */

}

void consumer_cleanup(Consumer* c) {
  if ((*c)->verbose)
    printf("[C] Consumer clean up...\n");

  if (close((*c)->data_fd) < 0) {
    printf("[C] ");
    perror("close");
    return;
  }

  if ((*c)->verbose)
    printf("[C] Consumer destroyed!\n");
  *c = NULL;
}
