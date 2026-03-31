#ifndef __ANET_H
#define __ANET_H

#include "globalsettings.h"


#include <sys/types.h>
#include <stdint.h>
#include <arpa/inet.h>

#define ANET_OK 0
#define ANET_ERR -1
#define ANET_ERR_LEN 256

/* Flags used with certian functions. */
#define ANET_NONE 0
#define ANET_IP_ONLY (1 << 0)
#define ANET_PREFER_IPV4 (1 << 1)
#define ANET_PREFER_IPV6 (1 << 2)

#if defined(__sun) || defined(_AIX)
#define AF_LOCAL AF_UNIX
#endif

#ifdef _AIX
#undef ip_len
#endif

int anetNonBlock(char *err, int fd);
int anetBlock(char *err, int fd);
int anetIsBlock(char *err, int fd);
int anetCloexec(int fd);
int anetTcpServer(char *err, int port, char *bindaddr, int backlog, int mptcp);
int anetTcp6Server(char *err, int port, char *bindaddr, int backlog, int mptcp);
int anetTcpAccept(char *err, int serversock, char *ip, size_t ip_len, int *port);
int anetSetSockMarkId(char *err, int fd, uint32_t id);
int anetEnableTcpNoDelay(char *err, int fd);
int anetDisableTcpNoDelay(char *err, int fd);
int anetRetryAcceptOnError(int err);
int anetKeepAlive(char *err, int fd, int interval);


#endif