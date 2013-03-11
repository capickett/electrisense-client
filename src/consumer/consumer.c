/**
 * @file consumer.c
 *
 * Consumer implementation
 * @authors Larson, Patrick; Pickett, Cameron
 */

#include <curl/curl.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "consumer.h"
#include "../shared/buffer.h"

#define ERROR_LIMIT 10 /**< Number of sdcard writes before notifying server */

static size_t get_read_size(Consumer *c);
static int notify_server(Consumer *c);

Consumer* consumer_init(Buffer *b, char *server_url, char *data_source,
    char *ext_dump, int verbose) {
  Consumer *c;
  int fd;

  if (verbose)
    printf("[C] Initializing consumer...\n");
  if ((fd = open(data_source, O_RDWR | O_NOCTTY | O_SYNC)) < 0)
    return NULL ;
  if (verbose)
    printf("[C] Data source opened. (fd = %d)\n", fd);

  c = (Consumer*) malloc(sizeof(struct consumer_st));

  c->buffers = b;
  c->err_count = 0;
  c->data_fd = fd;
  c->verbose = verbose;
  c->buf_idx = 0;
  c->server_url = server_url;

  /* +2 for optional slash */
  c->dump_path = (char*) malloc(strlen(ext_dump) + 2);
  strcpy(c->dump_path, ext_dump);
  if (ext_dump[strlen(ext_dump) - 1] != '/')
    strcat(c->dump_path, "/");

  /* Curl initialization */
  CURL* curl;

  curl_global_init(CURL_GLOBAL_NOTHING); /* Init curl vars */

  if ((curl = curl_easy_init()) == NULL ) { /* Init an easy_session */
    curl_global_cleanup();
    fprintf(stderr, "[C] curl: init failed\n");
    return NULL ;
  }

  curl_easy_setopt(curl, CURLOPT_URL, c->server_url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
  c->curl = curl;

  if (verbose)
    printf("[C] Consumer initialized!\n");

  return c;
}

int consumer_process(Consumer *c) {
  ssize_t amount_read;
  size_t amount_to_read = get_read_size(c);
  char* tmp_buf = (char*) malloc(amount_to_read);

  Buffer* cur_buf = &c->buffers[c->buf_idx];
  size_t buf_remaining = cur_buf->capacity - cur_buf->size;
  char* dest = cur_buf->data + cur_buf->size;
  int verbose = c->verbose;

  /* Step 1: read from data source */
  /* TODO: Switch to USB tty when figured out */
  if (verbose)
    printf("[C] Reading %zu bytes\n", amount_to_read);

  while ((amount_read = read(c->data_fd, tmp_buf, amount_to_read)) < 0) {
    if (errno == EAGAIN || errno == EINTR)
      continue;

    perror("read");
    free(tmp_buf);
    return -1;
  }

  /* Step 2: check if buffer can fit */
  if (amount_read <= buf_remaining) {
    /* Can fit? Fill buffer */
    memcpy(dest, tmp_buf, amount_read);
    cur_buf->size += amount_read;
  } else {
    if (buf_remaining > 0) {
      /* Partially fits? Fill buffer then... */
      amount_read -= buf_remaining;
      memcpy(dest, tmp_buf, buf_remaining);
      cur_buf->size = cur_buf->capacity; /* buffer is now full */
    }

    if (verbose)
      printf("[C] Switching buffers\n");

    /* Check if other buffer is empty */
    cur_buf = &c->buffers[c->buf_idx ^ 1];

    if (cur_buf->size == cur_buf->capacity) {
      int dump_fd;
      struct timeval tv;
      struct tm *timeinfo;
      char fmt_str[80];
      char time_str[80];
      /* Still full. Write cur buf to SD, incremement error counter */
      fprintf(stderr,
          "[C] WARNING: Buffer %d still full! Dumping current buffer\n",
          c->buf_idx ^ 1);

      gettimeofday(&tv, NULL );
      timeinfo = localtime(&tv.tv_sec);
      strftime(fmt_str, sizeof fmt_str, "client-dump_%s%%06u.dat", timeinfo);
      snprintf(time_str, sizeof time_str, fmt_str, tv.tv_usec);
      char *dump_file = (char*) malloc(
          strlen(c->dump_path) + strlen(time_str) + 1);
      strcpy(dump_file, c->dump_path);
      strcat(dump_file, time_str);
      if ((dump_fd = open(dump_file, O_CREAT | O_EXCL | O_WRONLY)) < 0) {
        fprintf(stderr, "[C] ERROR: Error writing to \"%s\"\n", dump_file);
        perror("[C] ");
      }

      while (write(dump_fd, &c->buffers[c->buf_idx], sizeof(Buffer)) < 0) {
        if (errno == EAGAIN || errno == EINTR)
          continue;

        perror("[C] write");
        close(dump_fd);
        free(tmp_buf);
        return -1;
      }
      close(dump_fd);
      c->buffers[c->buf_idx].size = 0;
      ++c->err_count;

      if (c->err_count >= ERROR_LIMIT) {
        fprintf(stderr, "[C] Error limit reached!\n");
        if (notify_server(c) == 0)
          c->err_count = 0;
      }
    } else {
      /* Empty. Switch buffers and begin filling. */
      if (verbose)
        printf("[C] Fits in buffer %d\n", c->buf_idx ^ 1);
      c->buf_idx ^= 1;
      dest = cur_buf->data;
      memcpy(dest, tmp_buf, amount_read);
      cur_buf->size = amount_read;
    }
  }
  free(tmp_buf);
  return 0;
}

void consumer_cleanup(Consumer **c) {
  if ((*c)->verbose)
    printf("[C] Consumer clean up...\n");

  if (close((*c)->data_fd) < 0) {
    printf("[C] ");
    perror("close");
  }

  curl_easy_cleanup((*c)->curl);
  curl_global_cleanup();
  free((*c)->dump_path);

  if ((*c)->verbose)
    printf("[C] Consumer destroyed!\n");
  free(*c);
  *c = NULL;
}

/**
 * Gets the amount of data to be read by the consumer.
 *
 * @param c the consumer handle to look up the data source
 */
static size_t get_read_size(Consumer *c) {
  return 1024;
}

/** Notifies the server that the consumer had to dump a buffer. */
static int notify_server(Consumer *c) {
  CURLcode res = curl_easy_perform(c->curl);
  if (res != CURLE_OK) {
    fprintf(stderr, "[R] Error on curl HTTP request!\n");
    fprintf(stderr, "[R] %s\n", curl_easy_strerror(res));
    return -1;
  }
  return 0;
}
