#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "ft_defs.h"

char acFileDirectory[PATH_MAX];

// ***********************************************************************************************************
// Function: searchForMatchingBlock
//
// Searches for a block in the Rsync hash file using the given hash
//
// Inputs: data_block        pointer to the data block to search for
//         hash_file_name    pointer to string containing the name of the hash file to send to the server
//         hash              corresponding hash to the data block
//
// Returns: Block number starting at 0, -1 if no corresponding block found
// ***********************************************************************************************************        
int searchForMatchingBlock(BYTE *data_block, char *hash_file_name, unsigned long hash)
{
    int rc = -1, iBlock = 0;
    FILE *pHashFile;
    unsigned char acMd5Hash[MD5_HASH_SIZE];
    tsFtFileBlockHash sBlockHash;
    
    if ((pHashFile = fopen(hash_file_name, "rb")) != NULL)
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

// ***********************************************************************************************************
// Function: generateRsyncFile
//
// Generates an Rsync file to be sent to the client based on the hash file provided by the client and the
// file requested on the server
//
// Inputs: sock_fd           Socket file descriptor to the remove client
//         source_file_name  Name of file requested by the client          
//         hash_file_name    pointer to string containing the name of the hash file
//
// Returns: 0 on success, -1 on failure
// ***********************************************************************************************************        
int generateRsyncFile(int sock_fd, char *source_file_name, char *hash_file_name)
{
    int iBlock, rc = 0;
    char asRsyncFileName[NAME_MAX];
    BYTE cFirstByte, acDataBlock[MAX_BLOCK_SIZE];
    FILE *pSrcFile, *pRsyncFile;
    long lFilePos = 0, lSrcFileSize;
    struct stat st;
    unsigned long ulHash;
    BOOL bComputeRollingHash = FALSE;
    
    // Start by creating the temp rsync file
    sprintf(asRsyncFileName, "tmpRsyncFile-%d", (int) pthread_self());
    
    // Get the size of the source file
    stat(source_file_name, &st);
    lSrcFileSize = st.st_size;
       
    // Process hash file and generate rsync file
    if (((pSrcFile = fopen(source_file_name, "rb+")) != NULL) &&
         (pRsyncFile = fopen(asRsyncFileName, "wb")) != NULL)
    {
        while(lFilePos < lSrcFileSize)
        {
            // Read block
            fseek(pSrcFile, lFilePos, SEEK_SET);
            fread(acDataBlock, 1, sizeof(acDataBlock), pSrcFile);

            // Compute hash for block
            if (!bComputeRollingHash)
                ulHash = computeHashForBlock(acDataBlock, sizeof(acDataBlock));
            else
                ulHash = computeRollingHash(ulHash, cFirstByte, acDataBlock[sizeof(acDataBlock) - 1],
                                            sizeof(acDataBlock));
                            
            // Search for matching hash in hash file
            if ((iBlock = searchForMatchingBlock(acDataBlock, hash_file_name, ulHash)) != -1)
            {
                // Insert block reference in the rsync file
                fputc(RSYNC_DATATYPE_BLOCK, pRsyncFile);
                fwrite(&iBlock, sizeof(iBlock), 1, pRsyncFile);
                
                // Advance input file pointer by one block
                lFilePos += sizeof(acDataBlock);
                
                // Need to compute full hash for next block
                bComputeRollingHash = FALSE;
            }    
            else 
            {
                // Insert character reference in the rsync file
                cFirstByte = acDataBlock[0];
                fputc(RSYNC_DATATYPE_BYTE, pRsyncFile);
                fputc(cFirstByte, pRsyncFile);

                // Advance input file pointer by one byte
                lFilePos += 1;
                
                // Need to compute rolling hash
                bComputeRollingHash = TRUE;
            }
        }
        
        fclose(pSrcFile);
        fclose(pRsyncFile);
    }
    else 
    {
        rc = -1;
    }
    
    // Send the rsync file to the client
    if (transferFileToRemote(sock_fd, asRsyncFileName) == -1)
    {
        rc = -1;
    }
    
    // Remove tmpfile
    remove(asRsyncFileName);
          
    return rc;
}

// ***********************************************************************************************************
// Function: handleFileRsyncRequest
//
// Handles an Rsync request from the client. It receives the hash file from the client and generates an
// Rsync file which is then sent back to the client
//
// Inputs: sock_fd           Socket file descriptor to the remove client
//         file_name         Name of file requested by the client          
//
// Returns: 0 on success, -1 on failure
// ***********************************************************************************************************        
void handleFileRsyncRequest(int sock_fd, char *file_name)
{
    char acHashFileName[NAME_MAX];
    
    // Get status response from remote end before starting
    if (waitForStatus(sock_fd) == FT_READY_SEND)
    {
        // Generate a local temp file to store the hashes
        sprintf(acHashFileName, "tmpHashFile-%d", (int) pthread_self());
    
        // Get the hash file from the client
        if (receiveFileFromRemote(sock_fd, acHashFileName) == 0)
        {
            // Process hash file and send response back
            if (generateRsyncFile(sock_fd, file_name, acHashFileName) == -1)
            {
                printf("Failed to generate and send Rsync file to client\n");
            }
            
            // Delete temp file
            remove(acHashFileName);
        }
        else {
            printf("Failed to download hash file from client\n");
        }
    }
    else
    {
        printf("Fatal client error - Aborting\n");
    }
}

// ***********************************************************************************************************
// Function: handleClientRequestThread
//
// Main thread function to service file download and Rsync requests from the cloent
//
// Inputs: argv    Point to thread argument. In this case a pointer to the socket connected to the client        
//
// Outputs: None
// ***********************************************************************************************************        
void *handleClientRequestThread(void *argv)
{
    int iSock = *((int *)argv);
    char acFileName[NAME_MAX];
    char acFilePath[PATH_MAX+NAME_MAX];
    BOOL qFileRsync = FALSE;

    memset(acFileName, 0, sizeof(acFileName));
    memset(acFilePath, 0, sizeof(acFilePath));
    
    // Wait for client to indicate download type
    if (waitForStatus(iSock) == FT_FILE_RSYNC)
    {
        qFileRsync = TRUE;
        printf("File Rsync requested\n");
    }
    
    // Get file name to transfer from client
    recv(iSock, acFileName, NAME_MAX, 0);

    // Get full path name
    sprintf(acFilePath, "%s/%s", acFileDirectory, acFileName);

    printf("File requested: %s\n", acFilePath);

    if (qFileRsync)
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
            printf("Socket Error: failed to accept connection\n");
            exit(EXIT_FAILURE);
        }
    }
}
