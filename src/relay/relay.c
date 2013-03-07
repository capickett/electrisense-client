/**
 * @file relay.c
 * Implementation of the network relay for the carambola client 
 * @see relay.h
 *
 * @authors Larson, Patrick; Pickett, Cameron
 */

#include <curl/curl.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "relay.h"
#include "../shared/buffer.h"

static size_t write_data(void* buffer,
                         size_t size,
                         size_t nmemb,
                         void* userp);
static void* overload_process(void* args);

/**
 * Initializes the relay
 * @see relay.h
 */
Relay relay_init(Buffer* b,
                 char* server_url,
                 char* backup_source,
                 int verbose) {
  Relay   r; /* Relay struct to create */
  /* int  b_fd; *//* fd for SD card communication */

  if (verbose)
    printf("[R] Initializing relay...\n");
  
  /* FIXME: sdcard dump reading
  if ((b_fd = open(backup_source, O_RDWR)) < 0) {
    printf("[R] failed to establish communication with backup\n");
    perror(backup_source);
    return NULL;
  }
  */

  r = (Relay) malloc(sizeof(struct relay_st));
  r->buffers = b;
  /* r->backup_fd = b_fd; */
  r->server_url = server_url;
  r->verbose = verbose;
  if (verbose)
    printf("[R] Relay initialized!\n");

  if (verbose)
    printf("[R] Creating CURL forms...\n");

  /* Curl initialization */
  CURL* curl;
  struct curl_httppost* buf0_formpost   = NULL;
  struct curl_httppost* buf0_lastptr    = NULL;

  struct curl_httppost* buf1_formpost   = NULL;
  struct curl_httppost* buf1_lastptr    = NULL;
  struct curl_slist*    headerlist      = NULL;
  static const char     buf[]      = "Expect:";

  curl_global_init(CURL_GLOBAL_NOTHING); /* Init curl vars */

  if ((curl = curl_easy_init()) == NULL) { /* Init an easy_session */
    curl_global_cleanup();
    fprintf(stderr, "[R] curl: init failed\n");
    return NULL;
  }
  
  /* Add the file to the request */
  curl_formadd(&buf0_formpost,
               &buf0_lastptr,
               CURLFORM_COPYNAME, "sendfile",
               CURLFORM_BUFFER, "buf0",
               CURLFORM_BUFFERPTR, r->buffers[0].data,
               CURLFORM_BUFFERLENGTH, r->buffers[0].capacity,
               CURLFORM_END);
  curl_formadd(&buf1_formpost,
               &buf1_lastptr,
               CURLFORM_COPYNAME, "sendfile",
               CURLFORM_BUFFER, "buf1",
               CURLFORM_BUFFERPTR, r->buffers[1].data,
               CURLFORM_BUFFERLENGTH, r->buffers[1].capacity,
               CURLFORM_END);
  
  if (verbose)
    printf("[R] CURL forms initialized!\n");
  
  headerlist = curl_slist_append(headerlist, buf);
  curl_easy_setopt(curl, CURLOPT_URL, r->server_url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);

  r->curl  =          curl;
  r->form0 = buf0_formpost;
  r->form1 = buf1_formpost;
  r->slist =    headerlist;
  
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
  
  int verbose = r->verbose;
  /* Step 1: check sd card */
  
  /* TODO: Fix conditional statement
  if (check SD card) {
    if (verbose)
      printf("[R] Detected SD overflow, spawning child...\n");
    pthread_t sd_thread;
    if (pthread_create(&sd_thread, NULL, &overload_process, (void *)r) != 0) {
      // TODO: Should we always print this? Or wrap in verbose?
      // Also, same thing on line 135
      printf("[R] Failed to create child thread");
      perror("pthread_create");
      return -1;
    }
    if (pthread_detach(sd_thread) != 0) {
      printf("[R] Failed to detach child thread");
      return -1;
    }
    if (verbose) {
      printf("[R] Child successfully spawned");
    }
  }
  */

  /* Step 2: check buffer */
    /* Anything there? send PAYLOAD_SIZE bytes to server */
    /* Grab payload bytes, readjust buffer (circular buffer might
       be good here) */
    /* send off bytes */
  
  if (r->buffers[r->buf_idx].capacity != r->buffers[r->buf_idx].size)
    r->buf_idx ^= 1; // switch buffers
  
  if (r->buffers[r->buf_idx].capacity != r->buffers[r->buf_idx].size)
    return 0; // neither buffer is full, work is done 
  
  // Set curl buffer pointer
  curl_easy_setopt(r->curl, CURLOPT_HTTPPOST, (r->buf_idx == 0)
      ? r->form0
      : r->form1);

  CURLcode res = curl_easy_perform(r->curl);
  if (res != CURLE_OK) {
    // TODO: Some error happened
    fprintf(stderr, "[R] Error on curl HTTP request!\n");
    fprintf(stderr, "[R] ");
    fprintf(stderr, curl_easy_strerror(res));
    fprintf(stderr, "\n");
    return -1;
  }
  // successful transfer, reset buffer and swap index
  r->buffers[r->buf_idx].size = 0;
  r->buf_idx = r->buf_idx ^ 1;

  return 0;
}

/**
 * Free the relay handle and clean up for shutdown
 * @see relay.h
 */
void relay_cleanup(Relay* r) {
  if ((*r)->verbose)
    printf("[R] Relay clean up...\n");

  /* FIXME: sdcard backup
  if (close((*r)->backup_fd) < 0) {
    perror("[R] close");
  }
  */

  if ((*r)->verbose)
    printf("[R] Cleaning up CURL request\n");

  curl_easy_cleanup((*r)->curl);
  curl_slist_free_all((*r)->slist);
  curl_formfree((*r)->form0);
  curl_formfree((*r)->form1);
  curl_global_cleanup();

  if ((*r)->verbose) {
    printf("[R] Relay destroyed!\n");
  }

  free(*r);
  *r = NULL;
}

static size_t write_data(void* buffer, size_t size, size_t nmemb, void* userp) {
  return size*nmemb; /* Do not print to stdout */
}

static void* overload_process(void *arg) {
  // TODO: This.
  // Relay r = (Relay) arg;
  /* get fd to SD stuff */
  // int sd_fd;
  return NULL;
}
