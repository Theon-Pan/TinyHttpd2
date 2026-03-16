#include "server.h"
#include "options.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

/* Global vars */
struct tinyHttpServer server;

/**
 * Low level logging.
 */
void serverLogRaw(int level, const char *msg)
{
    FILE *fp;
    int rawmode = (level & LL_RAW);
    int log_to_stdout;
    
    level &= 0xff;  /* Just use the low 8 bits. */
    if (level < LOG_VERBOSITY) return;
    
    /* @todo : So far we just use stdout to output server log, will change to output to a specified one as configure.*/
    fp = stdout;
    log_to_stdout = 1;
    if (!fp) return;
    
    /* @todo : We just support raw mode, just print the msg without any prefix. */
    if (rawmode)
    {
        fprintf(fp, "%s", msg);
    }
    
    fflush(fp);
    if (!log_to_stdout) fclose(fp);
    
}

/**
 * Initialize the tiny web server instance.
 */
void initServer(struct Options *options)
{
    snprintf(server.bindaddr, sizeof(server.bindaddr), options->bindadddr);
    server.port = options->port;
}

void _serverlog(int level, const char *fmt, ...)
{
    va_list ap;
    char msg[LOG_MAX_LEN];

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    serverLogRaw(level, msg);
}

__attribute__((weak)) int main(int argc, char *argv[])
{
    if (1 == argc)
    {
        usage();
        exit(EXIT_FAILURE);
    }

    struct Options options = {0};
    create_default_options(&options);
    set_options(argc, argv, &options);
    serverLog(LL_DEBUG|LL_RAW, "server options initialized!\n");

    initServer(&options);

}