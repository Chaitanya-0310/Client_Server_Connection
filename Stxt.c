#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

//Defining port no. for stxt server 
#define PORT_STXT 3012  
#define BUF_SIZE 1024  //Defining Buffer size

void error(const char *msg) {
    perror(msg);
    exit(1);
}

//Created a function for replacing smain with stext
//Here in this function we are replacing the smain string from the filepath with stext for creating the filepath for 
// for stext directory 

void replace_smain_with_stext(char *filepath) 
{
    // Find the position of the substring "smain" in the filepath string
    char *position = strstr(filepath, "smain");
    // If "smain" is found in the filepath
    if (position != NULL) {
        // Declare a temporary buffer to store the modified filepath
        char temp_storage[BUF_SIZE];
        // Copy the part of the filepath before "smain" into the temp_storage buffer
        strncpy(temp_storage, filepath, position - filepath);
        // Null-terminate the temporary buffer at the end of the copied portion
        temp_storage[position - filepath] = '\0';
        // Concatenate "stext" to the temporary buffer after the copied portion
        strcat(temp_storage, "stext");
        // Concatenate the rest of the filepath after "smain" to the temporary buffer
        strcat(temp_storage, position + strlen("smain"));
        // Copy the modified filepath from the temporary buffer back to the original filepath
        strcpy(filepath, temp_storage);
    }
}

//---------------------------------------------------Handling Upload file-------------------------------------------
// Created a ufile handler for handling ufile command for .txt files
void ufile_Handler(int socket_FD, char *filename, char *destination) {
    char buffer[BUF_SIZE];
    int n;
    // Remove any trailing newline from destination path
    destination[strcspn(destination, "\n")] = 0;
    char file_fullpath[BUF_SIZE]; // array for storing the file path 
    //creating path using snprintf and creating directory stxt for storing .txt files
    snprintf(file_fullpath, sizeof(file_fullpath), "%s/%s", destination, filename);
    printf("Uploading File to directory ---> %s\n", file_fullpath);

    // Ensure the destination directory exists
    struct stat st = {0};
    if (stat(destination, &st) == -1) {
        printf("Directory doesn't Exist, Making new directory --->  %s\n", destination);
        // Create directories recursively for ex smain/folder1/folder2
        char tmp_dir[BUF_SIZE];
        snprintf(tmp_dir, sizeof(tmp_dir), "%s", destination);
        for (char *i = tmp_dir + 1; *i; i++) {
            if (*i == '/') {
                *i = '\0';
                mkdir(tmp_dir, 0700);
                *i = '/';
            }
        }
        mkdir(destination, 0700);  // Created the final directory where we have to upload the file
    }

    // Open the file in write binary mode 
    FILE *fp = fopen(file_fullpath, "wb");
    if (fp == NULL) {
        perror("ERROR opening file");
        close(socket_FD);
        return;
    }

    // Receive file data from Smain
    int total_bytes_received = 0;
    while ((n = read(socket_FD, buffer, BUF_SIZE)) > 0) {
        fwrite(buffer, sizeof(char), n, fp);
        total_bytes_received += n;
        printf("Received %d bytes of data\n", n); 
    }
    fclose(fp);

    printf("File received: %s (%d bytes)\n", file_fullpath, total_bytes_received);

    // Sending acknowledgment back to Smain 
    n = write(socket_FD, "File upload successful.\n", 25);
    if (n < 0) error("ERROR writing to socket");

    close(socket_FD);
}


//-------------------------------------------------------------------------------------------------------
//handling dfile command for downloading file
void handle_dfile_Command(int socket_FD, char *filepath) {
    char buffer[BUF_SIZE];
    FILE *fp;
    int n;

    printf("Processing dfile command for: %s\n", filepath);

    // Open the file to read bytes mode
    fp = fopen(filepath, "rb");
    if (fp == NULL) {
        perror("!!! ERROR !!! ---> Got into Error while opening file");
        write(socket_FD, "File not found\n", 16);
        close(socket_FD);
        return;
    }

    printf("Sending file: %s\n", filepath);

    // Sending file data to Smain server 
    while ((n = fread(buffer, sizeof(char), BUF_SIZE, fp)) > 0) {
        if (write(socket_FD, buffer, n) < 0) {
            perror("!!! ERROR !!! ---> Got into Error while writing to socket");
            break;
        }
    }
    fclose(fp);
    printf("File transfering to SMain Server completed.\n");
    shutdown(socket_FD, SHUT_WR);

}
//------------------------------------------------handle_rmfile------------------------------

//Function for handling remove file function 
void handle_rmfile(int socket_FD, char *filepath) {
    replace_smain_with_stext(filepath);
    //Using remove() for removing the file
    if (remove(filepath) == 0) {
        printf("File %s deleted successfully.\n", filepath);
        //Writing message to socket_FD
        write(socket_FD, "File deleted successfully.\n", 28);
    } else {
        perror("!!! ERROR !!! ---> Got into Error while deleting file");
        write(socket_FD, "File deletion failed.\n", 23);
    }

    close(socket_FD);
}

//------------------------------------------------------------------------

