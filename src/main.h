/**
 * @file main.h
 *
 * Header file associated with the main client driver.
 * 
 * @authors Larson, Patrick; Pickett, Cameron
 */

#ifndef _MAIN_H
#define _MAIN_H

#include <getopt.h>

/**
 * The options available for invoking the program from the command line.
 * - _server_: 
 *       A valid URI pointing to the local network server for the relay
 *       to communicate with.
 * - _data-source_:
 *       A valid URI pointing to the data source for the collector to grab
 *       measured data from.
 * - _verbose_:
 *       A flag to increase the frequency and detail of logging by the program.
 *       Useful for debugging.
 */
static struct option long_options[] = {
  {"server-path",      required_argument,  NULL, 's'},
  {"data-source", required_argument,  NULL, 'd'},
  {"help",        no_argument,        NULL, 'h'},
  {"verbose",     no_argument,        NULL, 'v'}
};

/** Print out help message */
static void usage();

/** Get all args from the command line */
static void get_args(int argc, char** argv, char** data_source,
    char** server_path, int* verbose);

#endif
