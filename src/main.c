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

/**
 * Main entrypoint into the carambola client program.
 * @see main.c
 */
int main(int argc, char* argv[]) {
  
  return EXIT_SUCCESS;
}
