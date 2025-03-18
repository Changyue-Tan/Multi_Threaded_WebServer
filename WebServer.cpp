#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <thread>
#include <mutex>
#include <memory>
#include <atomic>

#define PORT 8080        // Port to listen for incoming connections
#define BUFFER_SIZE 4096 // Buffer size for receiving data

// Global variables
int server_socket;                     // Server socket
std::atomic<int> response_count(0);     // Request counter, atomic for thread safety
std::mutex response_count_mutex;       // Mutex for thread-safe access to response_count

// Signal handler function to gracefully handle termination signals
void handle_signal(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nReceived SIGINT (Ctrl+C). Stopping server...\n";
    } else if (signal == SIGTERM) {
        std::cout << "\nReceived SIGTERM. Stopping server...\n";
    }
    
    // Clean up resources before exiting
    std::cout << "Closing server socket...\n";
    close(server_socket);
    std::cout << "Server stopped.\n";
    exit(0);
}

// Function to send an HTTP response to the client
void send_http_response(int client_socket, const std::string_view& path, int current_response_number) {
    std::string response;
    std::string content_type = "text/html";  // Default content type for HTML
    std::string status_line = "HTTP/1.1 200 OK\r\n"; // Default status line
    std::string body = "";

    // Handle different request paths
    if (path == "/") {
        // Web page request
        body = "<html><body><h1>Hello, World!</h1><p>This is request #" + std::to_string(current_response_number) + "</p></body></html>";
    } else if (path == "/favicon.ico") {
        // Favicon request (return empty content)
        body = "";
        content_type = "image/x-icon"; // Content type for favicon
    } else {
        // 404 Not Found for unknown paths
        status_line = "HTTP/1.1 404 Not Found\r\n";
        body = "<html><body><h1>404 Not Found</h1></body></html>";
    }

    // Prepare the HTTP response headers
    std::ostringstream response_stream;
    response_stream << status_line
                   << "Content-Type: " << content_type << "\r\n"
                   << "\r\n"; // Empty line separates headers from body

    // Append the body
    response_stream << body;
    
    // Send the HTTP response
    response = response_stream.str();
    send(client_socket, response.c_str(), response.length(), 0);
    std::cout << "Sent response:\n<<<<<<<<<<<<<<<<<<<<\n" << response << "\n>>>>>>>>>>>>>>>>>>>>\n";
    close(client_socket); // Close client socket after sending the response
}

// Function to handle each client request in a separate thread
void handle_client(int client_socket) {
    char receive_buffer[BUFFER_SIZE];
    ssize_t bytes_received = recv(client_socket, receive_buffer, BUFFER_SIZE, 0);
    
    // Handle reception errors
    if (bytes_received < 0) {
        perror("Failed to receive data from client");
        close(client_socket);
        return;
    } else if (bytes_received == 0) {
        std::cout << "Client disconnected\n";
        close(client_socket);
        return;
    }

    // Print the received request
    receive_buffer[bytes_received] = '\0'; // Null-terminate the received data
    std::cout << "Received request:\n<<<<<<<<<<<<<<<<<<<<\n" << receive_buffer << "\n>>>>>>>>>>>>>>>>>>>>\n";

    // Extract the request path (e.g., / or /favicon.ico)
    std::string path;
    std::istringstream request_stream(receive_buffer);
    std::string method; // HTTP method (e.g., GET)
    request_stream >> method >> path;

    // Default to root path if no path is provided
    if (path.empty()) {
        path = "/";
    }

    // Lock the mutex to safely increment the response count
    std::lock_guard<std::mutex> lock(response_count_mutex);
    int current_response_number = ++response_count;

    // Send the HTTP response to the client
    send_http_response(client_socket, path, current_response_number);
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

    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
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

    std::cout << "Server is listening on port " << PORT << "...\n";

    // Main server loop to accept client connections
    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_address_size = sizeof(client_address);
        int client_socket = accept(server_socket, (struct sockaddr*)&client_address, &client_address_size);
        if (client_socket < 0) {
            perror("Failed to accept client connection");
            continue;
        }

        // Get client IP address and display it
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::cout << "Client connected: " << client_ip << "\n";

        // Create a thread to handle the client request
        std::thread(handle_client, client_socket).detach(); // Detach the thread so it runs independently
    }

    // This code will not be reached as the server runs in an infinite loop
    close(server_socket);
    return 0;
}
