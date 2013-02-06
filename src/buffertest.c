#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <curl/curl.h>


/* The size of the buffer before data is sent, measured in bytes */
#define BUFSIZE    102400
/* The number of bytes "available" at every read */
#define STREAM_LEN 1024
/* Default server to send data to */
#define SERVER     "http://localhost:8080/"
/* Default location to read data from */
#define STREAM     "/dev/zero"
/* Number of times to fill the buffer for testing */
#define TEST_ITERS 10


/* Used to replace curl's builtin method which prints server response to stdout
 */
size_t write_data(void* buffer, size_t size, size_t nmemb, void* userp) {
  return size*nmemb; /* Do not print to stdout */
}

/* Reads data from source and places it into buffer
 * If buffer is NULL, malloc's the appropriate sized memory region to hold
 *   the data read in, and sets buffer to the start of that region.
 * in: 
 *   source - The path of a file or pipe to be read from
 *   size   - The maximum amount of data to read in
 * out:
 *   buffer - The location in memory to write data to
 * returns: Number of bytes actually read on success
 *          -1 on failure
 */
ssize_t read_data(int fd, ssize_t size, void** buffer) {
  size_t data_avail; /* Data available from stream */
  ssize_t data_read; /* Amount of data read by read() */

  data_avail = STREAM_LEN;

  if (*buffer == NULL) {
    *buffer = malloc(size < data_avail ? size : data_avail);
    if (*buffer == NULL) {
      perror("malloc()");
      return -1;
    }
  }

  if ((data_read = read(fd, *buffer, data_avail)) < 0 && errno != EINTR) {
    perror("read()");
    return -1;
  }

  return data_read;
}

int main(int argc, char* argv[]) {
  char*   server     = SERVER;  /* Server URL to send data */
  char*   filepath   = STREAM;  /* Source to read data from */
  long    buffersize = BUFSIZE; /* Size of the buffer that holds data read in */
  void*   buffer     = NULL;    /* Pointer to the start of the buffer */
  void*   bufferptr  = NULL;    /* Pointer to the area of the buffer to write */
  size_t  bufferfree = 0;       /* Number of bytes available to be written to */
  ssize_t readlength = 0;       /* Length of data read by read_data() */
  int     fd         = 0;       /* Source file descriptor */
  long    sendtime   = 0;    /* Stores the time result for all data sends */

  CURL* curl;
  CURLcode res;
  struct curl_httppost* formpost   = NULL;
  struct curl_httppost* lastptr    = NULL;
  struct curl_slist*    headerlist = NULL;
  static const char     buf[]      = "Expect:";

  int i;
  int opt;
  while ((opt = getopt(argc, argv, "f:b:s:")) != -1) {
    switch (opt) {
      case 'f':
        filepath = optarg;
        break;
      case 'b':
        buffersize = atol(optarg);
        break;
      case 's':
        server = optarg;
        break;
      default:
        fprintf(stderr, "Usage: %s [-f file] [-b buffer_size] [-s url]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
  }

  printf("Electrisense client buffer test\nConfiguration:\n");
  printf("  Stream size: %d\n  Buffer size: %ld\n", STREAM_LEN, buffersize);
  printf("  Data source: %s\n  Server url:  %s\n", filepath, server);

  printf("Setup...");
  if ((buffer = malloc(buffersize)) == NULL) {
    perror("malloc()");
    exit(EXIT_FAILURE);
  }

  if ((fd = open(filepath, O_RDONLY)) < 0) {
    perror(filepath);
    exit(EXIT_FAILURE);
  }
  
  curl_global_init(CURL_GLOBAL_NOTHING); /* Init curl vars */

  if ((curl = curl_easy_init()) == NULL) { /* Init an easy_session */
    fprintf(stderr, "curl: init failed\n");
    exit(EXIT_FAILURE);
  }

  /* Add the file to the request */
  curl_formadd(&formpost,
               &lastptr,
               CURLFORM_COPYNAME, "sendfile",
               CURLFORM_BUFFER, "file",
               CURLFORM_BUFFERPTR, buffer,
               CURLFORM_BUFFERLENGTH, buffersize,
               CURLFORM_END);

  headerlist = curl_slist_append(headerlist, buf);
  curl_easy_setopt(curl, CURLOPT_URL, server);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
  curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
  printf("done!\n");

  printf("Starting test...\n\n");
  for (i = 0; i < TEST_ITERS; ++i) {
    struct timeval t1, t2;
    int num_reads = 0;
    long time_elapsed;

    printf("  iteration %d:\n", i);
    bufferptr  = buffer;
    bufferfree = buffersize;

    printf("    filling buffer...");
    gettimeofday(&t1, NULL);
    while (bufferfree != 0) {
      if ((readlength = read_data(fd, bufferfree, &bufferptr)) < 0) {
        fprintf(stderr, "Error in read_data(). Exiting.\n");
        exit(EXIT_FAILURE);
      }
      bufferfree  -= readlength;
      bufferptr   += readlength;
      ++num_reads;
    }
    gettimeofday(&t2, NULL);
    time_elapsed = (1000000*(t2.tv_sec - t1.tv_sec) + (t2.tv_usec - t1.tv_usec));
    printf("done! (%ld avg usec between read, %d reads)\n",
        time_elapsed/num_reads, num_reads);

    printf("    sending buffer...");
    gettimeofday(&t1, NULL);
    /* Send the request */
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
      continue;
    }
    gettimeofday(&t2, NULL);
    time_elapsed = (1000000*(t2.tv_sec - t1.tv_sec) + (t2.tv_usec - t1.tv_usec));
    sendtime += time_elapsed;
    printf("done! (%ld usec to complete)\n", time_elapsed);
  }

  printf("\nAverage time to send %ld bytes: %ld usec\n", 
      buffersize, sendtime/TEST_ITERS);
  printf("Cleanup...");
  curl_easy_cleanup(curl);
  curl_slist_free_all(headerlist);
  curl_formfree(formpost);
  curl_global_cleanup();
  close(fd);
  free(buffer);
  printf("done!\n");
  return 0;
}
