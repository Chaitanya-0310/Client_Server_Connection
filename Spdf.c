#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>


//Defining port no. for spdf server 
#define PORT 3011  
#define BUFSIZE 1024 //Defining Buffer size

void error(const char *msg) {
    perror(msg);
    exit(1);
}
//---------------------------------------------------Handling Upload file-------------------------------------------
// Created a ufile handler for handling ufile command for .txt files
void handle_upload(int newsockfd, char *filename, char *destination) {
    char buffer[BUFSIZE];
    int n;

    // Remove any trailing newline from destination path
    destination[strcspn(destination, "\n")] = 0;
    char fullpath[BUFSIZE]; // array for storing the file path 
       //creating path using snprintf and creating directory stxt for storing .txt files

    snprintf(fullpath, sizeof(fullpath), "%s/%s", destination, filename);

    printf("Uploading File to directory ---> %s\n", fullpath);

    // Ensure the destination directory exists
    struct stat st = {0};
    if (stat(destination, &st) == -1) {
        printf("Directory does not exist, creating: %s\n", destination); 
        // Create directories recursively for ex smain/folder1/folder2
        char tmp[BUFSIZE];
        snprintf(tmp, sizeof(tmp), "%s", destination);
        for (char *p = tmp + 1; *p; p++) {
            if (*p == '/') {
                *p = '\0';
                mkdir(tmp, 0700);
                *p = '/';
            }
        }
        mkdir(destination, 0700);  // Created the final directory where we have to upload the file
    }

    // Open the file in write binary mode 
    FILE *fp = fopen(fullpath, "wb");
    if (fp == NULL) {
        perror("ERROR opening file");
        close(newsockfd);
        return;
    }

    // Receive file data from Smain
    int total_bytes_received = 0;
    while ((n = read(newsockfd, buffer, BUFSIZE)) > 0) {
        fwrite(buffer, sizeof(char), n, fp);
        total_bytes_received += n;
        printf("Received %d bytes of data\n", n);
    }
    fclose(fp);

    printf("File received: %s (%d bytes)\n", fullpath, total_bytes_received);

    // Send acknowledgment back to Smain
    n = write(newsockfd, "File upload successful.\n", 25);
    if (n < 0) error("ERROR writing to socket");

    close(newsockfd);
}

//-------------------------------------------------------------------------------------------------------
//handling dfile command for downloading file
void handle_dfile_Command(int newsockfd, char *filepath) {
    char buffer[BUFSIZE];
    FILE *fp;
    int n;

    printf("Processing dfile command for: %s\n", filepath);

    // Open the file to read bytes mode 
    fp = fopen(filepath, "rb");
    if (fp == NULL) {
        perror("!!! ERROR !!! ---> Got into Error while opening file");
        write(newsockfd, "File not found\n", 16);
        close(newsockfd);
        return;
    }

    printf("Sending file: %s\n", filepath);

    // Sending file data to Smain server 
    while ((n = fread(buffer, sizeof(char), BUFSIZE, fp)) > 0) {
        if (write(newsockfd, buffer, n) < 0) {
            perror("!!! ERROR !!! ---> Got into Error while writing to socket");
            break;
        }
    }

    fclose(fp);
    printf("File transfer completed.\n");
    shutdown(newsockfd, SHUT_WR);
}

void replace_smain_with_spdf(char *filepath) {
    char *pos = strstr(filepath, "smain");
    if (pos != NULL) {
        char temp[BUFSIZE];
        strncpy(temp, filepath, pos - filepath);
        temp[pos - filepath] = '\0';
        strcat(temp, "spdf");
        strcat(temp, pos + strlen("smain"));
        strcpy(filepath, temp);
    }
}

//------------------------------------------------handle_rmfile------------------------------

//Function for handling remove file function 
void handle_rmfile(int newsockfd, char *filepath) {
    replace_smain_with_spdf(filepath);
        //Using remove() for removing the file
    if (remove(filepath) == 0) {
        printf("File %s deleted successfully.\n", filepath);
                //Writing message to socket_FD
        write(newsockfd, "File deleted successfully.\n", 28);
    } else {
        perror("!!! ERROR !!! ---> Got into Error while deleting file");
        write(newsockfd, "File deletion failed.\n", 23);
    }

    close(newsockfd);
}

//------------------------------------------------------------------------

