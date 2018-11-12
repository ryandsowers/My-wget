// ----------------------------------------------------------------------
// file: mywget.c
// ----------------------------------------------------------------------
// Description: This is a simple version of the wget command, which is
//     is a command-line utility for downloading files from a web server.
//
// Syntax:
//     mywget servername filename
//
// Created: 2017-05-24 (P. Clark)
//
// Modifications:
// 2017-11-20 (P. Clark)
//     Changed interface for getting HTTP header info.
//     Added more file management code than was previously provided.
//     Added when_exiting() and sig_handler().
//     Moved main() to the top of the file.
// 2017-12-07 (R. Sowers)
//     Completed functionality in sections labeled "TBD student code 
//     here..."
// ----------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#define VALID_INPUTS 3
#define DEST_ARG 1
#define FILE_ARG 2
#define SUCCESS 0
#define FILE_EXISTS 0
#define MAXBUF 1024
#define MAXERR 80
#define RESPONSE_OK "200 OK"
#define HEADER_END "\r\n\r\n"

#define ERR_NUM_INPUTS -1     /* invalid number of args at command-line */
#define ERR_DNS -3            /* unable to resolve domain name to IP */
#define ERR_NOT_FOUND -4      /* file not found on the server */
#define ERR_BAD_RESPONSE -5   /* server gave an unrecognized response */
#define ERR_ON_WRITE -6       /* error when writing */
#define ERR_SOCKET -7         /* error getting a socket */
#define ERR_CONNECT -8        /* error trying to connect to server */
#define ERR_FILE -9           /* unable to open local file for output */
#define ERR_FILE_EXISTS -10   /* file exists locally */
#define ERR_BAD_REQUEST -11   /* server complained of bad client request */
#define ERR_UNSUPPORTED -12   /* the returned file is not a text file */
#define ERR_INTERNAL -13      /* unexpected internal problem */
#define ERR_NO_DATA -14       /* no data received from the server */


// Prototypes
void when_exiting(void);
void sig_handler(int signal);
void get_header_info(const char *buffer, 
                     const int count,
                     int *hdr_size,
                     bool *istext);



// Global variables
int Sock_fd = 0;              // socket descriptor
FILE *Myfile = NULL;          // file stream



