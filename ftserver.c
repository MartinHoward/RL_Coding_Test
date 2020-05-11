#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "ft_defs.h"

char acFileDirectory[PATH_MAX];

int searchForMatchingBlock(BYTE *data_block, char *hash_file, unsigned long hash)
{
    int rc = -1, iBlock = 0;
    FILE *pHashFile;
    unsigned char acMd5Hash[MD5_HASH_SIZE];
    tsFtFileBlockHash sBlockHash;
    
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
                    rc = iBlock;
                    break;
                }
            }
            
            iBlock++;
        }
        
        fclose(pHashFile);
    }
    
    return rc;
}

void generateRsyncFile(int sock_fd, char *source_file, char *hash_file)
{
    int iBlock;
    char asRsyncFileName[NAME_MAX];
    BYTE acDataBlock[MAX_BLOCK_SIZE];
    FILE *pSrcFile, *pRsyncFile;
    long lFilePos = 0, lSrcFileSize;
    struct stat st;
    unsigned long ulHash;
    
    // Start by creating the temp rsync file
    sprintf(asRsyncFileName, "tmpRsyncFile-%d", (int) pthread_self());
    
    // Get the size of the source file
    stat(source_file, &st);
    lSrcFileSize = st.st_size;
       
    // Process hash file and generate rsync file
    if (((pSrcFile = fopen(source_file, "rb+")) != NULL) &&
         (pRsyncFile = fopen(asRsyncFileName, "wb")) != NULL)
    {
        //while (!feof(pSrcFile))
        while(lFilePos < lSrcFileSize)
        {
            // Read block
            fseek(pSrcFile, lFilePos, SEEK_SET);
            fread(acDataBlock, 1, sizeof(acDataBlock), pSrcFile);

            // Compute hash for block
            ulHash = computeHashForBlock(acDataBlock, sizeof(acDataBlock));
            
            // Search for matching hash in hash file
            if ((iBlock = searchForMatchingBlock(acDataBlock, hash_file, ulHash)) != -1)
            {
                // Insert block reference in the rsync file
                fputc(RSYNC_DATATYPE_BLOCK, pRsyncFile);
                fwrite(&iBlock, sizeof(iBlock), 1, pRsyncFile);
                
                // Advance input file pointer by one block
                lFilePos += sizeof(acDataBlock);
            }    
            else 
            {
                // Insert character reference in the rsync file
                fputc(RSYNC_DATATYPE_BYTE, pRsyncFile);
                fputc(acDataBlock[0], pRsyncFile);

                // Advance input file pointer by one byte
                lFilePos += 1;
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

void handleFileRsyncRequest(int sock_fd, char *file_name)
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
    BOOL bFileRsync = FALSE;

    memset(acFileName, 0, sizeof(acFileName));
    memset(acFilePath, 0, sizeof(acFilePath));
    
    // Wait for client to indicate download type
    if (waitForStatus(iSock) == FT_FILE_RSYNC)
    {
        bFileRsync = TRUE;
        printf("File Rsync requested\n");
    }
    
    // Receive file name to transfer from client
    recv(iSock, acFileName, NAME_MAX, 0);

    // Get full path name
    sprintf(acFilePath, "%s/%s", acFileDirectory, acFileName);

    printf("File requested: %s\n", acFilePath);

    if (bFileRsync)
    {
        handleFileRsyncRequest(iSock, acFilePath);
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