//Handling dtar command 
void handle_dtar_command(int socket_FD, char *filetype) {
    char tarname[BUF_SIZE];
    char command[BUF_SIZE];
    //Creating tarname according to the naming convention
    snprintf(tarname, sizeof(tarname), "%sfiles.tar", filetype + 1); // +1 for Skipping the '.' in the filetype
    
    // Confirming tarname does not have any whitespaces or newlines
    tarname[strcspn(tarname, "\n")] = 0;
    tarname[strcspn(tarname, "\r")] = 0; 

    // Construct the command to create the tar file
    snprintf(command, sizeof(command), "tar -cvf %s $(find ~/spdf -type f -name '*%s')", tarname, filetype);
    printf("Executing command: %s\n", command);
    system(command);

    // Sending tar file back to Smain
    FILE *fp = fopen(tarname, "rb");
    if (fp == NULL) {
        perror("ERROR opening tar file");
        write(socket_FD, "Tar file creation failed\n", 26);
        close(socket_FD);
        return;
    }

    // Sending the tar file data back to Smain
    char buffer[BUF_SIZE];
    int n;
    while ((n = fread(buffer, sizeof(char), BUF_SIZE, fp)) > 0) {
        if (write(socket_FD, buffer, n) < 0) {
            perror("!!! ERROR !!! ---> Got into Error while writing tar file to socket");
            break;
        }
    }

    fclose(fp);
    printf("Tar file sent Successfully---> %s \U0001F44D \n", tarname);

    shutdown(socket_FD, SHUT_WR);
}


//-------------------------------------Handle Display pathname--------------------------------

void handle_display_request(int socket_FD, char *directory) {
    DIR *d;
    struct dirent *dir;
    char buffer[BUF_SIZE] = {0};

    printf("Initial Directory format: %s",directory);
    // Handle .txt files by replacing "smain" with "stext"
    char directory_txt[BUF_SIZE];
    strcpy(directory_txt, directory);
    replace_smain_with_stext(directory_txt);

    printf("After replacing with smain: %s",directory_txt);

    //handle .txt files
    d = opendir(directory_txt);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_REG && strcmp(strrchr(dir->d_name, '.'), ".txt") == 0) {
               strcat(buffer, dir->d_name);
               strcat(buffer, "\n");

            }
        }
        closedir(d);
    } else {
        perror("!!! ERROR !!! ---> Got into Error while opening TXT directory");
    }
    write(socket_FD, buffer, strlen(buffer));
    shutdown(socket_FD, SHUT_WR);
}


// Handle Client function

void handle_client(int socket_FD) {
    char buffer[BUF_SIZE];
    int n;

    // Receiving command from Smain 
    bzero(buffer, BUF_SIZE);
    n = read(socket_FD, buffer, BUF_SIZE - 1);
    if (n < 0) error("!!! ERROR !!! ---> Got into Error while reading from socket");

    printf("Command Received From Smain: %s\n", buffer);

// handling for ufile command
    if (strncmp(buffer, "ufile", 5) == 0) {
        // Get filename and destination path
        char *filename = strtok(buffer + 6, " ");
        char *destination = strtok(NULL, " ");
        if (!filename || !destination) {
            printf("Invalid command format.\n");
            close(socket_FD);
            return;
        }

        ufile_Handler(socket_FD, filename, destination);

    } 
    //Handling for dfile command
    else if (strncmp(buffer, "dfile", 5) == 0) {
        // Extract the file path
        char *filepath = strtok(buffer + 6, " ");
        if (!filepath) {
            write(socket_FD, "Invalid command format\n", 24);
            close(socket_FD);
            return;
        }

        handle_dfile_Command(socket_FD, filepath);

    }
    //Handling for rmfile command 
    else if (strncmp(buffer, "rmfile", 6) == 0) {
        char *filepath = strtok(buffer + 7, " ");
        if (!filepath) {
            write(socket_FD, "Invalid command format\n", 24);
            close(socket_FD);
            return;
        }

        handle_rmfile(socket_FD, filepath);
    } 
    //handling for dtar command
    else if (strncmp(buffer, "dtar", 4) == 0) {
        // Extract the file type
        char *filetype = strtok(buffer + 5, " ");
        if (!filetype) {
            write(socket_FD, "Invalid command format\n", 24);
            close(socket_FD);
            return;
        }

        handle_dtar_command(socket_FD, filetype);

    } 
    //Handling for display file command
    else if (strncmp(buffer, "display", 7) == 0) {
        // Extract the directory path
        printf("pathname: %s",buffer);
        char *directory = strtok(buffer + 8, " ");
        if (!directory) {
            write(socket_FD, "Invalid command format\n", 24);
            close(socket_FD);
            return;
        }

        printf("%s",directory);

        handle_display_request(socket_FD, directory);

    } 
    //print error if invalid command format received
    else {
        printf("Invalid command format.\n");
        write(socket_FD, "Invalid command format\n", 24);
    }

    close(socket_FD);
}

int main(int argc, char *argv[]) 
{
    int sockfd, socket_FD;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;

    // Create socket connection
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("!!! ERROR !!! ---> Got into Error while opening socket");

    // Bind socket to port
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT_STXT);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("!!! ERROR !!! ---> Got into Error while binding");

    listen(sockfd, 5);
    printf("STXT Server Connected on Port --> %d\n", PORT_STXT);

    // Accept connections
    clilen = sizeof(cli_addr);
    while (1) {
        socket_FD = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (socket_FD < 0) error("!!! ERROR !!! ---> Got into Error while Accepting Connection");

        printf("STXT server now connected with SMAIN Server \U0001F44D \n");

        handle_client(socket_FD);
    }

    close(sockfd);
    return 0;
}
