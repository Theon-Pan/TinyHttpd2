#define _POSIX_C_SOURCE 200809L
#include "server.h"
#include "options.h"
#include "ae.h"
#include "anet.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

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
        fprintf(fp, "%s\n", msg);
    }
    
    fflush(fp);
    if (!log_to_stdout) fclose(fp);
    
}

void sigShutdownHandler(int sig)
{
    char *msg;
    switch(sig)
    {
        case SIGINT: 
            msg = "Received SIGINT scheduling shutdown...";
            break;
        case SIGTERM:
            msg = "Received SIGTERM scheduling shutdown...";
            break;
        default:
            msg = "Received shutdown signal, scheduling shutdown...";
    };

    serverLogRaw(LL_WARNING|LL_RAW, msg);
}

void setupSignalHandlers(void)
{
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = sigShutdownHandler;
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGINT, &act, NULL);
}

/**
 * Initialize the tiny web server instance.
 */
void initServer(struct Options *options)
{
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    setupSignalHandlers();

    /* Config server. */
    snprintf(server.bindaddr, sizeof(server.bindaddr), options->bindadddr);
    server.port = options->port;
    /* @todo: Currently we set the max clients to 10, will change to depend on the args in the options. */
    server.el = aeCreateEventLoop(10);
    if (server.el == NULL)
    {
        serverLog(LL_WARNING|LL_RAW, "Failed creating the event loop. Error message: '%s'", strerror(errno));
        exit(1);
    }
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
    serverLog(LL_DEBUG|LL_RAW, "server options initialized!");

    initServer(&options);
    sleep(10000);

}