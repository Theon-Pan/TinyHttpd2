#include "options.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define MAX_EVENTS 64

// Connection status for keep-alive support
typedef enum {
    CONN_CLOSE,      // Close connection
    CONN_KEEP_ALIVE  // Keep connection alive
} connection_status_t;

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


connection_status_t handle_client(int client_socket);

int main(int argc, char *argv[])
{
    int server_socket, epoll_fd, nfds, client_socket;
    struct sockaddr_in client_address = {0};
    size_t client_addr_len = sizeof(client_address);
    struct epoll_event event, events[MAX_EVENTS];

    if (1 == argc)
    {
        usage();
        exit(EXIT_FAILURE);
    }

    // Setup signal handlers for graceful shutdown.
    setup_signal_handlers();

    Options options = {0};
    set_options(argc, argv, &options);

    server_socket = create_server_socket(options.listening_port);

    if (server_socket < 0)
    {
        exit(EXIT_FAILURE);
    }

    // Create epoll instance
    epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd == -1)
    {
        perror("epoll_create1");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Add server socket to epoll
    event.events = EPOLLIN;
    event.data.fd = server_socket;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event) == -1)
    {
        perror("epoll_ctl: server_socket");
        close(server_socket);
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", options.listening_port);
    printf("Press Ctrl+C to stop\n");

    while (!shutdown_flag) 
    {
        // Wait for events with 1 second timeout
        nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);
        
        if (nfds == -1)
        {
            if (errno == EINTR)
            {
                if (shutdown_flag)
                {
                    break;
                }
                continue;
            }
            else
            {
                perror("epoll_wait");
                break;
            }
        }

        // Process all ready events
        for (int i = 0; i < nfds; i++)
        {
            if (events[i].data.fd == server_socket)
            {
                // New connection on server socket
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

                // Add client socket to epoll
                event.events = EPOLLIN | EPOLLET; // Edge-triggered for better performance
                event.data.fd = client_socket;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_socket, &event) == -1)
                {
                    perror("epoll_ctl: client_socket");
                    close(client_socket);
                    continue;
                }
            }
            else
            {
                // Activity on client socket
                int fd = events[i].data.fd;
                
                // Handle client request
                connection_status_t status = handle_client(fd);
                
                // Close connection if requested or on error
                if (status == CONN_CLOSE)
                {
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) == -1)
                    {
                        perror("epoll_ctl: EPOLL_CTL_DEL");
                    }
                    close(fd);
                }
                // For CONN_KEEP_ALIVE, connection stays in epoll for next request
            }
        }
    }

    // Graceful shutdown.
    printf("Shutting down server...\n");

    // Close epoll instance and server socket
    close(epoll_fd);
    close(server_socket);

    printf("Server closed.\n");

    return 0;
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

connection_status_t handle_client(int client_socket)
{
    char buffer[BUFFER_SIZE] = {0};
    connection_status_t conn_status = CONN_CLOSE;  // Default to close
    
    // Read the request
    ssize_t bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1);
    if (bytes_read <= 0)
    {
        // Connection closed by client or error
        return CONN_CLOSE;
    }
    
    buffer[bytes_read] = '\0';
    printf("Received request:\n%s\n", buffer);
    
    // Parse HTTP request to check for Connection header
    char *connection_header = NULL;
    char *header_start = strstr(buffer, "Connection:");
    if (!header_start)
    {
        header_start = strstr(buffer, "connection:");  // Case insensitive
    }
    
    if (header_start)
    {
        // Find the end of the header line
        char *header_end = strstr(header_start, "\r\n");
        if (header_end)
        {
            // Extract header value
            header_start += 11;  // Skip "Connection:"
            while (*header_start == ' ' || *header_start == '\t') header_start++;  // Skip whitespace
            
            size_t value_len = header_end - header_start;
            connection_header = malloc(value_len + 1);
            strncpy(connection_header, header_start, value_len);
            connection_header[value_len] = '\0';
            
            // Check if it's keep-alive
            if (strstr(connection_header, "keep-alive") || strstr(connection_header, "Keep-Alive"))
            {
                conn_status = CONN_KEEP_ALIVE;
            }
        }
    }
    else
    {
        // Check HTTP version - HTTP/1.1 defaults to keep-alive, HTTP/1.0 defaults to close
        if (strstr(buffer, "HTTP/1.1"))
        {
            conn_status = CONN_KEEP_ALIVE;
        }
    }
    
    // Prepare response with appropriate Connection header
    char http_response[1024];
    const char *html_content = "<html><body><h1>Hello from Tinyhttpd2!</h1></body></html>";
    const char *connection_value = (conn_status == CONN_KEEP_ALIVE) ? "keep-alive" : "close";
    
    snprintf(http_response, sizeof(http_response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %zu\r\n"
        "Connection: %s\r\n"
        "\r\n"
        "%s",
        strlen(html_content), connection_value, html_content);

    // Send response
    ssize_t sent = send(client_socket, http_response, strlen(http_response), 0);
    if (sent < 0)
    {
        perror("send failed");
        conn_status = CONN_CLOSE;
    }
    
    if (connection_header)
    {
        free(connection_header);
    }
    
    return conn_status;
}
