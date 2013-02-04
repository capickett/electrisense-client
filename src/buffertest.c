#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <curl/curl.h>


/* The size of the buffer before data is sent, measured in bytes */
#define BUFSIZE 102400
/* The rate samples are "generated", measured in bytes/second */
#define RDRATE 10240
/* Default server to send data to */
#define SERVER "http://athena:8080/"
/* Default location to read data from */
#define STREAM "/dev/zero"


/* Used to replace curl's builtin method which prints server response to stdout
 */
size_t write_data(void* buffer, size_t size, size_t nmemb, void* userp) {
  return size*nmemb; /* Do not print to stdout */
}

/* Reads data from source and places it into buffer
 * in: 
 *   source - The path of a file or pipe to be read from
 *   size   - The maximum amount of data to read in
 * out:
 *   buffer - The location in memory to write read data to
 * returns:  0 on success
 *          -1 on failure
 */
int read_data(char* source, size_t size, void** buffer);

/* Sends data stored in buffer via HTTP Post
 * in:
 *   server - The address of the server to receive the data
 *   size   - The amount of data to be sent in bytes
 *   buffer - A pointer to the data to be sent
 * returns:  0 on success
 *          -1 on failure
 */
int send_data(char* server, size_t size, void* buffer);


int main(int argc, char* argv[]) {
  char* server     = SERVER;
  char* filepath   = STREAM;
  int   buffersize = BUFSIZE;

  int opt;
  while ((opt = getopt(argc, argv, "f:b:s:")) != -1) {
    switch (opt) {
      case 'f':
        filepath = optarg;
        break;
      case 'b':
        buffersize = atoi(optarg);
        break;
      case 's':
        server = optarg;
        break;
      default:
        fprintf(stderr, "Usage: %s [-f file] [-r read_rate] [-s url]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
  }

  printf("Electrisense buffer test\nConfiguration:\n");
  printf("  Read rate: %d\n  Data source: %s\n", RDRATE, filepath);
  printf("  Buffer size: %d\n  Server url: %s\n", buffersize, server);

  void* buffer = malloc(buffersize);

  /* Test */

  free(buffer);
  return EXIT_SUCCESS;
}
