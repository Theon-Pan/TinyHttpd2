#ifndef _OPTIONS_H
#define _OPTIONS_H

#include "globalsettings.h"
#include <stdbool.h>

struct Options
{
    char *bindadddr[CONFIG_BINDADDR_MAX];                   /* The bind address */
    int port;                                               /* TCP listening port */

};

/**
 * Create a struct Options with default values.
 */
void create_default_options(struct Options * args);

/**
 * Get arguments from command-line and set them to struct Options.
 */
void set_options(int argc, char *argv[], struct Options *options);

/**
 * Show usage information.
 */
void usage(void);

/**
 * Validate the arguments parsed from command-line.
 */
bool validate_options(const struct Options options);

#endif