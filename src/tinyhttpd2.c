#include "options.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

// Global flag for graceful shutdown.
volatile sig_atomic_t shutdown_flag = 0;

/**
 * Signal handler for graceful shutdown.
 */
void signal_handler(int sig);

/**
 * Setup signal handlers
 */
void setup_signal_handlers(void);

/**
 * Create a server socket for listening at a specific port number.
 * 
 * RETURNS:
 *      Returns socket file descriptor if success, else returns negative. 
 */
int create_server_socket(int port);


void handle_client(int client_socket);

int main(int argc, char *argv[])
{
    int server_socket;
    fd_set read_fds;
    int max_fd, activity, client_socket;
    struct sockaddr_in client_address = {0};
    size_t client_addr_len = sizeof(client_address);
    int client_sockets[MAX_CLIENTS] = {0};
    struct timeval timeout;

    if (1 == argc)
    {
        usage();
        exit(EXIT_FAILURE);
    }

    // Setup signal handlers for graceful shutdown.
    setup_signal_handlers();

    Options options = {0};
    set_options(argc, argv, &options);

    server_socket= create_server_socket(options.listening_port);

    if (server_socket < 0)
    {
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", options.listening_port);
    printf("Press Ctrl+C to stop\n");

    while(!shutdown_flag) 
    {
        // Clear the socket set.
        FD_ZERO(&read_fds);

        // Add server socket to set
        FD_SET(server_socket, &read_fds);
        max_fd = server_socket;

        // Add client sockets to read fd set.
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (client_sockets[i] > 0)
            {
                FD_SET(client_sockets[i], &read_fds);
            }

            if (client_sockets[i] > max_fd)
            {
                max_fd = client_sockets[i];
            }
        }

        // Set timeout for select to check shutdown flag periodically.
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        // Wait for activity on one of the sockets.
        activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        

        if (activity < 0)
        {
            if (errno != EINTR)
            {
                perror("Socket select error");
                break;
            }
            else{
                if (shutdown_flag)
                {
                    break;
                }
            }
        }

        // If someting happened on the server socket, it's an incoming connection.
        if (FD_ISSET(server_socket, &read_fds))
        {
            client_socket = accept(server_socket, (struct sockaddr *)&client_address, (socklen_t *)&client_addr_len);
            if (client_socket < 0)
            {
                if (errno == EINTR && shutdown_flag)
                {
                    break;
                }
                else
                {
                    perror("Accepting in-bound connection failed.");
                    continue;

                }

            }

            // Add new socket to array of client sockets.
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (0 == client_sockets[i])
                {
                    client_sockets[i] = client_socket;
                    break;
                }
            }
        }

        // Check for activity on client sockets.
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (client_sockets[i] != 0 && FD_ISSET(client_sockets[i], &read_fds))
            {
                handle_client(client_sockets[i]);
                close(client_sockets[i]);
                client_sockets[i] = 0;
            }
        }

    }

    // Graceful shutdown.
    printf("Shutting down server...\n");

    // Close all sockets.
    close(server_socket);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (client_sockets[i] != 0)
        {
            close(client_sockets[i]);
        }
    }

    printf("Server closed.\n");

}

void signal_handler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM)
    {
        printf("\nReceived shutdown signal (%d). Shutting down gracefully...\n", sig);
        shutdown_flag = 1;
    }
}

void setup_signal_handlers(void)
{
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (-1 == sigaction(SIGINT, &sa, NULL))
    {
        perror("sigaction SIGINT");
        exit(EXIT_FAILURE);
    }

    if (-1 == sigaction(SIGTERM, &sa, NULL))
    {
        perror("sigaction SIGTERM");
        exit(EXIT_FAILURE);
    }

    // Ignore SIGPIPE, avoid crashing when client close the connection.
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    if (-1 == sigaction(SIGPIPE, &sa, NULL))
    {
        perror("sigaction SIGPIPE");
        exit(EXIT_FAILURE);
    }
    
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
        perror("Binding address to server socket failed.");
        return -1;
    }

    if (listen(server_fd, SOMAXCONN) < 0){
        close(server_fd);
        perror("Marking server socket as listener failed.");
        return -1;
    }

    return server_fd;
}

void handle_client(int client_socket)
{
    char buffer[BUFFER_SIZE] = {0};
    char *http_response = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: 46\r\n"
        "\r\n"
        "<html><body><h1>Hello from Tinyhttpd2!</h></body></html>";

    // Read the request (we'll just ignore it for now)
    ssize_t bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1);
    if (bytes_read > 0)
    {
        buffer[bytes_read] = '\0';
        printf("Received request:\n%s\n", buffer);
    }

    // Send response
    send(client_socket, http_response, strlen(http_response), 0);
}