// ***********************************************************************
// *****************************  M A I N ********************************
// ***********************************************************************
int main(int argc, char *argv[])
{
        int count = 0;
        int hdr_size = 0;
        bool istext = false;
        char request_buf[MAXBUF+1];
        char response_buf[MAXBUF+1];
        struct sigaction act;

        int result = 0;
        struct addrinfo hints;
        struct addrinfo *start = NULL;
        struct addrinfo *current = NULL;

        // Verify proper number of arguments on the command-line
        if (argc != VALID_INPUTS) {
                fprintf(stderr,"Error: invalid number of inputs.\n");
                exit(ERR_NUM_INPUTS);
        }

        // Verify that the requested file doesn't already exist locally
        if (access(argv[FILE_ARG], F_OK) == FILE_EXISTS) {
                fprintf(stderr,
                        "A copy of the requested file exists locally\n");
                exit(ERR_FILE_EXISTS);
        }

        // Register an exit handler
        atexit(when_exiting);

        // Register a handler for segmentation faults
        act.sa_handler = sig_handler;
        act.sa_flags = 0;
        sigemptyset(&act.sa_mask);
        errno = SUCCESS;
        if (sigaction(SIGSEGV, &act, NULL) != 0) {
                perror("Warning: unable to handle segmentation faults");
        }


        // Resolve DNS for the input server name / get a sockaddr
        // TBD student code here...
        // Fill out the 'hints' with the kind of answers we want.
        bzero(&hints,sizeof(hints));
        hints.ai_family   = AF_INET;     // we want IPv4
        hints.ai_socktype = SOCK_STREAM; // we're going to use TCP
        
        result = getaddrinfo(argv[DEST_ARG], "80", &hints, &start);
        if ((result != 0) || (start == NULL)) {
                fprintf(stderr,
                        "DNS resolution failed for %s: %s\n", 
                        argv[DEST_ARG], gai_strerror(result));
                exit(-1);
        }

        // getaddrinfo returns a linked list of DNS results.
        // Go thru the list looking for an IPv4 address.
        // When I find one, I've got a completed sockaddr_in.
        for (current=start; current != NULL; current = current->ai_next) {
                if (current->ai_family == AF_INET) {
                        // I found an IPv4 address.
                        // "current" now points to a completed addrinfo 
                        // type, (see "man getaddrinfo"), which contains a 
                        // completed sockaddr.
                        break;
                } 
        }
        if (current == NULL) {
                fprintf(stderr, "Could not find an IPv4 address");
                exit(-1);
        }
 
#ifdef DEBUG
        char buf[MAXBUF];
        struct sockaddr_in *saddr = NULL;
        saddr = (struct sockaddr_in *) current->ai_addr;
        if (saddr == NULL) {
                fprintf(stderr,"Error: saddr is NULL!!!\n");
                exit(-1);
        } else {
                // print out the IP address 
                fprintf(stderr,
                        "The returned IP address for '%s' is %s\n",
                        argv[DEST_ARG],
                        inet_ntop(AF_INET, &saddr->sin_addr, buf, MAXBUF)
                );
        }
        saddr = NULL;
#endif

        // Try to connect to server
        // ** Put the descriptor into the global "Sock_fd" **
        // TBD student code here...
        errno = SUCCESS;
        Sock_fd = socket(current->ai_family, current->ai_socktype, 0);
        if ((Sock_fd < 0) || (errno != SUCCESS)) {
                perror("Unable to get a descriptor");
                exit(-1);
        }

        errno = SUCCESS;
        result = connect(Sock_fd, current->ai_addr, current->ai_addrlen);
        if (errno != SUCCESS) {
                perror("Unable to connect to server");
        }

        // Build my request in "request_buf"
        // TBD student code here...
        strcat(request_buf, "GET /");
        strcat(request_buf, argv[FILE_ARG]);
        strcat(request_buf, " HTTP/1.1\r\nUser-Agent: mywget/0.1 (linux-gnu)\r\nAccept: text/html\r\nHost: ");
        strcat(request_buf, argv[DEST_ARG]);
        strcat(request_buf, "\r\nConnection: Close\r\n\r\n");

#ifdef DEBUG
        // Pipe the output to verify request: 
        //    './mywget tor.ern.nps.edu index.html | hexdump -c | more'
        printf("request_buf=%s", request_buf);
        fflush(stdout);
#endif


        // Send/write my request to the server
        // TBD student code here...
        count = write(Sock_fd, request_buf, strlen(request_buf));
        errno = SUCCESS;
        if ((count <= 0) || (errno != SUCCESS)) {
                fprintf(stderr, "Failed to send request: '%s'\n", request_buf);
        }

        // Read the response from the server.
        // Read the data into "response_buf", and the # of bytes into "count"
        count = read( Sock_fd, response_buf, MAXBUF );
        if (count <= 0) {
                fprintf(stderr,"No data received from server\n");
                exit(ERR_NO_DATA);
        }
        response_buf[count] = '\0';

        // Analyze the server's response...
        // Is it a valid response?
        // How many bytes is the HTTP header received from the server? 
        // Is this is a text file being returned?
        get_header_info(response_buf, count, &hdr_size, &istext);
        if (hdr_size <= 0) { 
                fprintf(stderr,"Unexpected header size\n");
                exit(ERR_INTERNAL);
        }
        if (!istext) {
                fprintf(stderr,"Error: The file type is not text\n");
                exit(ERR_UNSUPPORTED);
        }

        // I will finally try to create/open the local file.
        // I put this off as long as possible so that I don't leave a
        // created file in place if an error occurred.
        // But if we got this far, there is data to be written out.
        errno = SUCCESS;
        Myfile = fopen(basename(argv[FILE_ARG]), "w+");
        if (errno) {
                perror("Error opening/creating destination file");
                exit(ERR_FILE);
        }

        // Write out the part of the buffer that contains file info (if any).
        errno = SUCCESS;
        fwrite(&(response_buf[hdr_size]), sizeof(char), count-hdr_size, Myfile);
        if (errno) {
                perror("Unexpected error writing to file:");
                exit(ERR_ON_WRITE);
        }

        // Now read the rest of the file from the server (if anything)
        do {
                // Set "count" as the number of bytes read.
                // TBD student code here...
                count = read( Sock_fd, response_buf, MAXBUF );
                fwrite(response_buf, sizeof(char), count, Myfile);
        } while (count > 0);

#if 0   
#endif

        // Clean up
        if (Sock_fd) {
                shutdown(Sock_fd, SHUT_RDWR);
                close(Sock_fd);
        }
        if (Myfile != NULL) {
                fflush(Myfile);
                fclose(Myfile);
                Myfile = NULL;
        }

        // Free up linked list
        if (start != NULL) {
                freeaddrinfo(start);
                start = NULL;
                current = NULL;
        }

        fflush(stderr);
        fflush(stdout);
        return 0;
}



