#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <errno.h>
#include <libgen.h>
#include <dirent.h>

#define PORT 5345        // Port for Smain server
#define BUFFER_SIZE 1024 // Buffer size for communication
#define PDF_PORT 6543    // Port for Spdf server
#define TXT_PORT 7256    // Port for Stxt server

// Ensure the full path directory exists
void ensure_directory_exists(const char *path)
{
    char temp_path[BUFFER_SIZE];
    snprintf(temp_path, sizeof(temp_path), "%s", path);

    for (char *ptr = temp_path + 1; *ptr; ptr++)
    {
        if (*ptr == '/')
        {
            *ptr = '\0';
            mkdir(temp_path, 0755); // Create directory with rwxr-xr-x permissions
            *ptr = '/';
        }
    }

    mkdir(temp_path, 0755); // Create the final directory if it doesn't exist
}

// Function to check if a path is absolute
int is_absolute_path(const char *path)
{
    return path[0] == '/';
}

// Function to get the list of .c files from the specified directory in Smain
void get_c_files(const char *directory_path, char *file_list)
{
    DIR *dir;
    struct dirent *entry;

    if ((dir = opendir(directory_path)) == NULL)
    {
        perror("opendir");
        return;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_DIR)
        {
            // Skip '.' and '..' directories
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            // Recursively search in subdirectories
            char subdir_path[BUFFER_SIZE];
            snprintf(subdir_path, sizeof(subdir_path), "%s/%s", directory_path, entry->d_name);
            get_c_files(subdir_path, file_list);
        }
        else if (entry->d_type == DT_REG) // Check if it's a regular file
        {
            const char *ext = strrchr(entry->d_name, '.');
            if (ext && strcmp(ext, ".c") == 0)
            {
                // Only append the filename, not the path
                strcat(file_list, entry->d_name);
                strcat(file_list, "\n");
            }
        }
    }

    closedir(dir);
}

// Function to forward the file command to the appropriate server
void forward_command_to_server(const char *server_ip, int server_port, const char *command, int client_socket)
{
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};

    // Create a socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation error");
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);

    // Convert IP address to binary form
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0)
    {
        perror("Invalid address/ Address not supported");
        close(sock);
        return;
    }

    // Connect to the specified server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Connection failed");
        close(sock);
        return;
    }

    // Send the command to the server
    if (send(sock, command, strlen(command), 0) < 0)
    {
        perror("Send error");
        close(sock);
        return;
    }

    // Receive the response from the server and forward it to the client
    int valread;
    while ((valread = read(sock, buffer, BUFFER_SIZE)) > 0)
    {
        if (send(client_socket, buffer, valread, 0) < 0)
        {
            perror("Error forwarding data to client");
            break;
        }
    }

    // Close the server socket
    close(sock);
}

// Function to handle the display command
void handle_display_command(int client_socket, const char *pathname)
{
    char file_list[BUFFER_SIZE * 10] = {0}; // Large buffer to store the file list

    // Construct the full file path
    char full_path[BUFFER_SIZE];
    snprintf(full_path, BUFFER_SIZE, "/home/parmar8a/smain/%s", pathname + 8);

    printf("fullpath : %s\n", full_path);

    // Get list of .c files from Smain
    get_c_files(full_path, file_list);

    // Send the consolidated file list to the client
    if (send(client_socket, file_list, strlen(file_list), 0) < 0)
    {
        perror("Send error");
    }
}

// Function to send tar file to the client
void send_file(int client_socket, const char *filepath)
{
    FILE *file = fopen(filepath, "rb");
    if (!file)
    {
        perror("File open error");
        send(client_socket, "ERROR: File not found\n", 22, 0);
        return;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;

    // Send file contents
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0)
    {
        if (send(client_socket, buffer, bytes_read, 0) < 0)
        {
            perror("Send error");
            break;
        }
    }

    fclose(file);
}

// Function to handle file deletion commands
void handle_rmfile(int client_socket, const char *filename)
{
    // Construct the full file path
    char full_path[BUFFER_SIZE];
    snprintf(full_path, BUFFER_SIZE, "/home/parmar8a/smain/%s", filename + 8);

    // Extract the file extension
    char *file_extension = strrchr(full_path, '.');

    if (file_extension && strcmp(file_extension, ".c") == 0)
    {
        // Handle .c files locally
        if (remove(full_path) == 0)
        {
            char *success_msg = "File deleted successfully\n";
            send(client_socket, success_msg, strlen(success_msg), 0);
        }
        else
        {
            char error_msg[BUFFER_SIZE];
            snprintf(error_msg, sizeof(error_msg), "Error: Something went wrong while deleting");
            send(client_socket, error_msg, strlen(error_msg), 0);
        }
    }
    else if (file_extension && strcmp(file_extension, ".pdf") == 0)
    {
        // Handle .pdf files - forward to Spdf server
        char command[BUFFER_SIZE];
        snprintf(command, sizeof(command), "rmfile %s", filename);
        forward_command_to_server("127.0.0.1", PDF_PORT, command, client_socket);

        // Provide a response to the client
        char *success_msg = "Delete request forwarded to Spdf server\n";
        send(client_socket, success_msg, strlen(success_msg), 0);
    }
    else if (file_extension && strcmp(file_extension, ".txt") == 0)
    {
        // Handle .txt files - forward to Stxt server
        char command[BUFFER_SIZE];
        snprintf(command, sizeof(command), "rmfile %s", filename);
        forward_command_to_server("127.0.0.1", TXT_PORT, command, client_socket);

        // Provide a response to the client
        char *success_msg = "Delete request forwarded to Stxt server\n";
        send(client_socket, success_msg, strlen(success_msg), 0);
    }
    else
    {
        char *error_msg = "Unsupported file type\n";
        send(client_socket, error_msg, strlen(error_msg), 0);
    }
}

