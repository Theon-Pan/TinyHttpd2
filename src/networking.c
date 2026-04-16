#include "server.h"
#include "connection.h"

void acceptCommonHandler(connection *conn, char *ip) {
    UNUSED(ip);

    char addr[CONN_ADDR_STR_LEN] = {0};
    char laddr[CONN_ADDR_STR_LEN] = {0};
    connFormatAddr(conn, addr, sizeof(addr), 1);
    connFormatAddr(conn, laddr, sizeof(laddr), 0);

    if (connGetState(conn) != CONN_STATE_ACCEPTING) {
        serverLog(LL_VERBOSE | LL_RAW, "Accepted client connection in error state: %s (addr=%s laddr=%s)",
                    connGetLastError(conn), addr, laddr);
        connClose(conn);
        return;
    }

    char *msg = "Hello world!";
    if (connWrite(conn, msg, strlen(msg)) == -1) {

    }
    connClose(conn);

    return;

}