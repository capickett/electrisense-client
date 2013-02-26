/**
 * @file consumer.c
 *
 * Consumer implementation
 * @authors Larson, Patrick; Pickett, Cameron
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "consumer.h"
#include "../shared/buffer.h"

#define ERROR_LIMIT 10 /**< Number of sdcard writes before notifying server */

static size_t get_read_size(Consumer c);


Consumer consumer_init(Buffer* b, char* data_source, char* ext_dump, int verbose) {
  Consumer c;
  int      fd;
  int      ext_fd;
  char*    external_dump_path;
  
  if (verbose) printf("[C] Initializing consumer...\n");
  
  if ((fd = open(data_source, O_RDWR | O_NOCTTY | O_SYNC)) < 0)
    return NULL;
  
  if (verbose) printf("[C] Data source opened. (fd = %d)\n", fd);

  external_dump_path = (char*) malloc(strlen(ext_dump) + 12 + 6 + 1);
  strcpy(external_dump_path, ext_dump);
  strcat(external_dump_path, "client-dump_XXXXXX");
  
  if ((ext_fd = mkstemp(external_dump_path)) < 0)
    return NULL;

  free(external_dump_path);

  if (verbose) printf("[C] External dump file opened. (fd = %d)\n", ext_fd);

  c = (Consumer) malloc(sizeof(struct consumer_st));
  c->buffers = b;
  c->data_fd = fd;
  c->verbose = verbose;
  c->buf_idx = 0;
  c->ext_fd  = ext_fd;
  
  if (verbose) printf("[C] Consumer initialized!\n");

  return c;
}

int consumer_process(Consumer c) {
  ssize_t amount_read;
  size_t  amount_to_read = get_read_size(c);
  char*   tmp_buf        = (char*) malloc(amount_to_read);
  
  Buffer* cur_buf        = &c->buffers[c->buf_idx];
  size_t  buf_remaining  = cur_buf->capacity - cur_buf->size;
  char*   dest           = cur_buf->data + cur_buf->size;
  int     verbose        = c->verbose;

  /* Step 1: read from data source */
  /* TODO: Switch to USB tty when figured out */
  if (verbose) printf("[C] Reading %zu bytes\n", amount_to_read);
  
  while ((amount_read = read(c->data_fd, tmp_buf, amount_to_read)) < 0) {
    if (errno == EAGAIN || errno == EINTR)
      continue;

    perror("read");
    free(tmp_buf);
    return -1;
  }

  /* Step 2: check if buffer can fit */
  if (verbose) printf("[C] Checking buffers\n");
  
  if (amount_read <= buf_remaining) {
    /* Can fit? Fill buffer */
    if (verbose) printf("[C] Fits in buffer %d\n", c->buf_idx);
    
    memcpy(dest, tmp_buf, amount_read);
    cur_buf->size += amount_read;
  } else {
    
    if (buf_remaining > 0) {
      /* Partially fits? Fill buffer then... */
      if (verbose) printf("[C] Partially fits in buffer %d\n", c->buf_idx);
      
      amount_read -= buf_remaining;
      memcpy(dest, tmp_buf, buf_remaining);
      cur_buf->size = cur_buf->capacity; /* buffer is now full */
    }

    if (verbose) printf("[C] Switching buffers\n");

    /* Check if other buffer is empty */
    cur_buf = &c->buffers[c->buf_idx ^ 1];
    
    if (cur_buf->size == cur_buf->capacity) {
      /* Still full. Write cur buf to SD, incremement error counter */
      fprintf(stderr, "[C] Buffer %d still full! Dumping current buffer\n", c->buf_idx ^ 1);
      
      while (write(c->ext_fd, &c->buffers[c->buf_idx], sizeof(Buffer)) < 0) {
        if (errno == EAGAIN || errno == EINTR)
          continue;

        perror("write");
        return -1;
      }
      ++c->err_count;
      if (c->err_count == ERROR_LIMIT) {
        /* TODO: Notify server that we are writing to SD too much */
        fprintf(stderr, "[C] Error limit reached!\n");
        free(tmp_buf);
        return -1;
      }
    } else {
      /* Empty. Switch buffers and begin filling. */
      if (verbose) printf("[C] Fits in buffer %d\n", c->buf_idx ^ 1);
      c->buf_idx ^= 1;
      dest = cur_buf->data;
      memcpy(dest, tmp_buf, amount_read);
      cur_buf->size = amount_read;
    }
  }
  free(tmp_buf);
  return 0;
}

void consumer_cleanup(Consumer* c) {
  if ((*c)->verbose) printf("[C] Consumer clean up...\n");

  if (close((*c)->data_fd) < 0) {
    printf("[C] ");
    perror("close");
  }

  if (close((*c)->ext_fd) < 0) {
    printf("[C] ");
    perror("close");
  }

  if ((*c)->verbose) printf("[C] Consumer destroyed!\n");
  free(*c);
  *c = NULL;
}

/**
 * Gets the amount of data to be read by the consumer.
 *
 * @param c the consumer handle to look up the data source
 */
static size_t get_read_size(Consumer c) {
  return 1024;
}