// Function to create a tar file of .c files in ~/smain
void create_tar_of_c_files(const char *tar_filename)
{
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "find /home/parmar8a/smain -type f -name '*.c' | xargs tar -cvf %s", tar_filename);
    int result = system(command);
    if (result == -1)
    {
        perror("Error creating tar file");
    }
}

// Function to request tar files from Spdf or Stext servers
void request_tar_from_server(const char *server_ip, int server_port, const char *command, const char *tar_filename, int client_socket)
{
    int sock;
    struct sockaddr_in server_address;
    char buffer[BUFFER_SIZE] = {0};
    int bytes_received;

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation error");
        return;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, server_ip, &server_address.sin_addr) <= 0)
    {
        perror("Invalid address / Address not supported");
        close(sock);
        return;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("Connection Failed");
        close(sock);
        return;
    }

    // Send command to the server
    send(sock, command, strlen(command), 0);

    // Receive the tar file from the server
    FILE *file = fopen(tar_filename, "wb");
    if (!file)
    {
        perror("File open error");
        close(sock);
        return;
    }

    while ((bytes_received = recv(sock, buffer, BUFFER_SIZE, 0)) > 0)
    {
        fwrite(buffer, 1, bytes_received, file);
    }

    fclose(file);
    close(sock);

    // Send the tar file to the client
    send_file(client_socket, tar_filename);
    // remove(tar_filename); // Clean up the tar file after sending
}

