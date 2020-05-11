#include <stdio.h>
#include <stdlib.h>

#include "ft_defs.h"

#define TMP_FINAL_FILE      "tmpFinalFile"
#define TMP_HASH_FILE_NAME  "tmpHashFile"
#define TMP_RSYNC_FILE_NAME "tmpRsyncFile"

int generateHashFile(char *file_name, char *hash_file_name)
{
    int rc = 0;
    FILE *pInFile, *pTmpFile;
    tsFtFileBlock sFileBlock;
    tsFtFileBlockHash sBlockHash;

    if (((pInFile = fopen(file_name, "rb")) != NULL) &&
         (pTmpFile = fopen(hash_file_name, "wb")) != NULL)
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
    else 
    {
        rc = -1;
    }
    
    return rc;
}

int processRsyncFile(char *file_name, char *rsync_file_name)
{
    int iBlock, rc = 0;
    char cDataType;
    BYTE bNewByte;
    FILE *pInFile, *pTmpRsyncFile, *pTmpFinalFile;
    char acFileBlock[MAX_BLOCK_SIZE];
    
    if ((  ((pInFile = fopen(file_name, "rb")) != NULL) &&
           (pTmpRsyncFile = fopen(rsync_file_name, "rb")) != NULL) &&
           (pTmpFinalFile = fopen(TMP_FINAL_FILE, "wb")) != NULL)
    {
        while (!feof(pTmpRsyncFile))
        {
            // Read data type character
            cDataType = getc(pTmpRsyncFile);
            
            if (cDataType == RSYNC_DATATYPE_BYTE)
            {
                // Read the character and write to final file
                bNewByte = getc(pTmpRsyncFile);
                
                fputc(bNewByte, pTmpFinalFile);
            }
            else if (cDataType == RSYNC_DATATYPE_BLOCK)
            {
                // Read the corresponding block and write to final file
                fread(&iBlock, sizeof(iBlock), 1, pTmpRsyncFile);
                
                fseek(pInFile, iBlock * MAX_BLOCK_SIZE, SEEK_SET);
                fread(acFileBlock, sizeof(acFileBlock), 1, pInFile);
                
                fwrite(acFileBlock, sizeof(acFileBlock), 1, pTmpFinalFile);
            }
            else {
                if (!feof(pTmpRsyncFile))
                {
                    printf("Rsync file parse error\n");
                    rc = -1;
                    break;
                }
            }
        }
        
        if (rc == 0)
        {
            fclose(pInFile);
            fclose(pTmpRsyncFile);
            fclose(pTmpFinalFile);
            
            // Remove temp rsync file
            remove(rsync_file_name);
            
            // Replace old file with temp file
            remove(file_name);
            rename(TMP_FINAL_FILE, file_name);
        
            printf("Rsync of file: %s complete\n", file_name);
        }
    }
    else 
    {
        rc = -1;
        printf("Failed to process Rsync file\n");
    }
    
    return rc;
}

void handleFileRsync(int sock_fd, char *file_name)
{
    sendStatus(sock_fd, FT_FILE_RSYNC);

    // Send file name to server
    send(sock_fd, file_name, strlen(file_name), 0);

    // Generate hashes for the local file and send to server
    generateHashFile(file_name, TMP_HASH_FILE_NAME);

    // Send hash file to server
    transferFileToRemote(sock_fd, TMP_HASH_FILE_NAME);
    
    // Get status response from server before starting
    if (waitForStatus(sock_fd) == FT_READY_SEND)
    {
        // Get the resulting rsync file from the server
        receiveFileFromRemote(sock_fd, TMP_RSYNC_FILE_NAME);
        
        // Process rsync file
        processRsyncFile(file_name, TMP_RSYNC_FILE_NAME);
        
        // Remove temp hash file
        remove(TMP_HASH_FILE_NAME);
    }
    else
    {
        printf("Server failed to send file - Aborting\n");
    }
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
        handleFileRsync(iSockFd, acFileName);
    }
    else
    {
        handleFileDownload(iSockFd, acFileName);
    }
    
    close(iSockFd);
}
