#include <stdio.h>
#include <stdlib.h>

#include "ft_defs.h"


int main(int argc, char *argv[])
{
    int iSockFd;
    char acFileName[MAX_BLOCK_SIZE];
    BOOL bFileUpdate = FALSE;
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

    // Send file name to server
    send(iSockFd, acFileName, strlen(acFileName), 0);

    // Check if file exists locally. If it does then we want to update the exising file
    if( access( acFileName, F_OK ) != -1 )
        bFileUpdate = TRUE;
    else
        bFileUpdate = FALSE;
    
    // Get status response from server before starting
    if (waitForStatus(iSockFd) == FT_READY_SEND)
    {
        receiveFileFromRemote(iSockFd, acFileName, bFileUpdate);
    }
    else
    {
        printf("File not found on server - Aborting\n");
    } 
    
    close(iSockFd);
}
