#include "options.h"
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <errno.h>

void create_default_options(Options *options)
{
    if (NULL == options){
        return;
    }

    options->listening_port = DEFAULT_LISTENING_PORT;
}

void set_options(int argc, char *argv[], Options *options)
{
    if (NULL == options)
    {
        fprintf(stderr, "No options structure defined to carry on the command-line args.\n");
        exit(EXIT_FAILURE);
    }

    int opt;
    int options_index = 0;
    char *short_opts = "p:?h";
    char *endptr;
    long t;


    const struct option long_options[] = {
        {"port", required_argument, NULL, 'p'},
        {NULL, 0, NULL, 0}
    };

    // Get the command line options
    while ((opt = getopt_long(argc, argv, short_opts, long_options, &options_index)) != EOF)
    {
        switch (opt)
        {
            case 0:
                break;
            case 'p':
                t = strtol(optarg, &endptr, 10);
                if (errno != 0 || endptr == optarg || t <= 0 || t > 65535){
                    fprintf(stderr, "Invalid option --port %s: Illegal port number.\n", optarg);
                    exit(EXIT_FAILURE);
                }
                options->listening_port = (int) t;
                break;
            case ':':
            case 'h':
            case '?':
                usage();
                exit(EXIT_FAILURE);
        }
    }

    return;
}

void usage(void){
    return;
}