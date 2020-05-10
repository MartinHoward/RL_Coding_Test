#include <stdio.h>
#include <stdlib.h>

#include "ft_defs.h"

char acFileDirectory[PATH_MAX];

int searchForHash(BYTE *data_block, char *hash_file, unsigned long hash)
{
    int rc = -1, block = 0;
    FILE *pHashFile;
    tsFtFileBlockHash sBlockHash;
    unsigned char acMd5Hash[MD5_HASH_SIZE];
    
    if ((pHashFile = fopen(hash_file, "rb")) != NULL)
    {
        while (!feof(pHashFile))
        {
            fread(&sBlockHash, 1, sizeof(tsFtFileBlockHash), pHashFile);
            
            if (hash == sBlockHash.ulHash)
            {
                // Now get the MD5 hash of the source block
                computeMd5HashForBlock(data_block, MAX_BLOCK_SIZE, acMd5Hash);
                
                // Check for match
                if (memcmp(acMd5Hash, sBlockHash.acMd5Hash, sizeof(acMd5Hash)) == 0)
                {
                    rc = block;
                    break;
                }
            }
            
            block++;
        }
        
        fclose(pHashFile);
    }
    
    return rc;
}

void generateRsyncFile(int sock_fd, char *source_file, char *hash_file)
{
    int block;
    char asRsyncFileName[NAME_MAX];
    BYTE acDataBlock[MAX_BLOCK_SIZE];
    FILE *pSrcFile, *pRsyncFile;
    long lFilePos = 0;
    tsRsyncRecord sRsyncRecord;
    unsigned long ulHash;
    
    // Start by creating the temp rsync file
    sprintf(asRsyncFileName, "tmpRsyncFile-%d", (int) pthread_self());
    
    // Open all files
    if (((pSrcFile = fopen(source_file, "rb")) != NULL) &&    
        ((pRsyncFile = fopen(asRsyncFileName, "wb")) != NULL))
    {
        while (!feof(pSrcFile))
        {
            // Read block
            fread(acDataBlock, 1, sizeof(acDataBlock), pSrcFile);

            // Compute hash for block
            ulHash = computeHashForBlock(acDataBlock, sizeof(acDataBlock));
            
            printf("Computed hash %ld - lFilePos %ld\n", ulHash, lFilePos);
    
            // Search for matching hash in hash file
            if ((block = searchForHash(acDataBlock, hash_file, ulHash)) != -1)
            {
                // A matching block has been found, add info to rsync file
                sRsyncRecord.bReuseBlock = TRUE;
                sRsyncRecord.ulBlockIdOrChar = block;
            
                fwrite(&sRsyncRecord, sizeof(sRsyncRecord), 1, pRsyncFile);
                
                // Advance input file pointer by one block
                lFilePos += sizeof(acDataBlock);
                //fseek(pSrcFile, lFilePos, SEEK_CUR);
            }    
            else 
            {
                // Send the first byte in the block
                sRsyncRecord.bReuseBlock = FALSE;
                sRsyncRecord.ulBlockIdOrChar = (BYTE) acDataBlock[0];
            
                fwrite(&sRsyncRecord, sizeof(sRsyncRecord), 1, pRsyncFile);
                
                // Advance input file pointer by one byte
                lFilePos += sizeof(acDataBlock);
                rewind(pSrcFile);
                fseek(pSrcFile, lFilePos, SEEK_SET);
            }
        }
        
        fclose(pSrcFile);
        fclose(pRsyncFile);
    }
    
    // Send the rsync file to the client
    transferFileToRemote(sock_fd, asRsyncFileName);
    
    // Remove tmpfile
    remove(asRsyncFileName);
          
    return;
}

void handleFileUpdateRequest(int sock_fd, char *file_name)
{
    char acHashFileName[NAME_MAX];
    
    // Get status response from remote end before starting
    if (waitForStatus(sock_fd) == FT_READY_SEND)
    {
        // Generate a local temp file to store the hashes
        sprintf(acHashFileName, "tmpHashFile-%d", (int) pthread_self());
    
        // Get the hash file from the client
        receiveFileFromRemote(sock_fd, acHashFileName);
    
        // TODO: Process hash file and send response back
        generateRsyncFile(sock_fd, file_name, acHashFileName);
        
        // TODO: Delete temp file
        remove(acHashFileName);
    }
    else
    {
        printf("Fatal client error - Aborting\n");
    }
}

void * handleClientRequestThread(void *argv)
{
    int iSock = *((int *)argv);
    char acFileName[NAME_MAX];
    char acFilePath[PATH_MAX+NAME_MAX];
    BOOL bFileUpdate = FALSE;

    memset(acFileName, 0, sizeof(acFileName));
    memset(acFilePath, 0, sizeof(acFilePath));
    
    // Wait for client to indicate download type
    if (waitForStatus(iSock) == FT_FILE_UPDATE)
    {
        bFileUpdate = TRUE;
        printf("File update requested\n");
    }
    
    // Receive file name to transfer from client
    recv(iSock, acFileName, NAME_MAX, 0);

    // Get full path name
    sprintf(acFilePath, "%s/%s", acFileDirectory, acFileName);

    printf("File requested: %s\n", acFilePath);

    if (bFileUpdate)
    {
        handleFileUpdateRequest(iSock, acFilePath);
    }
    else 
    {
        transferFileToRemote(iSock, acFilePath);
    }
    
    close(iSock);

    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    int iSockFd, iClientSockFd;
    struct sockaddr_in sSockAddr;
    struct sockaddr_storage sSockStore;
    socklen_t sSockStoreSize = sizeof(sSockStore);
    pthread_t tid;

    if (argc == 1)
    {
        getcwd(acFileDirectory, sizeof(acFileDirectory));
    }
    else if (argc == 2)
    {
        strcpy(acFileDirectory, argv[1]);
    }
    else
    {
        printf("Usage: ftserver [DIR]\n");
        printf("Specify directory containing files to serve, defauft is current working directory\n");
        exit(EXIT_FAILURE);
    }

    printf("Working directory: %s\n", acFileDirectory);

    // Create TCP socket and bind
    memset(&sSockAddr, 0, sizeof(sSockAddr));
    sSockAddr.sin_family = AF_INET;
    sSockAddr.sin_port = htons(FT_IP_PORT);
    //sSockAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if ((iSockFd = socket(PF_INET, SOCK_STREAM, 0)) == 0)
    {	
        printf("Socket Error: failed to create socket\n");
        exit(EXIT_FAILURE);
    }

    if (bind(iSockFd, (struct sockaddr *) &sSockAddr, sizeof(sSockAddr)) < 0)
    {	
        printf("Socket Error: failed to bind socket\n");
        exit(EXIT_FAILURE);
    }

    // Listen on socket for client connections
    if (listen(iSockFd, MAX_CLIENTS) != 0)
    {
        printf("Socket Error: listen failed\n");
        exit(EXIT_FAILURE);
    }

    while(1)
    {
        iClientSockFd = accept(iSockFd, (struct sockaddr *) &sSockStore, &sSockStoreSize);

        if (pthread_create(&tid, NULL, handleClientRequestThread, &iClientSockFd) != 0)
        {
            printf("Socket Error: listen failed\n");
            exit(EXIT_FAILURE);
        }
    }
}
