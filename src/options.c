#include "options.h"
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>


void create_default_options(struct Options *options)
{
    if (NULL == options){
        return;
    }

    options->bindaddr_count = 0;
    options->port = CONFIG_DEFAULT_PORT;
}

void set_options(int argc, char *argv[], struct Options *options)
{
    if (NULL == options)
    {
        fprintf(stderr, "No options structure defined to carry on the command-line args.\n");
        exit(EXIT_FAILURE);
    }

    int opt;
    int options_index = 0;
    char *short_opts = "p:H:?h";
    char *endptr;
    long t;


    const struct option long_options[] = {
        {"host", required_argument, NULL, 'H'},
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
            case 'H':
                if (strlen(optarg) <= 0) {
                    fprintf(stderr, "The bind address must be specified.\n");
                    exit(EXIT_FAILURE);
                }
                char *copy = strdup(optarg);
                if (copy == NULL) {
                    perror("strdup bind addresses:");
                    exit(EXIT_FAILURE);
                }

                /* Parse the bind addresses str which separated by blank. */
                char *tokens[CONFIG_BINDADDR_MAX];
                char *saveptr;
                char *token = strtok_r(copy, " ", &saveptr);
                int count = 0;
                while (token != NULL && count < CONFIG_BINDADDR_MAX) {
                    tokens[count++] = token;
                    token = strtok_r(NULL, " ", &saveptr);
                }

                /* Copy to options. */
                for (int i = 0; i < count; i++) {
                    options->bindaddr[i] = strdup(tokens[i]);
                }

                options->bindaddr_count = count;

                /* Free the memory of the copy of optarg. */
                if (copy) {
                    free(copy);
                }
        
                break;
            case 'p':
                t = strtol(optarg, &endptr, 10);
                if (errno != 0 || endptr == optarg || t <= 0 || t > 65535){
                    fprintf(stderr, "Invalid option --port %s: Illegal port number.\n", optarg);
                    exit(EXIT_FAILURE);
                }
                options->port = (int) t;
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

void free_options(struct Options *args) {
    if (args) {
        /* Free the memory of bind addresses if exists. */
        if (args->bindaddr_count > 0) {
            for (int i = 0; i < args->bindaddr_count; i++) {
                if (args->bindaddr[i]) {
                    free(args->bindaddr[i]);
                }
            }
        }
    }
}

void usage(void){
    printf("Usage:\n");
    return;
}