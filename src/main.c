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

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "consumer/consumer.h"
#include "relay/relay.h"
#include "shared/buffer.h"
#include <sys/stat.h>

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
static struct option long_options[] = { { "server-path", required_argument,
    NULL, 's' }, { "data-source", required_argument, NULL, 'd' }, {
    "external-dir", required_argument, NULL, 'e' }, { "help", no_argument, NULL,
    'h' }, { "verbose", no_argument, NULL, 'v' } };

/**
 * A flag to enable/disable verbose debugging output at runtime.
 * 
 * Running the client program with one -v flag will enable LOW debugging level,
 * where only the initialization/destruction outputs are enabled. Running the
 * client program with two -v flags will enable HIGH debugging level, where
 * debugging output during operation is also enabled.
 */
static int verbose;

/**
 * The process id returned by fork().
 *
 * For the consumer process, pid is the id of the relay.
 * For the relay process, pid is 0.
 */
static int pid;

/**
 * A flag used when the relay process dies and the consumer needs to refork the
 * process.
 */
static int relay_needs_refork;

static void usage();

static void get_args(int argc, char** argv, char** data_source,
    char** server_path, char** external_dir, int* verbose);

void handle_relay_death(int sig);

/**
 * Main entrypoint into the carambola client program.
 * @see main.c
 */
int main(int argc, char* argv[]) {
  int shmid; /* shared memory id */
  size_t shm_size = sizeof(Buffer) * 2; /* shared memory size */
  Buffer* buffers; /* shared memory buffers */
  char* data_source = NULL; /* data source for consumer */
  char* server_path = NULL; /* server path for relay */
  char* external_dir = NULL; /* external dir for consumer */
  verbose = 0; /* Verbosity level: 0 = not verbose, 2+ = very verbose */
  struct sigaction act; /* Used to add the relay death signal handler */
  relay_needs_refork = 0;

  get_args(argc, argv, &data_source, &server_path, &external_dir, &verbose);
  if (data_source == NULL || server_path == NULL ) {
    usage();
    exit(EXIT_FAILURE);
  }
  if (verbose) /* print config */
    printf("Configuration:\n"
        "  verbosity:    %s\n"
        "  data source:  %s\n"
        "  ext. dump:    %s\n"
        "  server path:  %s\n\n", (verbose > 1 ? "HIGH" : "LOW"), data_source,
        external_dir, server_path);

  /* Check if path exists */
  struct stat dump_stat;
  if (stat(external_dir, &dump_stat) < 0) {
    if (errno == ENOENT) {
      fprintf(stderr,
          "[C] ERROR: Supplied external directory does not exist!\n");
      exit(EXIT_FAILURE);
    }
  } else {
    if (!S_ISDIR(dump_stat.st_mode)) {
      fprintf(stderr,
          "[C] ERROR: Supplied external directory is not a directory!\n");
      exit(EXIT_FAILURE);
    }
  }

  /* Set up shared memory buffer */
  if (verbose)
    printf("Setting up shared memory buffer (size = %zd)...\n", shm_size);
  if ((shmid = shmget(IPC_PRIVATE, shm_size, IPC_CREAT | IPC_EXCL | 00600))
      < 0) {
    /* shared mem get failed */
    printf("Shared memory setup FAILED!\n");
    perror("shmget");
    exit(EXIT_FAILURE);
  }
  if (verbose)
    printf("  created.  (shmid = %d)\n", shmid);

  buffers = (Buffer*) shmat(shmid, NULL, 0);
  if (buffers == (Buffer*) -1) { /* Could not attach to shm */
    perror("shmat");
    exit(EXIT_FAILURE);
  }
  buffers[0].capacity = __BUFFER_CAPACITY;
  buffers[1].capacity = __BUFFER_CAPACITY;
  if (verbose) {
    printf("  attached. (addr  = %p)\nShared memory setup done!\n\n", buffers);
  }

  /* fork */
  if (verbose) {
    printf("[C] Forking relay as child process...");
    /* Needed to prevent fork from copying buffers & printing 2x */
    fflush(stdout);
  }
  act.sa_handler = &handle_relay_death;
  act.sa_flags = SA_NOCLDSTOP;
  if (sigaction(SIGCHLD, &act, NULL ) < 0) {
    printf("FAILURE!\n");
    perror("signal");
    exit(EXIT_FAILURE);
  }
  if ((pid = fork()) < 0) {
    printf("FAILURE!\n");
    perror("fork");
    exit(EXIT_FAILURE);
  }
  if ((pid != 0) && verbose)
    printf("done! (pid = %d)\n", pid);

  relay_start: if (pid == 0) { /* relay code */
    Relay *r;
    if ((r = relay_init(buffers, server_path, external_dir, (verbose - 1) > 0))
        == NULL ) {
      fprintf(stderr, "[R] Relay init failed!\n");
      exit(EXIT_FAILURE);
    }

    while (1) {
      int res = relay_process(r);
      if (res < 0) {
        if (res == RELAYE_SERV) {
          sleep(1);
          continue;
        }
        break;
      }
    }

    relay_cleanup(&r);
  } else { /* consumer code */
    Consumer *c;
    if ((c = consumer_init(buffers, data_source, external_dir,
        (verbose - 1 > 0))) == NULL ) {
      fprintf(stderr, "[C] Consumer init failed!\n");
      exit(EXIT_FAILURE);
    }

    while (1) {
      if (consumer_process(c) < 0)
        break;

      if (relay_needs_refork) {
        fprintf(stderr, "[C] Attempting to restart relay process...");
        /* Needed to prevent fork from copying buffers & printing 2x */
        fflush(stderr);

        if ((pid = fork()) < 0) {
          fprintf(stderr, "FAILURE!\n");
          perror("fork");
          exit(EXIT_FAILURE);
        }
        relay_needs_refork = 0;
        if (pid != 0)
          fprintf(stderr, "done! (pid = %d)\n", pid);
        if (pid == 0)
          goto relay_start;
      } else {
        /* Wait for 1/100th of a second to simulate slow data collection */
        usleep(10000U);
      }
    }

    consumer_cleanup(&c);
  }

  /* detach shared memory */
  if (verbose) {
    printf(pid == 0 ? "[R] " : "[C] ");
    printf("Detaching shared memory buffer...");
  }
  if (shmdt((const void*) buffers) < 0) {
    printf("FAILURE!\n");
    perror("shmdt");
    exit(EXIT_FAILURE);
  }
  buffers = NULL;
  if (verbose)
    printf("done!\n");

  if (pid != 0) { /* Remove shared memory and reap child */
    if (verbose)
      printf("[C] Removing shared memory...");
    if (shmctl(shmid, IPC_RMID, NULL ) < 0) {
      printf("FAILURE!\n");
      perror("[C] shmctl");
      exit(EXIT_FAILURE);
    }
    if (verbose)
      printf("done!\n");

    if (verbose)
      printf("[C] Waiting on relay to exit...");
    signal(SIGCHLD, SIG_IGN );
    wait(NULL );
    if (verbose)
      printf("done!\n");
  }

  return EXIT_SUCCESS;
}

