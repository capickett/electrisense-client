/**
 * @file relay.c
 * Implementation of the network relay for the carambola client 
 * @see relay.h
 *
 * @authors Larson, Patrick; Pickett, Cameron
 */

#include <curl/curl.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>

#include "relay.h"
#include "../shared/buffer.h"

static size_t write_data(void* buffer, size_t size, size_t nmemb, void* userp);
static int handle_dump_files(Relay *r, struct dirent **namelist, int n);
static int dump_filter(const struct dirent *entry);

struct handler_st {
  Relay *r;
  struct dirent **namelist;
  int n;
};

/**
 * Initializes the relay
 * @see relay.h
 */
Relay* relay_init(Buffer* b, char* server_url, char* backup_source, int verbose) {
  Relay* r; /* Relay struct to create */

  if (verbose)
    printf("[R] Initializing relay...\n");

  r = (Relay*) malloc(sizeof(struct relay_st));
  r->buffers = b;
  r->server_url = server_url;
  r->verbose = verbose;
  if (verbose)
    printf("[R] Relay initialized!\n");

  if (verbose)
    printf("[R] Creating CURL forms...\n");

  /* Curl initialization */
  CURL* curl;
  struct curl_httppost* buf0_formpost = NULL;
  struct curl_httppost* buf0_lastptr = NULL;

  struct curl_httppost* buf1_formpost = NULL;
  struct curl_httppost* buf1_lastptr = NULL;
  struct curl_slist* headerlist = NULL;
  static const char buf[] = "Expect:";

  curl_global_init(CURL_GLOBAL_NOTHING); /* Init curl vars */

  if ((curl = curl_easy_init()) == NULL ) { /* Init an easy_session */
    curl_global_cleanup();
    fprintf(stderr, "[R] curl: init failed\n");
    return NULL ;
  }

  /* Add the file to the request */
  curl_formadd(&buf0_formpost, &buf0_lastptr, CURLFORM_COPYNAME, "sendfile",
      CURLFORM_BUFFER, "buf0", CURLFORM_BUFFERPTR, r->buffers[0].data,
      CURLFORM_BUFFERLENGTH, r->buffers[0].capacity, CURLFORM_END);
  curl_formadd(&buf1_formpost, &buf1_lastptr, CURLFORM_COPYNAME, "sendfile",
      CURLFORM_BUFFER, "buf1", CURLFORM_BUFFERPTR, r->buffers[1].data,
      CURLFORM_BUFFERLENGTH, r->buffers[1].capacity, CURLFORM_END);

  if (verbose)
    printf("[R] CURL forms initialized!\n");

  headerlist = curl_slist_append(headerlist, buf);
  curl_easy_setopt(curl, CURLOPT_URL, r->server_url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);

  /* Add slash at the end if not there */
  r->dump_dir = (char*) malloc(strlen(backup_source) + 2);
  strcpy(r->dump_dir, backup_source);
  if (backup_source[strlen(backup_source) - 1] != '/')
    strcat(r->dump_dir, "/");

  r->curl = curl;
  r->form0 = buf0_formpost;
  r->form1 = buf1_formpost;
  r->slist = headerlist;

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
int relay_process(Relay *r) {
  /* Step 1: check sd card */
  int n;
  struct dirent **namelist;
  if ((n = scandir(r->dump_dir, &namelist, &dump_filter, alphasort)) < 0) {
    fprintf(stderr, "[R] Error scanning dump directory!");
    perror("[R] scandir");
    return -1;
  }

  if (n > 0) {
    return handle_dump_files(r, namelist, n);
  }

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
  curl_easy_setopt(r->curl, CURLOPT_HTTPPOST,
      (r->buf_idx == 0) ? r->form0 : r->form1);

  CURLcode res = curl_easy_perform(r->curl);
  if (res != CURLE_OK) {
    fprintf(stderr, "[R] Error on curl HTTP request!\n");
    fprintf(stderr, "[R] %s\n", curl_easy_strerror(res));
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
void relay_cleanup(Relay **r) {
  if ((*r)->verbose)
    printf("[R] Relay clean up...\n");

  free((*r)->dump_dir);

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
  return size * nmemb; /* Do not print to stdout */
}

static int handle_dump_files(Relay *r, struct dirent **namelist, int n) {
  struct curl_httppost *file_formpost = NULL;
  struct curl_httppost *file_lastptr = NULL;

  int i;
  for (i = 0; i < n; ++i) {
    curl_formadd(&file_formpost, &file_lastptr, CURLFORM_FILE,
        namelist[i]->d_name, CURLFORM_END);
  }

  curl_easy_setopt(r->curl, CURLOPT_HTTPPOST, file_formpost);

  CURLcode res = curl_easy_perform(r->curl);
  if (res != CURLE_OK) {
    /* TODO: Can we retry on certain errors? */
    fprintf(stderr, "[R] Error on sending curl dump!\n");
    fprintf(stderr, "[R] %s\n", curl_easy_strerror(res));
    return -1;
  }

  for (i = 0; i < n; ++i) {
    char fullpath[256];
    strcpy(fullpath, r->dump_dir);
    strcat(fullpath, namelist[i]->d_name);
    if (unlink(fullpath) < 0) {
      fprintf(stderr, "[R] Error on deleting file:\n  %s\n", fullpath);
      perror("[R] unlink");
    }
    if (r->verbose)
      printf("[R] %d dump files transfered.\n", n);
  }

  curl_formfree(file_formpost);
  return 0;
}

static int dump_filter(const struct dirent *entry) {
  static const char filter_str[13] = "client-dump_";
  int i;
  for (i = 0; i < 12; ++i)
    if (filter_str[i] != entry->d_name[i])
      return 0;
  return 1;
}
