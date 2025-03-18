#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080        // Port to listen for incoming connections
#define BUFFER_SIZE 4096 // Buffer size for receiving data

// Global variables
int server_socket;                     // Server socket
int response_count = 0;                 // Request counter
pthread_mutex_t response_count_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for thread-safe access to response_count

// Signal handler function to gracefully handle termination signals
void handle_signal(int signal) {
    if (signal == SIGINT) {
        printf("\nReceived SIGINT (Ctrl+C). Stopping server...\n");
    } else if (signal == SIGTERM) {
        printf("\nReceived SIGTERM. Stopping server...\n");
    }
    
    // Clean up resources before exiting
    printf("Closing server socket...\n");
    close(server_socket);
    printf("Server stopped.\n");
    exit(0);
}

// Function to send an HTTP response to the client
void send_http_response(int client_socket, const char *path, int current_response_number) {
    char response[BUFFER_SIZE];
    const char *content_type = "text/html";  // Default content type for HTML
    const char *status_line = "HTTP/1.1 200 OK\r\n"; // Default status line
    const char *body = "";

    // Handle different request paths
    if (strcmp(path, "/") == 0) {
        // Web page request
        body = "<html><body><h1>Hello, World!</h1><p>This is request #%d</p></body></html>";
    } else if (strcmp(path, "/favicon.ico") == 0) {
        // Favicon request (return empty content)
        body = "";
        content_type = "image/x-icon"; // Content type for favicon
    } else {
        // 404 Not Found for unknown paths
        status_line = "HTTP/1.1 404 Not Found\r\n";
        body = "<html><body><h1>404 Not Found</h1></body></html>";
    }

    // Prepare the HTTP response headers
    snprintf(response, sizeof(response),
            "%s"
            "Content-Type: %s\r\n"
            "\r\n", // Empty line separates headers from body
            status_line, content_type);

    // Generate HTTP body, including request count
    if (strlen(body) > 0) {
        char body_buffer[BUFFER_SIZE];
        snprintf(body_buffer, sizeof(body_buffer), body, current_response_number);

        // Ensure available space is calculated properly
        size_t available_space = sizeof(response) - strlen(response) - 1; // Reserve space for null terminator

        // If there's space left in the buffer
        if (available_space > 0) {
            // Use snprintf to append to response safely, but limit the size of the body being copied
            int ret = snprintf(response + strlen(response), available_space, "%s", body_buffer);

            // If ret is negative, there was an error with snprintf
            if (ret < 0) {
                fprintf(stderr, "Error: snprintf failed\n");
            } else if (ret >= (int)available_space) {
                fprintf(stderr, "Warning: response buffer overflow risk\n");
            }
        }
    }

    // Send the HTTP response
    send(client_socket, response, strlen(response), 0);
    printf("Sent response:\n<<<<<<<<<<<<<<<<<<<<\n%s\n>>>>>>>>>>>>>>>>>>>>\n", response);
    close(client_socket); // Close client socket after sending the response
}

// Function to handle each client request in a separate thread
void *handle_client(void *arg) {
    int client_socket = *(int *)arg; // Extract client socket
    free(arg); // Free dynamically allocated memory

    char receive_buffer[BUFFER_SIZE];
    ssize_t bytes_received = recv(client_socket, receive_buffer, BUFFER_SIZE, 0);
    
    // Handle reception errors
    if (bytes_received < 0) {
        perror("Failed to receive data from client");
        close(client_socket);
        return NULL;
    } else if (bytes_received == 0) {
        printf("Client disconnected\n");
        close(client_socket);
        return NULL;
    }

    // Print the received request
    receive_buffer[bytes_received] = '\0'; // Null-terminate the received data
    printf("Received request:\n<<<<<<<<<<<<<<<<<<<<\n%s\n>>>>>>>>>>>>>>>>>>>>\n", receive_buffer);

    // Extract the request path (e.g., / or /favicon.ico)
    char *path = strtok(receive_buffer, " "); // First token is the method (e.g., GET)
    if (path != NULL) {
        path = strtok(NULL, " "); // Second token is the request path
    }

    // Default to root path if no path is provided
    if (path == NULL) {
        path = "/";
    }

    // Lock the mutex to safely increment the response count
    pthread_mutex_lock(&response_count_mutex);
    int current_response_number = ++response_count;
    pthread_mutex_unlock(&response_count_mutex);

    // Send the HTTP response to the client
    send_http_response(client_socket, path, current_response_number);

    return NULL; // Ensure the thread terminates correctly
}

int main() {
    // Register signal handlers for graceful termination
    signal(SIGINT, handle_signal);  // Capture Ctrl+C signal
    signal(SIGTERM, handle_signal); // Capture termination signal

    // Create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Failed to create socket");
        return 1;
    }

    // Set socket options to allow reuse of address
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Failed to set SO_REUSEADDR");
        close(server_socket);
        return 1;
    }

    // Bind the server socket to a specific port and address
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY; // Bind to all available interfaces
    server_address.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Failed to bind socket");
        close(server_socket);
        return 1;
    }

    // Start listening for incoming connections
    if (listen(server_socket, 5) < 0) {
        perror("Failed to listen on socket");
        close(server_socket);
        return 1;
    }

    printf("Server is listening on port %d...\n", PORT);

    // Main server loop to accept client connections
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
            free(client_socket); // Free memory on failure
            continue;
        }

        // Get client IP address and display it
        char client_ip[INET_ADDRSTRLEN]; 
        inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("Client connected: %s\n", client_ip);

        // Create a new thread to handle the client request
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, client_socket) != 0) {
            perror("Failed to create thread");
            close(*client_socket);
            free(client_socket);
        } else {
            pthread_detach(thread_id); // Detach the thread so it runs independently
        }
    }

    // This code will not be reached as the server runs in an infinite loop
    close(server_socket);
    return 0;
}
