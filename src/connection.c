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

int connTypeInitialize(void) {
    /* currently socket connection type is necessary */
    assert(TinyHttpdRegisterConnectionTypeSocket() == C_OK);

    /**/
    return C_OK;
}

ConnectionType *connectionByType(int type) {
    assert(type >= 0 && type < CONN_TYPE_MAX);

    ConnectionType *ct = connTypes[type];
    if (!ct) {
        serverLog(LL_WARNING|LL_RAW, "Missing implement of connection type %s", getConnectionTypeName(type));
    }
    return ct;
}


