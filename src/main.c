/**
 * @file main.c
 * Main driver for client; runs setup and spawns relay and collector processes.
 *
 * Setup
 * -----
 * - Read in any cli args
 * - Setup shared memory and allocate buffers in shared memory
 * - fork() child to be the relay
 *
 * Collector
 * ---------
 * - Reads in from configured data source (USB in production usage)
 * - Stores read data in larger double buffers for relay to read and send
 * - Incorporates error handling response to save data to SD card or other
 *   storage
 * - In case of relay (child process) dying, can refork() and restart relay
 *
 * Relay
 * -----
 * - Reads from the double buffers and transmits data to a nearby server
 * - In the case of data redirected to SD card, spawns additional thread to
 *   handle SD card data.
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "main.h"
#include "consumer/consumer.h"
#include "relay/relay.h"
#include "shared/buffer.h"


/**
 * Main entrypoint into the carambola client program.
 * @see main.c
 */
int main(int argc, char* argv[]) {
  int verbose = 0; /* Verbosity level: 0 = not verbose, 2+ = very verbose */
  char* data_source = NULL; /* data source for consumer */
  char* server_path = NULL; /* server path for relay */

  get_args(argc, argv, &data_source, &server_path, &verbose);
  
  if (data_source == NULL || server_path == NULL) {
    usage();
    exit(EXIT_FAILURE);
  }
  return EXIT_SUCCESS;
}

static void usage() {
  fprintf(stderr, "Usage: client [-d|--data-source=<path>] [-s|--server-path=<path>]\n");
  fprintf(stderr, "              [-v|--verbose [-v|--verbose]] [--help]\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "REQUIRED:\n");
  fprintf(stderr, "  -d, --data-source=PATH  sets the data path for the consumer\n");
  fprintf(stderr, "  -s, --server-path=PATH  sets the server uri for the relay\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "OPTIONAL:\n");
  fprintf(stderr, "      --help     display this help and exit\n");
  fprintf(stderr, "  -v, --verbose  increase program output. Use twice for more output\n");
}

static void get_args(int argc, char** argv, char** data_source,
    char** server_path, int* verbose) {
  int c;
  while ((c = getopt_long(argc, argv, "d:s:v", long_options, NULL))) {
    if (c == -1)
      break; /* Done processing optargs */

    switch (c) {
      case 'h': /* help option */
        usage();
        exit(EXIT_SUCCESS);
        break;
      case 'd': /* Data source option */
        *data_source = optarg;
        break;

      case 's': /* Server path option */
        *server_path = optarg;
        break;

      case 'v': /* verbose flag */
        ++(*verbose);
        break;

      case '?':
        break;
    }
  }
}
