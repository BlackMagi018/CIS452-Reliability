#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <unistd.h>

#define MAXBUF 1024
#define MAXSIZE 1028

int main(int argc, char **argv) {

    // Connection Status
    int connection = 1;
    //Slide window is implemented as a pointer to an array of char arrays that hold 1024 bytes
    char **slide = malloc(5 * sizeof(char *));
    for (int i = 0; i < 5; i++) {
        slide[i] = (char *) malloc(MAXSIZE * sizeof(char));
    }

    //Create socket file descriptor
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        printf("There was an error creating the socket\n");
        return 1;
    }

    //Have user enter the port number to use
    printf("Enter Port Number to listen on: ");
    char port[10];
    fgets(port, 10, stdin);

    //setup UDP connection
    struct sockaddr_in serveraddr, clientaddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons((uint16_t) strtol(port, NULL, 10));
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    //Bind socket with connection
    bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr));

    //Process keeps running until the connection is closed
    while (connection) {

        //length of size of clientaddr object
        socklen_t len = sizeof(clientaddr);
        //holder for filename
        char file[150];
        //new line character
        char nl[2] = "\n";
        //sent method return value
        int sent = 0;
        //receive method return value
        int receive = 0;
        //resend timeout
        int timeout = 0;
        //acknowledgment sent
        long receipt = 154;
        //acknowledgement received
        int ack;
        //Maximum # of Packets
        int MaxPaks = 0;
        // File Size
        long fsize = 0;

        //File pointer for file to be transferred
        FILE *fp = NULL;

        //receive file name
        while (receive <= 0) {
            receive = (int) recvfrom(sockfd, file, 151, 0,
                                     (struct sockaddr *) &serveraddr, &len);
            if (receive == -1) {
                fprintf(stderr, "Value of errno:  %d\n", errno);
                printf("Error sending:\n");
            } else if (receive == 0) {
                printf("Waiting for filename\n");
            } else {
                printf("Connection established, Filename received - %s\n", file);
                //Remove newline operator from the end of the string
                char *filename = strtok(file, nl);

                //File pointer for file to be transferred
                fp = fopen(filename, "rb");

                //Check to see if file opened properly. If not the connection is terminated
                if (fp == NULL) {
                    printf("File not found\n");
                    return -1;
                }

                //Get file size
                struct stat st;
                stat(file, &st);
                fsize = (int) st.st_size;
            }
        }

        //send acknowledgment of receipt
        while (sent <= 0) {
            sent = (int) sendto(sockfd, &receipt, sizeof(long), 0,
                                (struct sockaddr *) &serveraddr, len);
            if (sent == -1) {
                fprintf(stdout, "Error sending:\n");
                fprintf(stderr, "Value of error:  %d\n", errno);
            } else {
                fprintf(stdout, "File Name Acknowledgement sent\n");
            }
            sleep(2);
        }

        sent = 0;
        receive = 0;
        int MAX_PACKETS = (int) ((fsize / MAXBUF) + 1);

        //prepare to send file size
        char size[sizeof(long)];
        sprintf(size, "%ld", fsize);
        printf("File Size Sent: %s\n", size);


        //send file size
        while (sent <= 0) {
            sent = (int) sendto(sockfd, (const void *) size, sizeof(size), 0,
                                (struct sockaddr *) &serveraddr, len);
            if (sent == -1) {
                fprintf(stderr, "Value of error:  %d\n", errno);
                fprintf(stderr, "Error sending file size:\n");
            } else {
                printf("File size sent to client\n");
            }
        }

        //receive acknowledgment of receipt
        while (receive <= 0) {
            receive = (int) recvfrom(sockfd, &ack, sizeof(int), 0,
                                     (struct sockaddr *) &serveraddr, &len);
            if (receive == -1) {
                fprintf(stderr, "Value of errno:  %d\n", errno);
                fprintf(stderr, "Error receiving acknowledgment:\n");
            } else if (receive == 0) {
                printf("Client has requested an orderly shutdown\n");
                return 1;
            } else {
                printf("File Size Acknowledgement received\n");
            }
            timeout++;
            if(timeout == 3){
                sent = (int) sendto(sockfd, (const void *) size, sizeof(size), 0,
                                    (struct sockaddr *) &serveraddr, len);
            }
        }

        //Storage will be used as the transfer buffer on the file contents
        char * storage;
        storage = (char *) malloc(MAXBUF * sizeof(char));

        //Load the first 5120 bytes into the sliding window
        for (int i = 0; i < 5; i++) {
            fread(storage, sizeof(char), MAXBUF, fp);
            sprintf(slide[i], "%02i%s", i, storage);
        }
        free(storage);

        //Sequence variable used to keep track of sliding window
        int seq = 0;

        //Send the first set of data
        for (int i = 0; i < 5; i++) {
            sent = (int) sendto(sockfd, slide[i], MAXSIZE, 0,
                                (struct sockaddr *) &serveraddr, len);
            if (sent > 0) {
                fprintf(stdout, "Sent packet %i | Size %i\n", seq, sent);
            } else {
                fprintf(stderr, "Packet failed to send\n");
                fprintf(stderr, "Value of error:  %d\n", errno);
                sent = (int) sendto(sockfd, slide[i], MAXSIZE, 0,
                                    (struct sockaddr *) &serveraddr, len);
                if (sent < 1) {
                    printf("Second Packet failed to send\n");
                }
            }
            sleep(2);
            seq++;
        }

        //temp variable
        char * code[2];
        while (!feof(fp)) {
            receive = (int) recvfrom(sockfd, code, sizeof(code), 0,
                                     (struct sockaddr *) &clientaddr, &len);
            if (receive > 0) {
                ack = (int) strtol(code, NULL, 10);
                memset(storage,0,MAXBUF);
                int total = (int) fread(storage, 1, MAXBUF, fp);
                printf("total %i\n", total);
                memset(slide[ack % 5],0,MAXSIZE);
                sprintf(slide[(ack % 5)], "%02i%s",(ack+5), storage);
                printf("%s\n",slide[(ack % 5)]);
                sent = (int) sendto(sockfd, slide[(ack % 5)], sizeof(total + 1),0,
                                    (struct sockaddr *) &clientaddr, len);
                if(sent > 0){
                    fprintf(stdout,"Sent a packet %i | Size %i\n",(ack+5),sent);
                } else{
                    printf("Packet failed to send");
                }
            } else {
                printf("Failure to receive");
            }
            sleep(1);
        }

        printf("Reached end of file");
        connection = 0;
        close(sockfd);
    }
    return 0;
}