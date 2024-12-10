#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PORT 8080
#define STATIC_DIR "./public"

// Function to get the MIME type based on file extension
const char *get_mime_type(const char *filename) {
    if (strstr(filename, ".html") || strstr(filename, ".htm")) return "text/html";
    if (strstr(filename, ".css")) return "text/css";
    if (strstr(filename, ".js")) return "application/javascript";
    if (strstr(filename, ".jpg") || strstr(filename, ".jpeg")) return "image/jpeg";
    if (strstr(filename, ".png")) return "image/png";
    if (strstr(filename, ".gif")) return "image/gif";
    if (strstr(filename, ".json")) return "application/json";
    return "application/octet-stream";  // Default to binary data if unknown
}

// Function to serve static files (including contact.html)
void handle_get_static(int client_fd, const char *path) {
    char file_path[1024];
    snprintf(file_path, sizeof(file_path), "%s%s", STATIC_DIR, path);

    // Open the file
    int file = open(file_path, O_RDONLY);
    if (file == -1) {
        const char *response = 
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n\r\n"
            "<html><body><h1>404 Not Found</h1></body></html>";
        write(client_fd, response, strlen(response));
        return;
    }

    // Get file size
    struct stat st;
    fstat(file, &st);
    size_t file_size = st.st_size;

    // Determine MIME type
    const char *mime_type = get_mime_type(file_path);

    // Send response header
    char header[256];
    snprintf(header, sizeof(header), 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n\r\n", mime_type, file_size);
    write(client_fd, header, strlen(header));

    // Send file content
    char buffer[1024];
    ssize_t bytes_read;
    while ((bytes_read = read(file, buffer, sizeof(buffer))) > 0) {
        write(client_fd, buffer, bytes_read);
    }

    // Close the file
    close(file);
}

// Function to run Ruby script and handle output
void run_ruby_script(const char *script_path, const char *name, const char *email, const char *message, int client_fd) {
    char command[512];
    snprintf(command, sizeof(command), "ruby %s \"%s\" \"%s\" \"%s\"", script_path, name, email, message);
    
    // Run the Ruby script
    int ret = system(command);
    if (ret != 0) {
        const char *error_response = 
            "HTTP/1.1 500 Internal Server Error\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n\r\n"
            "<html><body><h1>500 Internal Server Error</h1></body></html>";
        write(client_fd, error_response, strlen(error_response));
        return;
    }

    // Send HTTP response header
    const char *response_header = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n\r\n";
    write(client_fd, response_header, strlen(response_header));

    // Send a simple thank you message
    const char *response_body =
        "<html><body><h1>Thank you for your message!</h1></body></html>";
    write(client_fd, response_body, strlen(response_body));
}

// Function to handle the request
void handle_request(int client_fd) {
    char buffer[1024];
    read(client_fd, buffer, sizeof(buffer) - 1);

    char method[8], path[256];
    sscanf(buffer, "%s %s", method, path);

    if (strcmp(method, "GET") == 0) {
        if (strcmp(path, "/contact") == 0) {
            handle_get_static(client_fd, "/contact.html");  // Serve the contact form
        } else if (strcmp(path, "/") == 0) {
            handle_get_static(client_fd, "/index.html");  // Serve the default index.html
        } else {
            handle_get_static(client_fd, path);  // Serve other static files
        }
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/contact") == 0) {
        // Extract form data from the request
        char *name = NULL, *email = NULL, *message = NULL;

        // Simple parser to extract the form fields (this can be improved)
        char *body = strstr(buffer, "\r\n\r\n") + 4;  // Skip headers
        name = strstr(body, "name=") + 5;
        email = strstr(body, "email=") + 6;
        message = strstr(body, "message=") + 8;

        if (name && email && message) {
            // Call the Ruby script to process the form (pass the extracted data)
            char script_path[] = "./process_contact.rb";
            run_ruby_script(script_path, name, email, message, client_fd);
        } else {
            const char *error_response = 
                "HTTP/1.1 400 Bad Request\r\n"
                "Content-Type: text/html\r\n"
                "Connection: close\r\n\r\n"
                "<html><body><h1>400 Bad Request</h1><p>Missing required fields.</p></body></html>";
            write(client_fd, error_response, strlen(error_response));
        }
    } else {
        const char *response = 
            "HTTP/1.1 405 Method Not Allowed\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n\r\n"
            "<html><body><h1>405 Method Not Allowed</h1></body></html>";
        write(client_fd, response, strlen(response));
    }
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set up the address structure
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces
    address.sin_port = htons(PORT);

    // Bind the socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", PORT);

    // Accept incoming connections
    while ((client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) >= 0) {
        handle_request(client_fd);
        close(client_fd);  // Close the connection after handling the request
    }

    if (client_fd < 0) {
        perror("accept failed");
        exit(EXIT_FAILURE);
    }

    return 0;
}