/** Print out help message */
static void usage() {
  fprintf(stderr,
      "Usage: client [-d|--data-source=<path>] [-s|--server-path=<path>]\n");
  fprintf(stderr, "              [-v|--verbose [-v|--verbose]] [--help]\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "REQUIRED:\n");
  fprintf(stderr,
      "  -d, --data-source=PATH  sets the data path for the consumer\n");
  fprintf(stderr,
      "  -e, --external-dir=PATH sets the dump path for the consumer\n");
  fprintf(stderr,
      "  -s, --server-path=PATH  sets the server uri for the relay\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "OPTIONAL:\n");
  fprintf(stderr, "      --help     display this help and exit\n");
  fprintf(stderr,
      "  -v, --verbose  increase program output. Use twice for more output\n");
}

/** Get all args from the command line */
static void get_args(int argc, char** argv, char** data_source,
    char** server_path, char** external_dir, int* verbose) {
  int c;
  while ((c = getopt_long(argc, argv, "d:s:ve:", long_options, NULL ))) {
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

    case 'e': /* External directory option */
      *external_dir = optarg;
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
  if (*external_dir == NULL ) {
    *external_dir = (char*) malloc(2);
    strcpy(*external_dir, ".");
  }
}

/**
 * Handle the death of a child process.
 *
 * If the child process exits abnormally, or exits with an error code, then
 * make an attempt to respawn the process.
 */
void handle_relay_death(int sig) {
  int status;

  if (verbose)
    printf("[C] Received child death signal\n");

  if (waitpid(-1, &status, WNOHANG) > 0) {

    if (WIFEXITED(status)) {
      printf("[C] Relay exited normally with status: %d\n",
          WEXITSTATUS(status));
    }

    if (WIFSIGNALED(status)) {
      printf("[C] Relay was terminated by signal: %d\n", WTERMSIG(status));
    }

    relay_needs_refork = 1;
  }
}
