# Distributed File System (Using Socket Programming)

## Project Overview
This project implements a **Distributed File System** using socket programming, designed to handle file operations across multiple servers. The system is composed of three servers—**Smain**, **Spdf**, and **Stext**—and multiple clients. 

- **Smain** acts as the main server, managing all client requests.
- **Spdf** is responsible for handling **.pdf** files.
- **Stext** is responsible for handling **.txt** files.

### Key Features:
- **Multi-Client Support**: The system can manage multiple clients simultaneously, each communicating exclusively with the Smain server.
- **File Distribution**: Clients upload **.c**, **.pdf**, and **.txt** files to Smain. Smain stores **.c** files locally and forwards **.pdf** and **.txt** files to Spdf and Stext servers, respectively.
- **Command Processing**: Clients can upload, download, delete files, create tar archives, and list files using specific commands.

---

## Project Structure
The project consists of the following core components:

- **Smain.c**: Manages client connections and distributes files to the appropriate servers.
- **Spdf.c**: Dedicated to storing and managing **.pdf** files.
- **Stext.c**: Dedicated to storing and managing **.txt** files.
- **client24s.c**: The client-side program that interacts with the Smain server to perform various file operations.

---

## Commands
The client can use the following commands to interact with the distributed file system:

1. **ufile `filename` `destination_path`**
   - **Description**: Uploads a file from the client’s working directory to the specified path on the Smain server.
   - **Parameters**:
     - `filename`: The file to upload (**.c**, **.pdf**, or **.txt**).
     - `destination_path`: Destination path on Smain (e.g., `~/smain/folder1`).
   - **Example**:
     ```
     ufile sample.c ~/smain/folder1/folder2
     ```

2. **dfile `filename`**
   - **Description**: Downloads a file from Smain to the client’s working directory.
   - **Parameters**:
     - `filename`: The file to download (e.g., **.c**, **.pdf**, or **.txt**).
   - **Example**:
     ```
     dfile ~/smain/folder1/folder2/sample.pdf
     ```

3. **rmfile `filename`**
   - **Description**: Deletes a file from the Smain server.
   - **Parameters**:
     - `filename`: Path of the file to delete.
   - **Example**:
     ```
     rmfile ~/smain/folder1/folder2/sample.txt
     ```

4. **dtar `filetype`**
   - **Description**: Creates a tar archive of all files of the specified type and downloads the tar file to the client’s working directory.
   - **Parameters**:
     - `filetype`: File type to archive (e.g., **.c**, **.pdf**, or **.txt**).
   - **Example**:
     ```
     dtar .pdf
     ```

5. **display `pathname`**
   - **Description**: Displays a list of all **.c**, **.pdf**, and **.txt** files in the specified directory on the Smain server.
   - **Parameters**:
     - `pathname`: Path of the directory on Smain (e.g., `~/smain`).
   - **Example**:
     ```
     display ~/smain/folder1/folder2
     ```

---

## Setup and Execution

### Prerequisites
- Linux-based operating system
- GCC compiler

### Steps to Run:

1. **Compile the server and client programs:**
   ```bash
   gcc -o smain Smain.c
   gcc -o spdf Spdf.c
   gcc -o stext Stext.c
   gcc -o client client24s.c

2. **Run the servers on separate terminals:**
   ```bash
   ./smain
   ./spdf
   ./stext
3. **Run the client:***
   ```bash
   ./client
