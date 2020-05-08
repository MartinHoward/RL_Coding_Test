#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define FT_IP_PORT      9999
#define MAX_BLOCK_SIZE  256


void handleFileTransfer(int sock_fd, char* file_name)
{
    int iBytesRead;
    char acReceiveBlock[MAX_BLOCK_SIZE];
    FILE *pInFile;

    if ((pInFile = fopen(file_name, "wb")) != NULL)
    {
        // Send file name of requested file to server
        send(sock_fd, file_name, strlen(file_name), 0);

        printf("Transfering file: %s\n", file_name);

        while ((iBytesRead = recv(sock_fd, acReceiveBlock, MAX_BLOCK_SIZE, 0)) != 0)
        {
            fwrite(acReceiveBlock, iBytesRead, 1, pInFile);
            putchar('.');
        }

        printf("\nFile transfer complete\n");

        fclose(pInFile);
    }
    else
    {
        printf("Failed to open local file - Aborting\n");
    }

    close(sock_fd);
}

int main(int argc, char *argv[])
{
    int iSockFd;
    char acFileName[MAX_BLOCK_SIZE];
    struct sockaddr_in sSockAddr;

    // Get file to transfer and remote host ip from the command line
    if (argc > 2)
    {
	strcpy(acFileName, argv[1]);

        memset(&sSockAddr, 0, sizeof(sSockAddr));

        if ((sSockAddr.sin_addr.s_addr = inet_addr(argv[2])) == 0)
        {
            printf("Invalid IP address for HOST\n");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        printf("Usage: FTClient [FILE] [HOST]\n");
        printf("Specify file to transfer from server\n");
        exit(EXIT_FAILURE);
    }

    // Create TCP socket and connect
    sSockAddr.sin_family = AF_INET;
    sSockAddr.sin_port = htons(FT_IP_PORT);

    if ((iSockFd = socket(PF_INET, SOCK_STREAM, 0)) == 0)
    {	
        printf("Socket Error: failed to create socket\n");
        exit(EXIT_FAILURE);
    }

    if (connect(iSockFd, (struct sockaddr *) &sSockAddr, sizeof(sSockAddr)) != 0)
    {	
        printf("Socket Error: failed to connect to HOST\n");
        exit(EXIT_FAILURE);
    }

    handleFileTransfer(iSockFd, acFileName);
}