//Handling dtar command 
void handle_dtar_command(int newsockfd, char *filetype) {
    char tarname[BUFSIZE];
    char command[BUFSIZE];
        //Creating tarname according to the naming convention
    snprintf(tarname, sizeof(tarname), "%sfiles.tar", filetype + 1); // Skip the '.' in the filetype
    
    // Confirming tarname does not have any whitespaces or newlines
    tarname[strcspn(tarname, "\n")] = 0;
    tarname[strcspn(tarname, "\r")] = 0; 

    // Construct the command to create the tar file
    snprintf(command, sizeof(command), "tar -cvf %s $(find ~/spdf -type f -name '*%s')", tarname, filetype);
    printf("Executing command: %s\n", command);
    system(command);

    // Send the tar file back to Smain
    FILE *fp = fopen(tarname, "rb");
    if (fp == NULL) {
        perror("!!! ERROR !!! ---> Got into Error while opening tar file");
        write(newsockfd, "Tar file creation failed\n", 26);
        close(newsockfd);
        return;
    }

    // Sending the tar file data back to Smain
    char buffer[BUFSIZE];
    int n;
    while ((n = fread(buffer, sizeof(char), BUFSIZE, fp)) > 0) {
        if (write(newsockfd, buffer, n) < 0) {
            perror("!!! ERROR !!! ---> Got into Error while writing tar file to socket");
            break;
        }
    }

    fclose(fp);
    remove(tarname); // Remove the tar file after sending it to Smain
    printf("Tar file sent Successfully---> %s \U0001F44D \n", tarname);

    shutdown(newsockfd, SHUT_WR);
}

//-------------------------------------Handle Display pathname
void handle_display_request(int newsockfd, char *directory) {
    DIR *d;
    struct dirent *dir;
    char buffer[BUFSIZE] = {0};

    // Handle .txt files by replacing "smain" with "spdf"
    char directory_txt[BUFSIZE];
    strcpy(directory_txt, directory);
    replace_smain_with_spdf(directory_txt);
    printf("%s",directory_txt);

    //handle .pdf files
    d = opendir(directory_txt);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_REG && strcmp(strrchr(dir->d_name, '.'), ".pdf") == 0) {
                strcat(buffer, dir->d_name);
               strcat(buffer, "\n");
            }
        }
        closedir(d);
    } else {
        perror("!!! ERROR !!! ---> Got into Error while opening TXT directory");
    }
    write(newsockfd, buffer, strlen(buffer));
    shutdown(newsockfd, SHUT_WR);
}




// Handle Client function

void handle_client(int newsockfd) {
    char buffer[BUFSIZE];
    int n;

    // Receiving command from Smain
    bzero(buffer, BUFSIZE);
    n = read(newsockfd, buffer, BUFSIZE - 1);
    if (n < 0) error("!!! ERROR !!! ---> Got into Error while reading from socket");

    printf("Command Received From Smain: %s\n", buffer);
// handling for ufile command
    if (strncmp(buffer, "ufile", 5) == 0) {
        // Get filename and destination path
        char *filename = strtok(buffer + 6, " ");
        char *destination = strtok(NULL, " ");
        if (!filename || !destination) {
            printf("Invalid command format.\n");
            close(newsockfd);
            return;
        }

        handle_upload(newsockfd, filename, destination);

    } 
        //Handling for dfile command
    else if (strncmp(buffer, "dfile", 5) == 0) {
        // Extract the file path
        char *filepath = strtok(buffer + 6, " ");
        if (!filepath) {
            write(newsockfd, "Invalid command format\n", 24);
            close(newsockfd);
            return;
        }

        handle_dfile_Command(newsockfd, filepath);

    }
        //Handling for rmfile command 
     else if (strncmp(buffer, "rmfile", 6) == 0) {
        char *filepath = strtok(buffer + 7, " ");
        if (!filepath) {
            write(newsockfd, "Invalid command format\n", 24);
            close(newsockfd);
            return;
        }

        handle_rmfile(newsockfd, filepath);
    } 
        //handling for dtar command
    else if (strncmp(buffer, "dtar", 4) == 0) {
        // Extract the file type and handle the tar creation
        char *filetype = strtok(buffer + 5, " ");
        if (filetype) {
            handle_dtar_command(newsockfd, filetype);
        } else {
            write(newsockfd, "Invalid dtar command format\n", 29);
        }
    } 
        //Handling for display file command
    else if (strncmp(buffer, "display", 7) == 0) {
        // Extract the directory path
        char *directory = strtok(buffer + 8, " ");
        if (!directory) {
            write(newsockfd, "Invalid command format\n", 24);
            close(newsockfd);
            return;
        }

        handle_display_request(newsockfd, directory);

    } 
        //print error if invalid command format received
    else {
        printf("Invalid command format.\n");
        write(newsockfd, "Invalid command format\n", 24);
    }

    close(newsockfd);
}

int main(int argc, char *argv[]) {
    int sockfd, newsockfd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("!!! ERROR !!! ---> Got into Error while opening socket");

    // Bind socket to port
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("!!! ERROR !!! ---> Got into Error on binding");

    listen(sockfd, 5);
    printf("Spdf server Connected on port ---> %d...\n", PORT);

    // Accept connections
    clilen = sizeof(cli_addr);
    while (1) {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) error("!!! ERROR !!! ---> Got into Error while on Accepting Connection Request");

        printf("SPDF server now connected with SMAIN Server \U0001F44D\n");

        handle_client(newsockfd);
    }

    close(sockfd);
    return 0;
}
