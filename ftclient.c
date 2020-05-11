#include <stdio.h>
#include <stdlib.h>

#include "ft_defs.h"

#define TMP_FINAL_FILE      "tmpFinalFile"
#define TMP_HASH_FILE_NAME  "tmpHashFile"
#define TMP_RSYNC_FILE_NAME "tmpRsyncFile"

// ***********************************************************************************************************
// Function: generateRsyncHashFile
//
// Generates a hash file to send to the file server when an Rsync is requested. The file contains a simple
// hash along with an MD5 hash for each block in the original file
//
// Inputs: orig_file_name    pointer to string containing the name of the original file to Rsync
//         hash_file_name    pointer to string containing the name of the hash file to send to the server
//
// Returns: 0 on success, -1 on error
// ***********************************************************************************************************        
int generateRsyncHashFile(char *orig_file_name, char *hash_file_name)
{
    int rc = 0;
    FILE *pInFile, *pTmpFile;
    tsFtFileBlock sFileBlock;
    tsFtFileBlockHash sBlockHash;

    if (((pInFile = fopen(orig_file_name, "rb")) != NULL) &&
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

// ***********************************************************************************************************
// Function: processRsyncFile
//
// Processes the Rsync file sent from the server. This function updates the original file based on the
// Rsync instructions contained in the file
//
// Inputs: orig_file_name    pointer to string containing the name of the original file to Rsync
//         rsync_file_name   pointer to string containing the name of the Rsync file to use
//
// Returns: 0 on success, -1 on error
// ***********************************************************************************************************        
int processRsyncFile(char *orig_file_name, char *rsync_file_name)
{
    int iBlock, rc = 0;
    char cDataType;
    BYTE bNewByte;
    FILE *pInFile, *pTmpRsyncFile, *pTmpFinalFile;
    char acFileBlock[MAX_BLOCK_SIZE];
    
    if ((((pInFile = fopen(orig_file_name, "rb")) != NULL) &&
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
                    printf("Rsync file parse failure\n");
                    rc = -1;
                    break;
                }
            }
        }
        
        fclose(pInFile);
        fclose(pTmpRsyncFile);
        fclose(pTmpFinalFile);
            
        // Remove temp rsync file
        remove(rsync_file_name);
            
        if (rc == 0)
        {
            // Replace old file with temp file
            remove(orig_file_name);
            rename(TMP_FINAL_FILE, orig_file_name);
        }
    }
    else 
    {
        rc = -1;
    }
    
    return rc;
}

// ***********************************************************************************************************
// Function: handleFileRsync
//
// Handles the Rsync operation for a file with the server. This function generates and sends the hash file
// to the server then receives the Rsync file and contstructs the final file using the instructions in
// the Rsync file received from the server
//
// Inputs:  sock_fd           Socket file descriptor to the server
//          orig_file_name    pointer to string containing the name of the original file to Rsync
//
// Returns: None
// ***********************************************************************************************************        
void handleFileRsync(int sock_fd, char *orig_file_name)
{
    sendStatus(sock_fd, FT_FILE_RSYNC);

    // Send file name to server
    send(sock_fd, orig_file_name, strlen(orig_file_name), 0);

    // Generate hashes for the local file and send to server for Rsync operation
    if (generateRsyncHashFile(orig_file_name, TMP_HASH_FILE_NAME) == 0)
    {
        // Send hash file to server
        transferFileToRemote(sock_fd, TMP_HASH_FILE_NAME);
        
        // Get status response from server before starting
        if (waitForStatus(sock_fd) == FT_READY_SEND)
        {
            // Get the resulting rsync file from the server
            if (receiveFileFromRemote(sock_fd, TMP_RSYNC_FILE_NAME) == 0)
            {
                // Process rsync file
                if (processRsyncFile(orig_file_name, TMP_RSYNC_FILE_NAME) == 0)
                {
                    printf("Rsync of file: %s complete\n", orig_file_name);
                }
                else 
                {
                    printf("Rsync of file: %s failure\n", orig_file_name);
                }
            }
            else 
            {
                printf("Server failed to send file - Aborting\n");
            }
            
            // Remove temp hash file
            remove(TMP_HASH_FILE_NAME);
        }
        else
        {
            printf("Failed to get status from server - Aborting\n");
        }
    }
    else 
    {
        printf("Failed to generate Rsync hash file - Aborting\n");
    }
}

// ***********************************************************************************************************
// Function: handleFileDownload
//
// Handles the downloading of a file from the server. Will overwrite the file in the working directory if
// it exists. No Rsync operation is attempted.
//
// Inputs:  sock_fd           Socket file descriptor to the server
//          orig_file_name    pointer to string containing the name of the original file to Rsync
//
// Returns: None
// ***********************************************************************************************************        
void handleFileDownload(int sock_fd, char *file_name)
{
    sendStatus(sock_fd, FT_FILE_NEW);

    // Send file name to server
    send(sock_fd, file_name, strlen(file_name), 0);

    // Get status response from server before starting
    if (waitForStatus(sock_fd) == FT_READY_SEND)
    {
        // Transfer complete file from server - existing file will be overwritten if it exists
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
    if (argc == 3)
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

    // Check if file exists locally. If it does then we want to update the exising file using Rsync
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
