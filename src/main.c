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

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>

#include "consumer/consumer.h"
#include "relay/relay.h"
#include "shared/buffer.h"


/**
 * The options available for invoking the program from the command line.
 * - *server-path*: 
 *       A valid URI pointing to the local network server for the relay
 *       to communicate with.
 * - *data-source*:
 *       A valid URI pointing to the data source for the collector to grab
 *       measured data from.
 * - *verbose*:
 *       A flag to increase the frequency and detail of logging by the program.
 *       Useful for debugging.
 */
static struct option long_options[] = {
  {"server-path", required_argument,  NULL, 's'},
  {"data-source", required_argument,  NULL, 'd'},
  {"help",        no_argument,        NULL, 'h'},
  {"verbose",     no_argument,        NULL, 'v'}
};

static void usage();

static void get_args(int argc, char** argv, char** data_source,
    char** server_path, int* verbose);


/**
 * Main entrypoint into the carambola client program.
 * @see main.c
 */
int main(int argc, char* argv[]) {
  int pid;   /* pid for fork() */
  int shmid; /* shared memory id */
  size_t shm_size = __BUFFER_SIZE * 2; /* shared memory size */
  Buffer* buffers; /* shared memory buffers */
  int verbose = 0; /* Verbosity level: 0 = not verbose, 2+ = very verbose */
  char* data_source = NULL; /* data source for consumer */
  char* server_path = NULL; /* server path for relay */

  get_args(argc, argv, &data_source, &server_path, &verbose);
  
  if (data_source == NULL || server_path == NULL) {
    usage();
    exit(EXIT_FAILURE);
  }

  if (verbose) { /* print config */
    printf("Configuration:\n");
    printf("  verbosity:    %s\n", (verbose > 1 ? "HIGH" : "LOW"));
    printf("  data source:  %s\n", data_source);
    printf("  server path:  %s\n", server_path);
    printf("\n");
  }

  /* Set up shared memory buffer */
  if (verbose)
    printf("Setting up shared memory buffer (size: %zd)...\n", shm_size);
  if ((shmid = shmget(IPC_PRIVATE, shm_size,
          IPC_CREAT | IPC_EXCL | 00600)) < 0) {
    /* shared mem get failed */
    perror("shmget");
    exit(EXIT_FAILURE);
  }
  if (verbose)
    printf("  created. shmid=%d\n", shmid);

  buffers = (Buffer*) shmat(shmid, NULL, 0);
  if (buffers == (Buffer*) -1) { /* Could not attach to shm */
    perror("shmat");
    exit(EXIT_FAILURE);
  }
  if (verbose) {
    printf("  attached. addr=%p\nShared memory setup done!\n", buffers);
  }
  
  /* fork */
  if ((pid = fork()) < 0) {
    perror("fork");
    exit(EXIT_FAILURE);
  }

  if (pid == 0) { /* relay code */
    Relay r;
    /* TODO: Implement relay code */
    /*if ((r = relay_init()) == NULL) {
      perror("relay_init");
      exit(EXIT_FAILURE);
    }

    while (1) {
      if (relay_process(r) < 0)
        break;
    }

    relay_cleanup(&r);*/
  } else { /* consumer code */
    Consumer c;
    /* TODO: Implement consumer code */
    /*if ((c = consumer_init()) == NULL) {
      perror("consumer_init");
      exit(EXIT_FAILURE);
    }

    while (1) {
      if (consumer_process(c) < 0)
        break;
    }

    consumer_cleanup(&c);*/
  }

  /* detach shared memory */
  if (verbose) {
    printf(pid == 0 ? "[R] " : "[C] ");
    printf("Detaching shared memory buffer...");
  }
  if (shmdt((const void*) buffers) < 0) {
    printf("\n");
    perror("shmdt");
    exit(EXIT_FAILURE);
  }
  buffers = NULL;
  if (verbose)
    printf("done!\n");

  if (pid != 0) { /* Remove shared memory and reap child */
    if (verbose)
      printf("[R] Removing shared memory...");
    if (shmctl(shmid, IPC_RMID, NULL) < 0) {
      printf("\n[R] ");
      perror("shmctl");
      exit(EXIT_FAILURE);
    }
    if (verbose)
      printf("done!\n");
    
    if (verbose)
      printf("[R] Waiting on relay to exit...");
    wait(NULL);
    if (verbose)
      printf("done!\n");
  }

  return EXIT_SUCCESS;
}

/** Print out help message */
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

/** Get all args from the command line */
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
