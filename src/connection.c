#include "server.h"
#include "serverassert.h"
#include "connection.h"

static ConnectionType *connTypes[CONN_TYPE_MAX];

int connTypeRegister(ConnectionType *ct) {
    int type = ct->get_type();
    assert(type >= 0 && type <= CONN_TYPE_MAX && !connTypes[type]);

    serverLog(LL_VERBOSE|LL_RAW, "Connection type %s registering", getConnectionTypeName(type));
    connTypes[type] = ct;

    if (ct->init) {
        ct->init();
    }

    return C_OK;
}