#include "options.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>

/**
 * Create a server socket for listening at a specific port number.
 * 
 * RETURNS:
 *      Returns socket file descriptor if success, else returns negative. 
 */
int create_server_socket(int port);

int main(int argc, char *argv[])
{
    int server_socket;

    if (1 == argc)
    {
        usage();
        exit(EXIT_FAILURE);
    }

    Options options = {0};
    set_options(argc, argv, &options);

    server_socket= create_server_socket(options.listening_port);

    close(server_socket);

    printf("Implementation is still in prgress...\n");

}

int create_server_socket(int port)
{
    if (port <= 0)
    {
        fprintf(stderr, "Illegal port number: %d\n", port);
        return -1;
    }

    int server_fd;
    struct sockaddr_in address = {0};
    int opt = 1;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd <= 0)
    {
        perror("Can not get a server socket.");
        return -1;
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)(&address), sizeof(address)) < 0)
    {
        close(server_fd);
        perror("Binding address to server socket is failed.");
        return -1;
    }

    if (listen(server_fd, SOMAXCONN) < 0){
        close(server_fd);
        perror("Marking server socket as listener is failed.");
        return -1;
    }

    return server_fd;
}