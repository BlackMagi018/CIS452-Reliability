#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>

#define MAXBUF 1024
#define MAXSIZE 1028

int main(int argc, char **argv) {
    //Created socket file descriptor
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        printf("There was an error creating the socket\n");
        return 1;
    }

    //User prompted for input. Port Number to use, IP Address to send to
    printf("Enter Port Number: ");
    char port[20];
    fgets(port, 20, stdin);
    /*printf("Enter IP Address: ");
    char IP[15];
    fgets(IP, 15, stdin);*/

    //setup UDP connection
    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(atoi(port));
    serveraddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    //connect to server
    int e = connect(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr));
    if (e < 0) {
        printf("There was an error connecting\n");
        return 1;
    }

    //User  prompted for input. File name to retrieve
    socklen_t len = sizeof(serveraddr);
    //printf("What file would you like to retrieve. ");

    //filename holder
    char line1[150];
    //Data buffer
    char buff[MAXSIZE];

    //fgets(line1, 151, stdin);
    strcpy(line1,"TextFile.txt\n");

    //file size
    long fsize = 0;
    //received data size
    int rsize = 0;
    //sent method return value
    int sent = 0;
    //receive method return value
    int receive = 0;
    //resend timeout
    int timeout = 0;
    //acknowledgment
    long receipt = 154;

    //send file name
    while (sent <= 0) {
        sent = (int) sendto(sockfd, line1, 151, 0,
                            (struct sockaddr *) &serveraddr, len);
        if (sent == -1) {
            fprintf(stderr, "Value of error:  %d\n", errno);
            printf("Error sending:\n");
        } else {
            fprintf(stdout, "File name sent to server\n");
        }
    }

    //receive acknowledgment of receipt
    while (receive <= 0) {
        receive = (int) recvfrom(sockfd, &fsize, sizeof(long), 0,
                                 (struct sockaddr *) &serveraddr, &len);
        if (receive == -1) {
            fprintf(stderr, "Value of error:  %d\n", errno);
            printf("Error receiving:\n");
        } else if (receive == 0) {
            printf("Server has requested and orderly shutdown\n");
            return 1;
        } else {
            printf("File Name Acknowledgement received\n");
        }
        timeout++;
        if(timeout == 3){
            sent = (int) sendto(sockfd, line1, 151, 0,
                                (struct sockaddr *) &serveraddr, len);
        }
        sleep(2);
    }

    sent = 0;
    receive = 0;
    char size[sizeof(long)];


    //receive file size
    while (receive <= 0) {
        receive = (int) recvfrom(sockfd, size, sizeof(long), 0,
                                 (struct sockaddr *) &serveraddr, &len);
        if (receive == -1) {
            fprintf(stderr, "Value of error:  %d\n", errno);
            printf("Error receiving file size:\n");
        } else if (receive == 0) {
            printf("Waiting for filename");
        } else {
            fsize = strtol(size,NULL,10);
            printf("File size received - %ld\n", fsize);
        }
    }

    //send acknowledgment of receipt
    while (sent <= 0) {
        sent = (int) sendto(sockfd, &receipt, sizeof(long), 0,
                            (struct sockaddr *) &serveraddr, len);
        if (sent == -1) {
            fprintf(stderr, "Value of error:  %d\n", errno);
            printf("Error sending acknowledgment:\n");
        } else {
            printf("File Size Acknowledgement sent");
        }

    }


    int transferring = 1;
    FILE *rfp = fopen("TransFile.txt", "wb");
    if (rfp == NULL) {
        printf("File not opened. Transfer canceled\n");
        return -1;
    }

    int MAXPAKS = (int) (fsize/MAXBUF)+1;
    //char **data_array = malloc((MAXPAKS * sizeof(char *)));
   // for (int i = 0; i < MAXPAKS; i++) {
   //     data_array[i] = (char *) malloc(MAXBUF);
    //}

    while (transferring) {
        while (fsize > rsize) {
            sleep(1);
            receive = (int) recvfrom(sockfd, buff, MAXSIZE, 0,
                                     (struct sockaddr *) &serveraddr, &len);
            if (receive > 0) {
                char code[2];
                memcpy(code,buff,1);
                int ack = atoi(code);
                printf("\nReceived packet %s | Size %i\n",ack, receive);
                char * text = &buff[2];
                //printf("%s\n\n",text);
                rsize += sizeof(buff)-4;
                printf("%i\n", rsize);
                fwrite(text, 1, strlen(text), rfp);
                sent = (int) sendto(sockfd, code, sizeof(code) + 1, 0,
                                    (struct sockaddr *) &serveraddr, sizeof(serveraddr));
                memset(buff, 0, MAXSIZE);
            } else {
                printf("%i", receive);
            }
        }
        transferring = 0;

        fclose(rfp);
    }

    close(sockfd);
    return 0;
}