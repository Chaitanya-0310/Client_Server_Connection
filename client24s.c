#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <limits.h> 
#include <dirent.h>

//Declared global variable for Smain Port to connect the client with smain server using PORT_SMAIN
#define PORT_SMAIN 3112
#define BUF_SIZE 1024  //Buffer Size for storing command

void error(const char *msg) {
    perror(msg);
    exit(1);
}

//Created a function to remove all the unwanted whitespaces present in the string
//So that the terminal can get proper input and can get the command properly for comparison
char *trimming_whitespaces(char *string) {
    char *end;
    //trimming down the leading space if any found in the string
    while(isspace((unsigned char)*string)) string++;
    if(*string == 0) // If there are no characters or its an empty string which means all spaces present
        return string;
    end = string + strlen(string) - 1;
    while(end > string && isspace((unsigned char)*end)) end--;
    // Write new null terminator character
    end[1] = '\0';
    return string;
}

//Created a function for connecting the client to server
int connect_Client_with_Server() 
{
    int socket_FD;
    struct sockaddr_in serv_addr;

    // Now here we are Creating socket Connection
    printf("-----Setting Up Socket Connection-----\n");
    socket_FD = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_FD < 0) error("!!! ERROR !!! ---> Got into Error while Opening File");

    // Here I am setting the server address//
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT_SMAIN);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        error("!!! ERROR !!! --->  Provided IP address is not Valid");
    }
    // Connecting to the server 
    printf("Connection Established to Server at ---> 127.0.0.1 at Port No. ---> %d\n", PORT_SMAIN);
    if (connect(socket_FD, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
    {
        error("!!! ERROR !!! ---> Got into Error While Connecting to Server");
    }
    printf("\U0001F44D Hurray, Client is now connected to server.\U0001f600 \n");
    return socket_FD;
}

//-------------------------------------------------------------------ufile Handler-----------------------------------------------
//Creating Handle Function for ufile command for uplaoding file 
//It takes 3 parameters socket file Descriptor, filename and destination path where file needs to be uploaded
void ufile_Handler(int socket_FD, char *filename, char *destination) 
{
    char buffer[BUF_SIZE];
    FILE *fp;
    int n;
    //here using snprintf generating a command for passing the command to the server from client
    //Storing the created command in buffer
    snprintf(buffer, sizeof(buffer), "ufile %s %s", filename, destination);
    //Writing the buffer to socket file descriptor
    n = write(socket_FD, buffer, strlen(buffer));
    if (n < 0) error("!!! ERROR !!! ---> Got into Error while Writing to Server Socket");

    //Opening the file to read the data present in the file that needs to be upload using fopen function
    // Here we are opening the file in read binary mode
    fp = fopen(filename, "rb");
    if (fp == NULL) 
    {
        perror("!!! ERROR !!! ---> Got into Error Whille Opening File");
        return;
    }

//  Keeping track of how many bytes of the file sent to the server.
    int total_bytes_sent = 0;
    while ((n = fread(buffer, sizeof(char), BUF_SIZE, fp)) > 0) 
    {
        printf("Read total %d bytes of the file.\n", n);  // Debug print
        //Storing total bytes to total bytes written
        int total_bytes_written = write(socket_FD, buffer, n);
        //If the total_bytes_written is less than 0 than it throws error
        if (total_bytes_written < 0) {
            error("!!! ERROR !!! ---> Got into Error While Writing data of the file to socket");
        }
        total_bytes_sent += total_bytes_written;
        printf("Read total %d bytes to of the file, total sending %d bytes to the server.\n", total_bytes_written, total_bytes_sent);  // Debug print
    }
    fclose(fp);
    printf("Completed Uploading File. Total %d bytes forwared to server. \n *** Closing socket write end ***\n", total_bytes_sent);
    // Shutdown the writing side of the socket to signal the end of data transmission
    shutdown(socket_FD, SHUT_WR);

    // Wait for server acknowledgment
    bzero(buffer, BUF_SIZE);
    printf("Client Waiting For Server Response\n");
    n = read(socket_FD, buffer, BUF_SIZE - 1);
    if (n < 0) error("ERROR reading from socket");
    printf("Response From The Server --->  %s\n", buffer);
}

//-----------------------------------------------------------------Handle File Download-----------------------------------
void dfile_Handler(int socket_FD, char *filepath) 
{
    //Array for sotoring buffer
    char buffer[BUF_SIZE];
    FILE *fp;
    int n;

     //here using snprintf generating a command for passing the command to the server from client
    //Storing the created command in buffer
    snprintf(buffer, sizeof(buffer), "dfile %s", filepath);
    n = write(socket_FD, buffer, strlen(buffer));
    if (n < 0) error("!!! ERROR !!! ---> Got into Error while Writing to Server Socket");

    // Here we are extracting the filename from the filepath eg  "/home/pancha9c/smain/folder1/sample.c" ---> filepath  
    char *filename = strrchr(filepath, '/');
    if (filename == NULL) {
        filename = filepath;  // No directory specified, so the filepath is just the filename
    } else {
        filename++;  // Move past the last '/'
    }
    // Remove any trailing newline found in filename
    filename[strcspn(filename, "\n")] = 0;

    // Opening the file in wb mode write binary mode
    fp = fopen(filename, "wb");
    if (fp == NULL) {
        perror("ERROR opening file");
        return;
    }
    //providing the name of the file that is getting download
    printf("Downloading %s file\n", filename);
 // Here we are reading the data from file from the server and writing to the file
    while ((n = read(socket_FD, buffer, BUF_SIZE)) > 0) {
        // Check if the server sent an error message
        //Storing error of server if any in buffer
        if (strncmp(buffer, "ERROR", 5) == 0) {
            printf("Server caught some error: %s\n", buffer);
            fclose(fp);
            remove(filename); // removing the incomplete file
            return;
        }
        fwrite(buffer, sizeof(char), n, fp);
    }
    if (n < 0) {
        perror("!!! ERROR !!! ---> Got into Error while Reading from server");
    } else {
        printf("File download completed: %s \U0001F44D \n ", filename);
    }
    fclose(fp);    
}


//------------------------------------------handling File Removal---------------------------------------------------------
//Created a function for handling file removal that rmfile command

void handle_file_removal(int socket_FD, char *filepath) 
{
    // Array for storing Buffer
    char buffer[BUF_SIZE];
    int n;
    //here using snprintf generating a command for passing the command to the server from client
    //Storing the created command in buffer
    snprintf(buffer, sizeof(buffer), "rmfile %s", filepath);
    //Writing buffer to the socket file descriptor
    n = write(socket_FD, buffer, strlen(buffer));
    if (n < 0) error("!!! ERROR !!! ---> Got into Error while Writing to Server Socket");
    // Waiting for acknowledgment from the server
    bzero(buffer, BUF_SIZE);
    printf("----- Waiting for the Response From Server -----.\n");
    //reading buffer content sent by the server using socket file descriptor
    n = read(socket_FD, buffer, BUF_SIZE - 1);
    if (n < 0) error("!!! ERROR !!! ---> Got into Error while reading from socket");
    //Printing Server Response 
    printf("Response From The Server --->  %s\n", buffer);
}


//--------------------------------------------------------------Handling TAR DOWNLAOD---------------------------------
//Created a handler for dtar command which creates a dtar file for given extension format provided by the client
void dtar_Handler(int socket_FD, char *filetype) 
{
    // Array for storing buffer 
    char buffer[BUF_SIZE];
    //Array for storing cwd that is client's current working directory so that created tar file gets stored at cwd
    char cwd[BUF_SIZE];
    int n;

    // Get current working directory using getcwd() function
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("!!! ERROR !!! ---> Got into Error while Getting current working directory of the client");
        return;
    }

// Trimming down any whitespaces if found in the filetype that while providing .c, .txt or .pdf
    filetype = trimming_whitespaces(filetype);

    //here using snprintf generating a command for passing the command to the server from client
    //Storing the created command in buffer
    snprintf(buffer, sizeof(buffer), "dtar %s", filetype);
    char *trimmed_command = trimming_whitespaces(buffer);
    //Writing buffer to the socket file descriptor
    n = write(socket_FD, trimmed_command, strlen(trimmed_command));
    if (n < 0) error("!!! ERROR !!! ---> Got into Error while Writing to server socket");
     // Storing the filepath of the tar file from the server
    char filepath_of_TarFile[BUF_SIZE];
    //Creating a filepath for the tar file where to store it and in which format
    snprintf(filepath_of_TarFile, sizeof(filepath_of_TarFile), "%s/%sfiles.tar", cwd, filetype + 1);
    printf("Tar File getting downloaded in the Directory ---> %s \U0001F600\n", filepath_of_TarFile);
    
    //Here we are opening the filepath in write binary mode
    FILE *fp = fopen(filepath_of_TarFile, "wb");
    if (fp == NULL) {
        perror("ERROR opening file");
        return;
    }
    // Read the tar file data from the server
    while ((n = read(socket_FD, buffer, BUF_SIZE)) > 0) {
        if (n < 0) {
            perror("!!! ERROR !!! ---> Got into Error while reading from socket");
            break;
        }
        fwrite(buffer, sizeof(char), n, fp);
    }
    if (n == 0) {
        printf("Completed downloading the Tar file in : %s directory \U0001F44D \n ", filepath_of_TarFile);
    } else if (n < 0) {
        perror("!!! ERROR !!! ---> Got into Error while reading from socket");
    }
    //closing file
    fclose(fp);
    // Sendinf acknowledge to the server that the download is complete
    snprintf(buffer, sizeof(buffer), "Download completed for %sfiles.tar\n", filetype + 1);
    write(socket_FD, buffer, strlen(buffer));
    printf("*** Acknowledgment sent to server ***\n");
}

