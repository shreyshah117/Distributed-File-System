#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>

#define PORT 5345        // Port to connect to Smain server
#define BUFFER_SIZE 1024 // Buffer size for file operations

// Function to receive file from server and create it only when content is received
void receive_file(int socket, const char *filename)
{
    FILE *file = NULL;
    char buffer[BUFFER_SIZE];
    int bytes;
    int file_created = 0;

    while ((bytes = recv(socket, buffer, BUFFER_SIZE, 0)) > 0)
    {
        if (!file_created)
        {
            file = fopen(filename, "wb");
            if (!file)
            {
                perror("File open error");
                return;
            }
            file_created = 1; // Mark that file is created
        }
        fwrite(buffer, 1, bytes, file);
    }

    if (file)
    {
        fclose(file);
        printf("File received and saved as %s\n", filename);
    }
    else
    {
        printf("No content received; file not created.\n");
    }
}

// Function to receive a list of filenames from the server
void receive_file_list(int socket)
{
    char buffer[BUFFER_SIZE];
    int bytes;

    printf("Files in the directory:\n");
    while ((bytes = recv(socket, buffer, BUFFER_SIZE - 1, 0)) > 0)
    {
        buffer[bytes] = '\0'; // Null-terminate the received string
        printf("%s", buffer);
    }

    if (bytes < 0)
    {
        perror("Receive error");
    }
}

int main()
{
    struct sockaddr_in server_address;
    char command[BUFFER_SIZE];
    char filename[BUFFER_SIZE];
    char destination[BUFFER_SIZE];

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) <= 0)
    {
        perror("Invalid address / Address not supported");
        return -1;
    }

    while (1)
    {
        int client_socket;

        // Create socket
        if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            perror("Socket creation error");
            return -1;
        }

        // Connect to the Smain server
        if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
        {
            perror("Connection Failed");
            close(client_socket);
            break; // Retry connecting
        }

        // Prompt user for command input
        printf("client24s$ ");
        fflush(stdout);

        // Read command from the user
        if (fgets(command, sizeof(command), stdin) == NULL)
        {
            perror("fgets error");
            close(client_socket);
            continue;
        }

        // Remove newline character from the end of the input
        command[strcspn(command, "\n")] = 0;

        // Handle the ufile command
        if (sscanf(command, "ufile %s %s", filename, destination) == 2)
        {
            // Open the file to be sent
            FILE *file = fopen(filename, "rb");
            if (!file)
            {
                perror("File open error");
                close(client_socket);
                continue;
            }

            // Send the command
            if (send(client_socket, command, strlen(command), 0) < 0)
            {
                perror("Command send error");
                close(client_socket);
                fclose(file);
                continue;
            }

            // Send a delimiter to indicate the end of the command
            char delimiter[] = "\n";
            if (send(client_socket, delimiter, strlen(delimiter), 0) < 0)
            {
                perror("Delimiter send error");
                close(client_socket);
                fclose(file);
                continue;
            }

            // Send file contents
            char buffer[BUFFER_SIZE];
            size_t bytes;
            while ((bytes = fread(buffer, 1, BUFFER_SIZE, file)) > 0)
            {
                if (send(client_socket, buffer, bytes, 0) < 0)
                {
                    perror("send");
                    close(client_socket);
                    fclose(file);
                    exit(EXIT_FAILURE);
                }
                memset(buffer, 0, BUFFER_SIZE); // Clear the buffer
            }
            printf("File sent successfully.\n");

            // Close the file
            fclose(file);
        }
        // Handle the dfile command
        else if (sscanf(command, "dfile %s", filename) == 1)
        {
            // Send the command
            if (send(client_socket, command, strlen(command), 0) < 0)
            {
                perror("Command send error");
                close(client_socket);
                continue;
            }

            // Remove the path and keep the filename only for saving the received file
            char *basename = strrchr(filename, '/');
            if (basename)
            {
                basename++; // Skip the '/'
            }
            else
            {
                basename = filename; // No '/' found, the filename is already the base name
            }

            // Receive the file from the server
            receive_file(client_socket, basename);
        }
        // Handle the rmfile command
        else if (sscanf(command, "rmfile %s", filename) == 1)
        {
            // Send the command to Smain
            if (send(client_socket, command, strlen(command), 0) < 0)
            {
                perror("Command send error");
                close(client_socket);
                continue;
            }

            // Receive response from server
            char response[BUFFER_SIZE];
            int valread = recv(client_socket, response, BUFFER_SIZE, 0);
            if (valread > 0)
            {
                response[valread] = '\0';
                printf("%s\n", response);
            }
        }
        // Handle the dtar command
        else if (sscanf(command, "dtar %s", filename) == 1)
        {
            // Send the command to Smain
            if (send(client_socket, command, strlen(command), 0) < 0)
            {
                perror("Command send error");
                close(client_socket);
                continue;
            }

            // Determine the tar file name based on the file type
            char *tar_filename;
            if (strcmp(filename, ".c") == 0)
            {
                tar_filename = "cfiles.tar";
            }
            else if (strcmp(filename, ".pdf") == 0)
            {
                tar_filename = "pdf.tar";
            }
            else if (strcmp(filename, ".txt") == 0)
            {
                tar_filename = "text.tar";
            }
            else
            {
                fprintf(stderr, "Unsupported file type: %s\n", filename);
                close(client_socket);
                continue;
            }

            // Receive the tar file from the server
            receive_file(client_socket, tar_filename);
        }
        // Handle the display code
        else if (sscanf(command, "display %s", filename) == 1)
        {
            // Send the command to Smain
            if (send(client_socket, command, strlen(command), 0) < 0)
            {
                perror("Command send error");
                close(client_socket);
                continue;
            }

            // Receive the list of files from the server
            receive_file_list(client_socket);
        }
        else
        {
            fprintf(stderr, "Invalid command syntax.\n");
        }

        // Close the socket after operation
        close(client_socket);
    }

    return EXIT_SUCCESS;
}
