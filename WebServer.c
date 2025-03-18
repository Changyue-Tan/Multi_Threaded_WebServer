#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080 // Port Listening for connection requests
#define BUFFER_SIZE 4096

int server_socket; // Global variable to store the server socket
int response_count = 0; // Global variable to track the request number
pthread_mutex_t response_count_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex to protect response_count

// Signal handler function
void handle_signal(int signal) {
    if (signal == SIGINT) {
        printf("\nReceived SIGINT (Ctrl+C). Stopping server...\n");
    } else if (signal == SIGTERM) {
        printf("\nReceived SIGTERM. Stopping server...\n");
    }
    printf("Closing server socket...\n");
    close(server_socket);
    printf("Server stopped.\n");
    exit(0);
}

// Send HTTP response
void send_http_response(int client_socket, const char *path, int current_response_number) {
    char response[BUFFER_SIZE];
    const char *content_type = "text/html";
    const char *status_line = "HTTP/1.1 200 OK\r\n";
    const char *body = "";

    // Set response content based on request path
    if (strcmp(path, "/") == 0) {
        // Web page request
        body = "<html><body><h1>Hello, World!</h1><p>This is request #%d</p></body></html>";
    } else if (strcmp(path, "/favicon.ico") == 0) {
        // favicon.ico request (return empty content)
        body = "";
        content_type = "image/x-icon";
    } else {
        // Unknown path (return 404 error)
        status_line = "HTTP/1.1 404 Not Found\r\n";
        body = "<html><body><h1>404 Not Found</h1></body></html>";
    }

    // Generate HTTP response headers
    snprintf(response, sizeof(response),
            "%s"
            "Content-Type: %s\r\n"
            "\r\n", // Empty line separates headers from body
            status_line, content_type);

    // Generate HTTP body
    if (strlen(body) > 0) {
        char body_buffer[BUFFER_SIZE];
        snprintf(body_buffer, sizeof(body_buffer), body, current_response_number);
        strncat(response, body_buffer, sizeof(response) - strlen(response) - 1);
    }

    // Send response
    send(client_socket, response, strlen(response), 0);
    printf("Sent response:\n<<<<<<<<<<<<<<<<<<<<\n%s\n>>>>>>>>>>>>>>>>>>>>\n", response);
    close(client_socket);
}

// Handle client request
void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    free(arg); // Free dynamically allocated memory

    char receive_buffer[BUFFER_SIZE];
    ssize_t bytes_received = recv(client_socket, receive_buffer, BUFFER_SIZE, 0);
    if (bytes_received < 0) {
        perror("Failed to receive data from client");
        close(client_socket);
        return NULL;
    } else if (bytes_received == 0) {
        printf("Client disconnected\n");
        close(client_socket);
        return NULL;
    }

    // Print client request
    receive_buffer[bytes_received] = '\0';
    printf("Received request:\n<<<<<<<<<<<<<<<<<<<<\n%s\n>>>>>>>>>>>>>>>>>>>>\n", receive_buffer);

    // Extract request path
    char *path = strtok(receive_buffer, " "); // Get request method (e.g., GET)
    if (path != NULL) {
        path = strtok(NULL, " "); // Get request path (e.g., / or /favicon.ico)
    }

    // Default to root path if no path is provided
    if (path == NULL) {
        path = "/";
    }

    // Get response number
    pthread_mutex_lock(&response_count_mutex);
    int current_response_number = ++response_count;
    pthread_mutex_unlock(&response_count_mutex);

    // Send HTTP response
    send_http_response(client_socket, path, current_response_number);

    return NULL; // Ensure all paths have a return value
}

int main() {
    // Register signal handlers
    signal(SIGINT, handle_signal);  // Capture Ctrl+C signal
    signal(SIGTERM, handle_signal); // Capture termination signal

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Failed to create socket");
        return 1;
    }

    // Set SO_REUSEADDR option
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Failed to set SO_REUSEADDR");
        close(server_socket);
        return 1;
    }

    // Bind socket to port
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Failed to bind socket");
        close(server_socket);
        return 1;
    }

    // Listen for connections
    if (listen(server_socket, 5) < 0) {
        perror("Failed to listen on socket");
        close(server_socket);
        return 1;
    }

    printf("Server is listening on port %d...\n", PORT);

    // Accept client connections
    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_address_size = sizeof(client_address);
        int *client_socket = malloc(sizeof(int)); // Dynamically allocate memory for client socket
        if (client_socket == NULL) {
            perror("Failed to allocate memory for client socket");
            continue;
        }

        *client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_size);
        if (*client_socket < 0) {
            perror("Failed to accept client connection");
            free(client_socket);
            continue;
        }

        char client_ip[INET_ADDRSTRLEN]; // Store dotted-decimal string
        inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("Client connected: %s\n", client_ip);

        // Create a new thread to handle client request
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, client_socket) != 0) {
            perror("Failed to create thread");
            close(*client_socket);
            free(client_socket);
        } else {
            pthread_detach(thread_id); // Detach thread to let it run independently
        }
    }

    // Close server socket (this will not be reached normally)
    close(server_socket);
    return 0;
}