// ----------------------------------------------------------------------
// function
//     when_exiting
// description
//     A function to be called whenever the program is terminating, in
//     order to do some cleanup.
// ----------------------------------------------------------------------
void when_exiting(void)
{
        if (Myfile != NULL) {
                // close the file
                fflush(Myfile);
                fclose(Myfile);
                Myfile = NULL;
        }
        if (Sock_fd != 0) {
                // close the connection
                shutdown(Sock_fd, SHUT_RDWR);
                close(Sock_fd); 
        }
}



// ----------------------------------------------------------------------
// function
//     get_header_info
// description
//     Given an initial response from a web server, this function 
//     verifies that it is a valid response with the start of the
//     requested file, and then returns the number of bytes in the 
//     input that belong to the header (such that the remainder of the
//     input contains file data). It also verifies that the input
//     data is from a text file. If an error condition is detected, or
//     the buffer does not contain the expected format, then this
//     function will not return to the caller because it will terminate
//     the program.
// inputs
//     buffer
//         A buffer with the inital response from a web server
//     count
//         The number of bytes in the buffer that were returned by the 
//         server.
// outputs
//     hdr_size
//         The number of bytes in the input buffer that belong to 
//         the HTTP header.
//     istext
//         A boolean value indicating whether the data from the requested 
//         file is text-based (true) or not (false).
// ----------------------------------------------------------------------
void get_header_info(const char *buffer, 
                     const int count,
                     int *hdr_size,
                     bool *istext)
{
        char *copy = NULL;
        char *header_end = NULL;

        // Verify "good" input
        if ((buffer == NULL) || (count <= 0)) {
                fprintf(stderr, "Bad HTTP header info.\n");
                exit(ERR_BAD_RESPONSE);
        } else if ((hdr_size == NULL) || (istext == NULL)) {
                fprintf(stderr, "Bad output pointer(s).\n");
                exit(ERR_INTERNAL);
        }

#ifdef DEBUG
        // print out the buffer to see what the server provided
        fprintf(stderr, "count=%d\n", count);
        fprintf(stderr,"\n-------------\n");
        fprintf(stderr, "%s", buffer);
        fprintf(stderr,"\n-------------\n");
#endif
        // Make a copy of the buffer
        copy = strdup(buffer);
        if (copy == NULL) {
                fprintf(stderr, "Unexpected strdup error\n");
                exit(ERR_INTERNAL);
        }
        
        // Look for the end of the header 
        header_end = strstr(copy, HEADER_END);
        if (header_end == NULL) {
                free(copy);
                fprintf(stderr, "Invalid HTTP header\n");
                exit(ERR_BAD_RESPONSE);
        }
        // mark the end of header as the end of the string
        *header_end = '\0';

        // Look for some known codes
        if (strstr(copy, "404 Not Found")!=NULL) {
                free(copy);
                fprintf(stderr, "File not found on server\n");
                exit(ERR_NOT_FOUND);
        } else if (strstr(copy, "400 Bad Request")!=NULL) {
                free(copy);
                fprintf(stderr, "Server said 'bad request'\n");
                exit(ERR_BAD_REQUEST);
        } else if (strstr(copy, "200 OK")!=NULL) {
                // File found
                *hdr_size = strlen(copy) + strlen(HEADER_END);
                if (strstr(copy, "Content-Type: text")!=NULL) {
                        *istext = true;
                } else {
                        *istext = false;
                }
        } else {
                free(copy);
                fprintf(stderr, "Unsupported header code\n");
                exit(ERR_BAD_RESPONSE);
        }

        if (copy != NULL) {
                free(copy);
        }
}



// ----------------------------------------------------------------------
// function
//     sig_handler
// description
//     Handles segmentation fault cleanup
// ----------------------------------------------------------------------
void sig_handler(int signal)
{
        if (signal != SIGSEGV) {
                fprintf(stderr, "Unexpected call to sig_handler\n");
                exit(ERR_INTERNAL);
        }
        fprintf(stderr, 
                "\nA segmentation fault has been detected.\nExiting...\n");
        exit(ERR_INTERNAL);
}

// End of myget.c
