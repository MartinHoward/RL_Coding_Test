#include <stdio.h>
#include <stdlib.h>

#include "ft_defs.h"

#define TMP_HASH_FILE_NAME "tmpHashFile"

void generateHashFile(char *file_name)
{
    FILE *pInFile, *pTmpFile;
    tsFtFileBlock sFileBlock;
    tsFtFileBlockHash sBlockHash;

    if (((pInFile = fopen(file_name, "rb")) != NULL) &&
         (pTmpFile = fopen(TMP_HASH_FILE_NAME, "wb")) != NULL)
    {
        while (!feof(pInFile))
        {
            fread(sFileBlock.acDataBlock, 1, MAX_BLOCK_SIZE, pInFile);
            
            // Compute the hashes and store to temp file
            sBlockHash.ulHash = computeHashForBlock(sFileBlock.acDataBlock, MAX_BLOCK_SIZE);
            computeMd5HashForBlock(sFileBlock.acDataBlock, MAX_BLOCK_SIZE, sBlockHash.acMd5Hash);
            
            fwrite(&sBlockHash, sizeof(sBlockHash), 1, pTmpFile);
        }
        
        fclose(pInFile);
        fclose(pTmpFile);
    }
    return;
}

void handleFileUpdate(int sock_fd, char *file_name)
{
    sendStatus(sock_fd, FT_FILE_UPDATE);

    // Send file name to server
    send(sock_fd, file_name, strlen(file_name), 0);

    // Generate hashes for the local file and send to server
    generateHashFile(file_name);

    // Send hash file to server
    transferFileToRemote(sock_fd, TMP_HASH_FILE_NAME);
    
    //receiveFileFromRemote(iSockFd, acFileName);
}

void handleFileDownload(int sock_fd, char *file_name)
{
    sendStatus(sock_fd, FT_FILE_NEW);

    // Send file name to server
    send(sock_fd, file_name, strlen(file_name), 0);

    // Get status response from server before starting
    if (waitForStatus(sock_fd) == FT_READY_SEND)
    {
        // File does not exist locally - transfer complete file
        receiveFileFromRemote(sock_fd, file_name);
    }
    else
    {
        printf("File not found on server - Aborting\n");
    }
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

    // Check if file exists locally. If it does then we want to update the exising file
    if( access( acFileName, F_OK ) != -1 )
    {
        handleFileUpdate(iSockFd, acFileName);
    }
    else
    {
        handleFileDownload(iSockFd, acFileName);
    }
    
    close(iSockFd);
}
