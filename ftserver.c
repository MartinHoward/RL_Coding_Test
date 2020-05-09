#include <stdio.h>
#include <stdlib.h>

#include "ft_defs.h"

char acFileDirectory[PATH_MAX];

void handleFileUpdateRequest(int sock_fd, char *file_name)
{
    char acFileName[NAME_MAX];
    
    // Get status response from remote end before starting
    if (waitForStatus(sock_fd) == FT_READY_SEND)
    {
        // Generate a local temp file to store the hashes
        sprintf(acFileName, "tmpFile-%d", (int) pthread_self());
    
        // Get the hash file from the client
        receiveFileFromRemote(sock_fd, acFileName);
    
        // TODO: Process hash file and send response back
    
        // TODO: Delete temp file
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
