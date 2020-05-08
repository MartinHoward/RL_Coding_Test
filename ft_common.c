#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <limits.h>

#include "ft_defs.h"

teFtStatus waitForStatus(int sock_fd)
{
    int iRet;
    teFtStatus eStatus;
    
    iRet = recv(sock_fd, &eStatus, sizeof(eStatus), 0);
    
    if (iRet > 0)
        return eStatus;
    else
        return iRet;
}

int sendStatus(int sock_fd, teFtStatus status)
{
    return send(sock_fd, &status, sizeof(status), 0);
}

int transferFileToRemote(int sock_fd, char* file_path)
{
    tsFtMessage sClientMsg;
    FILE *pInFile;
  
    if ((pInFile = fopen(file_path, "rb")) != NULL)
    {
        // Signal client that transfer is about to start
        sendStatus(sock_fd, FT_READY_RECEIVE);

        // Each block will contain the send conintue status until the last block is sent with
        // send complete status
        sClientMsg.eStatus = FT_TRANSFER_CONTINUE;

        while (!feof(pInFile))
        {
            // Read block from local file and send
            sClientMsg.iByteCount = fread(sClientMsg.acDataBlock, 1, MAX_BLOCK_SIZE, pInFile);

            // When end of file is reached we want to tell the client to not expect any more
            if (feof(pInFile))
                sClientMsg.eStatus = FT_TRANSFER_COMPLETE;

            if (send(sock_fd, &sClientMsg, sizeof(sClientMsg), 0) == -1)
                printf("Failed to send to client - errno: %d", errno);
        }

        // Wait for receipt ack from client
        if (waitForStatus(sock_fd) == FT_RECEIVE_ACK)
            printf("Transfer complete: %s\n", file_path);
        else
            printf("Client failed to acknowledge receipt\n");

        fclose(pInFile);
    }
    else
    {
        // Send error status to client
        sendStatus(sock_fd, FT_FILE_NOT_FOUND);
        printf("File not found - transfer aborted\n");
    }
    
    return 0;
}

