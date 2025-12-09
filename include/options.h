#ifndef _OPTIONS_H
#define _OPTIONS_H

#include <stdbool.h>

#define DEFAULT_LISTENING_PORT 4000

typedef struct 
{
    int listening_port;           // Port number.

} Options;

/**
 * Create a struct Options with default values.
 */
void create_default_options(Options * args);

/**
 * Get arguments from command-line and set them to struct Options.
 */
void set_options(int argc, char *argv[], Options *options);

/**
 * Show usage information.
 */
void usage(void);

/**
 * Validate the arguments parsed from command-line.
 */
bool validate_options(const Options options);

#endif