#include <stdio.h>
#include <stdlib.h>

#include "md5.h"
#include "ft_defs.h"

#define HASH_BASE 256
#define PRIME_MOD 1000000007

unsigned long computeHashForBlock(BYTE *data_block, int block_size)
{
    int i;
    unsigned long hash = 0;
    
    for (i = 0; i < block_size; i++)
    {
        hash += data_block[i];
    }
    
    hash %= block_size;
    
    return hash;
}

unsigned long computeRollingHash(unsigned long hash, BYTE first_byte, BYTE new_byte, int block_size)
{
    return (hash + new_byte - first_byte) % (block_size + 1);
}

unsigned char *computeMd5HashForBlock(BYTE *data_block, int block_size, unsigned char* hash)
{
    MD5_CTX ctx;
        
    MD5Init(&ctx);
    
    MD5Update(&ctx, data_block, block_size);
    
    MD5Final(hash, &ctx);
    
    return hash;
}

// ***********************************************************************************************************
// Function: sendStatus
//
// Sends a status code to the other end of the socket connecton
//
// Inputs: sock_fd    Socket file descriptor
//         status     Status code of type teFtStatus
//
// Returns: Number of bytes send or -1 for error
// ***********************************************************************************************************        
int sendStatus(int sock_fd, teFtStatus status)
{
    return send(sock_fd, &status, sizeof(status), 0);
}

// ***********************************************************************************************************
// Function: waitForStatus
//
// Waits on a socket for a status code sent from the remote end
//
// Inputs: sock_fd    Socket file descriptor
//
// Returns: Status code or -1 on error
// ***********************************************************************************************************        
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

// ***********************************************************************************************************
// Function: transferFileToRemote
//
// Sends a specified file to the remote end of the socket. Will send an error status to remote end if file
// is not found
//
// Inputs: sock_fd    Socket file descriptor
//         file_name  Pointer to string containing local file to send
//
// Returns: None
// ***********************************************************************************************************        
void transferFileToRemote(int sock_fd, char* file_name)
{
    tsFtFileBlock sFileBlock;
    FILE *pInFile;
  
    if ((pInFile = fopen(file_name, "rb")) != NULL)
    {
        // Signal client that transfer is about to start
        sendStatus(sock_fd, FT_READY_SEND);

        // Wait for acknowledge that client is ready
        if (waitForStatus(sock_fd) == FT_READY_RECEIVE)
        {
            // Each block will contain the send conintue status until the last block is sent with
            // send complete status
            sFileBlock.eStatus = FT_TRANSFER_CONTINUE;

            while (!feof(pInFile))
            {
                // Read block from local file and send
                sFileBlock.iByteCount = fread(sFileBlock.acDataBlock, 1, MAX_BLOCK_SIZE, pInFile);

                // When end of file is reached we want to tell the client to not expect any more
                if (feof(pInFile) || sFileBlock.iByteCount < MAX_BLOCK_SIZE)
                {
                    sFileBlock.eStatus = FT_TRANSFER_COMPLETE;
                }
                
                if (send(sock_fd, &sFileBlock, sizeof(sFileBlock), 0) == -1)
                    printf("Failed to send to client - errno: %d", errno);
                    
                // Slow things down a bit - Raspberry pi seems to get overwhelmed - fine on Mac OS
                usleep(50000);
            }

            // Wait for receipt ack from client
            if (waitForStatus(sock_fd) == FT_RECEIVE_ACK)
                printf("Transfer complete: %s\n", file_name);
            else
                printf("Client failed to acknowledge receipt\n");

            fclose(pInFile);
        }
        else 
        {
            printf("Client failed to acknowledge ready to receive");
        }
    }
    else
    {
        // Send error status to client
        sendStatus(sock_fd, FT_FILE_NOT_FOUND);
        printf("File not found - transfer aborted\n");
    }
    
    return;
}

// ***********************************************************************************************************
// Function: receiveFileFromRemote
//
// Receives a specified file from a server at the remote end of the socket. The file will be stored in the
// current working directory if the transfer succeeds. 
//
// Inputs: sock_fd    Socket file descriptor
//         file_name  Pointer to string containing remote file to retreive
//
// Returns: None
// ***********************************************************************************************************        
void receiveFileFromRemote(int sock_fd, char* file_name)
{
    unsigned long ulBytesTransferred = 0; 
    tsFtFileBlock sFileBlock;
    FILE *pInFile;
    
    if ((pInFile = fopen(file_name, "wb")) != NULL)
    {
        sendStatus(sock_fd, FT_READY_RECEIVE);
        printf("Transfering file: %s\n", file_name);

        while (1)
        {
            if (recv(sock_fd, &sFileBlock, sizeof(sFileBlock), 0) == -1)
            {
                printf("Fatal server error - aborting\n");
                break;
            }

            ulBytesTransferred += sFileBlock.iByteCount;

            printf("**** Recevied: %ld bytes, status: %d\n", ulBytesTransferred, sFileBlock.eStatus);
            
            if ((sFileBlock.eStatus == FT_TRANSFER_CONTINUE) ||
                (sFileBlock.eStatus == FT_TRANSFER_COMPLETE))
            {
                fwrite(sFileBlock.acDataBlock, sFileBlock.iByteCount, 1, pInFile);

                if ((sFileBlock.eStatus == FT_TRANSFER_COMPLETE) ||
                    (sFileBlock.iByteCount < MAX_BLOCK_SIZE))
                {
                    // Acknowledge file received to server
                    sendStatus(sock_fd, FT_RECEIVE_ACK);
                        
                    printf("\nFile transfer complete - bytes: %ld\n", ulBytesTransferred);
                    break;
                }
            }
            else
            {
                // Something unexpected happened - remove local file
                //remove(file_name);
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

    return;
}

