#ifndef __CONNHELPS_H
#define __CONNHELPS_H

#include "connection.h"

static inline int connHasRefs(connection *conn) {
    return conn->refs;
}

#endif