#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ft_defs.h"


void handleFileTransfer(int sock_fd, char* file_name)
{
    unsigned long ulBytesTransferred = 0; 
    tsFtMessage sServerMsg;
    teFtStatus eStatus;
    FILE *pInFile;

    // Send file name to server
    send(sock_fd, file_name, strlen(file_name), 0);

    // Get status response from server
    recv(sock_fd, &eStatus, sizeof(eStatus), 0);

    if (eStatus == FT_READY_RECEIVE)
    {
        if ((pInFile = fopen(file_name, "wb")) != NULL)
        {
            printf("Transfering file: %s\n", file_name);

            while (1)
            {
                recv(sock_fd, &sServerMsg, sizeof(sServerMsg), 0);

                ulBytesTransferred += sServerMsg.iByteCount;

                if ((sServerMsg.eStatus == FT_TRANSFER_CONTINUE) ||
                    (sServerMsg.eStatus == FT_TRANSFER_COMPLETE))
                {
                    fwrite(sServerMsg.acDataBlock, sServerMsg.iByteCount, 1, pInFile);

                    if (sServerMsg.eStatus == FT_TRANSFER_COMPLETE)
                    {
                        // Acknowledge file received to server
                        eStatus = FT_RECEIVE_ACK;
                        send(sock_fd, &eStatus, sizeof(eStatus), 0);
                        
                        printf("\nFile transfer complete - bytes: %ld\n", ulBytesTransferred);
                        break;
                    }
                }
                else
                {
                    // Something unexpected happened - remove local file
                    remove(file_name);
                    printf("Fatal server error - aborting\n");
                    break;
                }
            }

            fclose(pInFile);
        }
        else
        {
            printf("Failed to open local file - Aborting\n");
        }
    }
    else
    {
        printf("File not found on server - Aborting\n");
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
        printf("Usage: ftclient [FILE] [HOST]\n");
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