//------------------------------------------------------------ Display Pathname Handler------------------------------------------
//Created a handler for display pathname command for handling displaying of files in the path
void handle_display_pathname(int socket_FD, char *directory) {
    char buffer[BUF_SIZE];
    int n;

    // Send the display command to the server
    snprintf(buffer, sizeof(buffer), "display %s", directory);
    n = write(socket_FD, buffer, strlen(buffer));
    if (n < 0) error("!!! ERROR !!! ---> Got into Error while writing to server socket");

    // Read the file names list from the server
    printf("Files present in the Directory are listed below:\n");
    //Reading the buffer sent from the server through socket file descriptor
    while ((n = read(socket_FD, buffer, BUF_SIZE - 1)) > 0) {
        buffer[n] = '\0';  
        printf("%s", buffer); //Printing files present in the buffer
    }
    if (n < 0) {
        error("ERROR reading from socket");
    }

    shutdown(socket_FD, SHUT_WR);
}




// Main function for client24s.c file 
int main(int argc, char *argv[]) {
    int socket_FD;
    char buffer[BUF_SIZE];

    printf("Here you can perform total 5 commands\n");
        printf("1. ufile --> For uploading files to the server \nSyntax --> ufile filename destination-Path \n\n");
        printf("2. dfile --> For Downloading Files from the server\nSyntax --> dfile filepath\n\n");
        printf("3. rmfile --> Remove file from the serve\nSyntax --> rmfile filepath\n\n");
        printf("4. dtar --> Create a tar file for the files present in directory\nSyntax --> dtar filetype\n\n");
        printf("5. display --> Display all the files present in the directory\nSyntax --> display directory\n\n");
        printf("Use ** /home/username/ ** format for any directory\n\n");
        printf("Lets get Started \U0001F600 \n");

    while (1) {
        socket_FD = connect_Client_with_Server();  // Reconnect for each command

        printf("Enter the command syntax: ");
        bzero(buffer, BUF_SIZE);
        fgets(buffer, BUF_SIZE - 1, stdin);

        //Handling specially for exit command 
        if (strncmp(buffer, "exit", 4) == 0) {  
            printf("Client Exited.\n");
            //closing the socket connection with the server
            close(socket_FD);
            break;
        }
        // handling file upload
        if (strncmp(buffer, "ufile", 5) == 0) {
            char *filename = strtok(buffer + 6, " ");
            char *destination = strtok(NULL, " ");
            //if filename or destination doesnot match than throw syntax error to the client
            if (!filename || !destination) {
                printf("!!! Invalid ufile command syntax !!! Valide Syntaz --> ufile filename destination_path\n");
                close(socket_FD);
                continue;
            }
            // passing 3 arguments to ufile_handler function 
            ufile_Handler(socket_FD, filename, destination);
        }

        // Handle file download command
        else if (strncmp(buffer, "dfile", 5) == 0) {
            char *filepath = strtok(buffer + 6, " ");
            //If filepath is not existed than 
            if (!filepath) {
                printf("!!! Invalid dfile command syntax !!! Valid Syntax --> dfile filepath\n");
                close(socket_FD);
                continue;
            }

            dfile_Handler(socket_FD, filepath);
        }
         // Handle file removal command
        //Comparing string for rmfile if found than 
        else if (strncmp(buffer, "rmfile", 6) == 0) {
            char *filepath = strtok(buffer + 7, " ");
            //If filepath is not existed than 
            if (!filepath) {
                printf("!!! Invalid rmfile command syntax !!!. Valide Syntax --> rmfile filepath\n");
                close(socket_FD);
                continue;
            }

            handle_file_removal(socket_FD, filepath);
        }
         // Handle tar download command
         //Comparing string with dtar string if found than proceed further
        else if (strncmp(buffer, "dtar", 4) == 0) {
            char *filetype = strtok(buffer + 5, " ");
            //If filepath is not existed than 
            if (!filetype) {
                printf("!!! Invalid dtar command syntax !!! Valid Syntax --> dtar filetype\n");
                close(socket_FD);
                continue;
            }

            dtar_Handler(socket_FD, filetype);
        }
        //Comparing string using strncmp with display string, if found than proceed further
         else if (strncmp(buffer, "display", 7) == 0) {
            char *directory = strtok(buffer + 8, " ");
            // If Directory does not exist
            if (!directory) {
                printf("!!! Invalid display command syntax !!! Valid Syntax --> display directory\n");
                close(socket_FD);
                continue;
            }

            handle_display_pathname(socket_FD, directory);
        }

// Close connection after handling the command
        close(socket_FD);  
    }

    return 0;
}