// Function to process client commands
void prcclient(int client_socket)
{
    char buffer[BUFFER_SIZE] = {0};
    int valread;

    // Read the command from the client
    valread = read(client_socket, buffer, BUFFER_SIZE);
    if (valread <= 0)
    {
        perror("read");
        return;
    }
    buffer[valread] = '\0';
    printf("Received command: %s\n", buffer);

    // Parse the command
    char *command = strtok(buffer, " ");
    if (strcmp(command, "ufile") == 0)
    {
        char *filename = strtok(NULL, " ");
        char *destination = strtok(NULL, " ");

        if (filename && destination)
        {
            // Trim any newline characters from the destination path
            destination[strcspn(destination, "\n")] = 0;

            // Handle relative paths by converting them to absolute paths
            char absolute_path[BUFFER_SIZE];
            if (!is_absolute_path(filename))
            {
                // Get the current working directory and construct the absolute path
                if (getcwd(absolute_path, sizeof(absolute_path)) == NULL)
                {
                    perror("getcwd() error");
                    return;
                }
                strncat(absolute_path, "/", sizeof(absolute_path) - strlen(absolute_path) - 1);
                strncat(absolute_path, filename, sizeof(absolute_path) - strlen(absolute_path) - 1);
            }
            else
            {
                // If the path is already absolute, just copy it
                strncpy(absolute_path, filename, sizeof(absolute_path) - 1);
            }

            // Extract the file extension
            char *file_extension = strrchr(absolute_path, '.');

            if (file_extension && strcmp(file_extension, ".c") == 0)
            {
                // Ensure the destination directory exists
                char dir_path[BUFFER_SIZE];

                // Construct the full file path
                char full_path[BUFFER_SIZE];
                if (strcmp(destination + 8, "") != 0)
                {
                    snprintf(full_path, BUFFER_SIZE, "/home/parmar8a/smain/%s/%s", destination + 8, basename(absolute_path));
                }
                else
                {
                    snprintf(full_path, BUFFER_SIZE, "/home/parmar8a/smain/%s%s", destination + 8, basename(absolute_path));
                }

                snprintf(dir_path, sizeof(dir_path), "/home/parmar8a/smain/%s", destination + 8);
                ensure_directory_exists(dir_path);

                // Read the file content from the client
                char file_buffer[BUFFER_SIZE] = {0};
                int file_size = read(client_socket, file_buffer, BUFFER_SIZE);

                FILE *file = fopen(full_path, "wb");
                if (!file)
                {
                    perror("File open error");
                    return;
                }
                fwrite(file_buffer, 1, file_size, file);
                fclose(file);
                printf("C File stored at: %s\n", full_path);

                // Remove the local file
                if (remove(absolute_path) != 0)
                {
                    perror("Error deleting the file");
                }
            }
            else if (file_extension && strcmp(file_extension, ".pdf") == 0)
            {
                // Handle .pdf files - forward to Spdf server
                snprintf(buffer, sizeof(buffer), "ufile %s %s", basename(absolute_path), destination);
                forward_command_to_server("127.0.0.1", PDF_PORT, buffer, client_socket);
            }
            else if (file_extension && strcmp(file_extension, ".txt") == 0)
            {
                // Handle .txt files - forward to Stxt server
                snprintf(buffer, sizeof(buffer), "ufile %s %s", basename(absolute_path), destination);
                forward_command_to_server("127.0.0.1", TXT_PORT, buffer, client_socket);
            }
            else
            {
                char *error_msg = "Unsupported file type\n";
                send(client_socket, error_msg, strlen(error_msg), 0);
            }
        }
        else
        {
            char *error_msg = "Invalid ufile command syntax\n";
            send(client_socket, error_msg, strlen(error_msg), 0);
        }
    }
    else if (strcmp(command, "dfile") == 0)
    {
        char *filename = strtok(NULL, " ");
        if (filename)
        {
            // Construct the full file path
            char full_path[BUFFER_SIZE];
            snprintf(full_path, BUFFER_SIZE, "/home/parmar8a/smain/%s", filename + 8);

            // Extract the file extension
            char *file_extension = strrchr(full_path, '.');

            if (file_extension && strcmp(file_extension, ".c") == 0)
            {
                // Handle .c files locally
                FILE *file = fopen(full_path, "rb");
                if (file == NULL)
                {
                    perror("File open error");
                    char *error_msg = "Error: File not found\n";
                    send(client_socket, error_msg, strlen(error_msg), 0);
                    return;
                }

                // Send the file to the client
                char file_buffer[BUFFER_SIZE];
                size_t n;
                while ((n = fread(file_buffer, 1, BUFFER_SIZE, file)) > 0)
                {
                    send(client_socket, file_buffer, n, 0);
                }
                fclose(file);
            }
            else if (file_extension && strcmp(file_extension, ".pdf") == 0)
            {
                // Handle .pdf files - forward to Spdf server
                snprintf(buffer, sizeof(buffer), "dfile %s", filename);
                forward_command_to_server("127.0.0.1", PDF_PORT, buffer, client_socket);
            }
            else if (file_extension && strcmp(file_extension, ".txt") == 0)
            {
                // Handle .txt files - forward to Stxt server
                snprintf(buffer, sizeof(buffer), "dfile %s", filename);
                forward_command_to_server("127.0.0.1", TXT_PORT, buffer, client_socket);
            }
            else
            {
                char *error_msg = "Unsupported file type\n";
                send(client_socket, error_msg, strlen(error_msg), 0);
            }
        }
        else
        {
            char *error_msg = "Invalid dfile command syntax\n";
            send(client_socket, error_msg, strlen(error_msg), 0);
        }
    }
    else if (strcmp(command, "rmfile") == 0)
    {
        char *filename = strtok(NULL, " ");
        if (filename)
        {
            handle_rmfile(client_socket, filename);
        }
        else
        {
            char *error_msg = "Invalid rmfile command syntax\n";
            send(client_socket, error_msg, strlen(error_msg), 0);
        }
    }
    else if (strcmp(command, "dtar") == 0)
    {
        char *filename = strtok(NULL, " ");
        if (strcmp(filename, ".c") == 0)
        {
            // Create tar file for .c files
            const char *tar_filename = "cfiles.tar";
            create_tar_of_c_files(tar_filename);

            // Send tar file to client
            send_file(client_socket, tar_filename);
            // remove(tar_filename); // Clean up after sending
        }
        else if (strcmp(filename, ".pdf") == 0)
        {
            // Request tar file from Spdf server
            request_tar_from_server("127.0.0.1", PDF_PORT, "dtar .pdf", "pdf.tar", client_socket);
        }
        else if (strcmp(filename, ".txt") == 0)
        {
            // Request tar file from Stext server
            request_tar_from_server("127.0.0.1", TXT_PORT, "dtar .txt", "text.tar", client_socket);
        }
        else
        {
            fprintf(stderr, "Unsupported file type: %s\n", filename);
        }
    }
    else if (strcmp(command, "display") == 0)
    {
        char *filename = strtok(NULL, " ");
        // Handle display command
        handle_display_command(client_socket, filename);
    }
    else
    {
        char *error_msg = "Unknown command\n";
        send(client_socket, error_msg, strlen(error_msg), 0);
    }
}

int main()
{
    int server_socket, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Create socket file descriptor
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(PORT);

    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the port
    if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_socket, 3) < 0)
    {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Smain server listening on port %d\n", PORT);

    // Main server loop
    while (1)
    {
        // Accept incoming connection
        if ((new_socket = accept(server_socket, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            perror("Accept failed");
            continue;
        }

        // Process client commands
        prcclient(new_socket);

        // Close the client socket
        close(new_socket);
    }

    return 0;
}
