#define _POSIX_C_SOURCE 200809L
#include "server.h"
#include "options.h"
#include "ae.h"
#include "anet.h"
#include "connection.h"
#include "serverassert.h"

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
    char *default_bindaddr[CONFIG_DEFAULT_BINDADDR_COUNT] = CONFIG_DEFAULT_BINDADDR;

    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    setupSignalHandlers();

    /* Config server. */
    /* First, set the default bind addresses. */
    for (int i = 0; i < CONFIG_DEFAULT_BINDADDR_COUNT; i++) {
        server.bindaddr[i] = strdup(default_bindaddr[i]);
    }

    /* Second, if there are bind addresses in options, replace with options. */
    if (options->bindaddr_count > 0) {
        for (int i = 0; i < options->bindaddr_count; i++) {
            if (i < CONFIG_DEFAULT_BINDADDR_COUNT) {
                free(server.bindaddr[i]);
            }
            server.bindaddr[i] = strdup(options->bindaddr[i]);
        }
    }

    server.port = options->port;
    memset(server.listeners, 0x00, sizeof(server.listeners));

    /* @todo: Will get value from options */
    server.mptcp = 0;
    server.tcp_backlog = 300;
    server.socket_mark_id = 0;
    server.max_new_conns_per_cycle = 1000;

    /* @todo: Currently we set the max clients to 1024 + 96, will change to depend on the args in the options. */
    server.el = aeCreateEventLoop(1024 + 96);
    if (server.el == NULL)
    {
        serverLog(LL_WARNING|LL_RAW, "Failed creating the event loop. Error message: '%s'", strerror(errno));
        exit(1);
    }
}

void initListeners(void) {
    /* Setup listeners from server config for TCP/TLS/Unix */
    ConnectionType *ct;
    connListener *listener;
    if (server.port != 0) {
        ct = connectionByType(CONN_TYPE_SOCKET);
        if (!ct) panic("Failed finding connection listener of %s", getConnectionTypeName(CONN_TYPE_SOCKET));
        listener = &server.listeners[CONN_TYPE_SOCKET];
        listener->bindaddr = server.bindaddr;
        listener->bindaddr_count = server.bindaddr_count;
        listener->port = server.port;
        listener->ct = ct;
    }

    // if (server.tls_port != 0) {
    //     ct = connectionByType(CONN_TYPE_TLS);
    // }

    /* Create all the configured listener, and add handler to start to accept */
    int listen_fds = 0;
    for (int j = 0; j < CONN_TYPE_MAX; j++) {
        listener = &server.listeners[j];
        if (listener->ct == NULL) continue;

        if (connListen(listener) == C_ERR) {
            serverLog(LL_WARNING|LL_RAW, "Failed listening on port %u (%s), abording.", listener->port,
                    getConnectionTypeName(listener->ct->get_type()));
            exit(EXIT_FAILURE);
        }
        listen_fds += listener->count;  
    }

    if (listen_fds == 0) {
        serverLog(LL_WARNING|LL_RAW, "Configured to not listen anywhere, exiting.");
        exit(EXIT_FAILURE);
    }
    
}

/* Create an event handler for accepting new connections in TCP or TLS domain sockets. 
 * This works atomically for all socket fds */
int createSockerAcceptHandler(connListener *sfd, aeFileProc *accept_handler) {
    int j;
    
    for (j = 0; j < sfd->count; j++) {
        if (aeCreateFileEvent(server.el, sfd->fd[j], AE_READABLE, accept_handler, sfd) == AE_ERR) {
            /* Rollback */
            for (j = j - 1; j >= 0; j--) aeDeleteFileEvent(server.el, sfd->fd[j], AE_READABLE);
            return C_ERR;
        }
    }

    return C_OK;
}


/* Initialize a set of file descriptors to listen to the specified 'port'
 * binding the addresses specified in the server configuration.
 * 
 * The listening file descriptors are stored in the integer array 'fds'
 * and their number is set in '*count'. Actually @sfd should be 'listener',
 * for the historical reasons, let's keep 'sfd here.
 * 
 * The addresses  to bind are specified in the global server.bindaddr array
 * and their number is server.bindaddr_count. If the sever configuration
 * contains no specific addresses to bind, this function will try to 
 * bind * (all addresses) for both the IPv4 and Ipv6 protocols.
 * 
 * On success the function returns C_OK.
 * 
 * On error the function returns C_ERR. For the function to be on
 * error, at least one of the server.bindaddr addresses was
 * impossible to bind, or no bind addresses were specified in the server
 * configuration but the function is not able to bind * for at least
 * one of the IPv4 or IPv6 protocols. */
int listenToPort(connListener * sfd) {
    int port = sfd->port;
    char **bindaddr = sfd->bindaddr;
    
    /* If we have not bind address, we don't listen on a TCP socket */
    if (sfd->bindaddr_count == 0) return C_OK;

    for (int i = 0; i < sfd->bindaddr_count; i++) {
        char *addr = bindaddr[i];
        int optional = ((*addr) == '-');
        if (optional) addr++;
        if (strchr(addr, ':')) {
            /* Bind IPv6 address. */
            sfd->fd[sfd->count] = anetTcp6Server(server.neterr, port, addr, server.tcp_backlog, server.mptcp);

        } else {
            /* Bind IPv4 address. */
            sfd->fd[sfd->count] = anetTcpServer(server.neterr, port, addr, server.tcp_backlog, server.mptcp);
        }
        if (sfd->fd[sfd->count] == ANET_ERR) {
            int net_errno = errno;
            serverLog(LL_WARNING | LL_RAW, "Warning: Could not create server TCP listening socket %s:%d: %s", addr, port, 
                    server.neterr);
            if (net_errno == EADDRNOTAVAIL && optional) continue;
            if (net_errno == ENOPROTOOPT || net_errno == EPROTONOSUPPORT || net_errno == ESOCKTNOSUPPORT ||
                net_errno == EPFNOSUPPORT || net_errno == EAFNOSUPPORT)
                continue;
            
            /* Rollback successful listens before exiting */
            connCloseListener(sfd);
            return C_ERR;
        }
        if (server.socket_mark_id > 0) anetSetSockMarkId(NULL, sfd->fd[sfd->count], server.socket_mark_id);
        anetNonBlock(NULL, sfd->fd[sfd->count]);
        anetCloexec(sfd->fd[sfd->count]);
        sfd->count++;
    }
    return C_OK;
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