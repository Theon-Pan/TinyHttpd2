#ifndef __CONNECTION_H
#define __CONNECTION_H

typedef enum {
    CONN_TYPE_INVALID = -1,
    CONN_TYPE_SOCKET,
    CONN_TYPE_TLS,
    CONN_TYPE_MAX,
} ConnectTypeId;

#endif