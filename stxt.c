#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <libgen.h>
#include <errno.h>
#include <dirent.h>

#define PORT 7256        // Port for Stxt server
#define BUFFER_SIZE 1024 // Buffer size for communication

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
    // Delete the file
    if (remove(filename) == 0)
    {
        char *success_msg = "File deleted successfully\n";
        send(client_socket, success_msg, strlen(success_msg), 0);
    }
    else
    {
        char error_msg[BUFFER_SIZE];
        snprintf(error_msg, sizeof(error_msg), "Error: %s\n", strerror(errno));
        send(client_socket, error_msg, strlen(error_msg), 0);
    }
}

// Function to create a tar file of .txt files in ~/stxt
void create_tar_of_pdf_files(const char *tar_filename)
{
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "find /home/parmar8a/stxt -type f -name '*.txt' | xargs tar -cvf %s", tar_filename);
    int result = system(command);
    if (result == -1)
    {
        perror("Error creating tar file");
    }
}

// Function to get the list of .txt files from the specified directory
void get_txt_files(const char *directory_path, char *file_list)
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
            get_txt_files(subdir_path, file_list);
        }
        else if (entry->d_type == DT_REG) // Check if it's a regular file
        {
            const char *ext = strrchr(entry->d_name, '.');
            if (ext && strcmp(ext, ".txt") == 0)
            {
                // Only append the filename, not the path
                strcat(file_list, entry->d_name);
                strcat(file_list, "\n");
            }
        }
    }

    closedir(dir);
}

// Function to handle the display command
void handle_display_command(int client_socket, const char *pathname)
{
    char file_list[BUFFER_SIZE * 10] = {0}; // Large buffer to store the file list

    printf("pathname : %s\n", pathname);

    // Construct the full file path
    char full_path[BUFFER_SIZE];
    snprintf(full_path, BUFFER_SIZE, "/home/parmar8a/stxt/%s", pathname + 8);

    printf("full_path : %s\n", full_path);

    // Get list of .txt files from Stxt
    get_txt_files(full_path, file_list);

    // Send the consolidated file list to Smain
    if (send(client_socket, file_list, strlen(file_list), 0) < 0)
    {
        perror("Send error");
    }
}

void handle_client(int client_socket)
{
    char buffer[BUFFER_SIZE] = {0}; // Buffer to store command data
    char command[BUFFER_SIZE] = {0};
    char filename[BUFFER_SIZE] = {0};
    char destination_path[BUFFER_SIZE] = {0};
    int valread;

    // Read the command from Smain
    valread = read(client_socket, buffer, BUFFER_SIZE);
    buffer[valread] = '\0';

    // Parse the command to get the action, filename, and destination path
    sscanf(buffer, "%s %s %s", command, filename, destination_path);

    if (strcmp(command, "ufile") == 0)
    {
        // Construct the full destination path
        char full_destination_path[BUFFER_SIZE];
        snprintf(full_destination_path, BUFFER_SIZE, "/home/parmar8a/stxt/%s", destination_path + 8);

        // Ensure destination directory exists
        ensure_directory_exists(full_destination_path);

        // Construct the full file path
        char full_path[BUFFER_SIZE];
        if (strcmp(destination_path + 8, "") != 0)
        {
            snprintf(full_path, BUFFER_SIZE, "%s/%s", full_destination_path, basename(filename));
        }
        else
        {
            snprintf(full_path, BUFFER_SIZE, "%s%s", full_destination_path, basename(filename));
        }

        // Open the source file to read data
        FILE *source_file = fopen(filename, "rb");
        if (!source_file)
        {
            perror("Source file open error");
            return;
        }

        // Open file to write data to destination
        FILE *dest_file = fopen(full_path, "wb");
        if (!dest_file)
        {
            perror("Destination file open error");
            fclose(source_file);
            return;
        }

        // Read from source and write to destination
        while ((valread = fread(buffer, 1, BUFFER_SIZE, source_file)) > 0)
        {
            fwrite(buffer, 1, valread, dest_file);
        }

        // Close files
        fclose(source_file);
        fclose(dest_file);
        printf("File stored at: %s\n", full_path);

        // Remove the local file
        if (remove(filename) != 0)
        {
            perror("Error deleting the file");
        }
    }
    else if (strcmp(command, "dfile") == 0)
    {
        printf("Command Received: %s\n", command);

        // Handle dfile command
        char full_path[BUFFER_SIZE];
        snprintf(full_path, BUFFER_SIZE, "/home/parmar8a/stxt/%s", filename + 8);

        printf("full path: %s\n", full_path);

        // Check if file exists and send it
        struct stat st;
        if (stat(full_path, &st) == 0)
        {
            send_file(client_socket, full_path);
        }
        else
        {
            perror("File not found");
            send(client_socket, "ERROR: File not found\n", 22, 0);
        }
    }
    else if (strcmp(command, "rmfile") == 0)
    {
        printf("Command Received: %s\n", command);
        // Handle dfile command
        char full_path[BUFFER_SIZE];
        snprintf(full_path, BUFFER_SIZE, "/home/parmar8a/stxt/%s", filename + 8);

        printf("full path: %s\n", full_path);

        // char *filename = strtok(NULL, " ");
        if (full_path)
        {
            handle_rmfile(client_socket, full_path);
        }
        else
        {
            char *error_msg = "Invalid rmfile command syntax\n";
            send(client_socket, error_msg, strlen(error_msg), 0);
        }
    }
    else if (strcmp(command, "dtar") == 0)
    {
        printf("Command Received: %s\n", command);

        // char *filename = strtok(NULL, " ");

        if (filename && strcmp(filename, ".txt") == 0)
        {
            // Create tar file for .txt files
            const char *tar_filename = "text.tar";
            create_tar_of_pdf_files(tar_filename);

            // Send tar file to Smain
            send_file(client_socket, tar_filename);
            // remove(tar_filename); // Clean up after sending
        }
        else
        {
            char *error_msg = "Unsupported file type for tar\n";
            send(client_socket, error_msg, strlen(error_msg), 0);
        }
    }
    else if (strcmp(command, "display") == 0)
    {
        printf("command: %s", command);
        char *filename = strtok(NULL, " ");

        // Handle display command
        handle_display_command(client_socket, filename);
    }
    else
    {
        fprintf(stderr, "Invalid command received.\n");
    }

    // Close the client socket
    close(client_socket);
}

int main()
{
    int server_socket, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Creating the socket file descriptor for the Stxt server
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Configuring the address structure for the Stxt server
    address.sin_family = AF_INET;         // IPv4
    address.sin_addr.s_addr = INADDR_ANY; // Bind to any available interface
    address.sin_port = htons(PORT);       // Convert port number to network byte order

    // Binding the socket to the network address and port
    if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections on the Stxt server
    if (listen(server_socket, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Stxt server is listening on port %d\n", PORT);

    // Infinite loop to accept and process incoming client connections
    while (1)
    {
        // Accepting a new connection from a client
        if ((new_socket = accept(server_socket, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        // Forking a new process to handle the client connection
        if (fork() == 0)
        {
            // In the child process: Close the listening socket and handle the client
            close(server_socket);
            handle_client(new_socket);
            close(new_socket);
            exit(0);
        }
        else
        {
            // In the parent process: Close the connected socket and continue accepting connections
            close(new_socket);
        }
    }

    return 0;
}
