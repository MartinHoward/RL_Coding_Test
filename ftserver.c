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

char acFileDirectory[PATH_MAX];
char acInterface[10] = "lo";

void * handleClientRequestThread(void *argv)
{
    int iSock = *((int *)argv);
    char acFileName[NAME_MAX];
    char acFilePath[PATH_MAX+NAME_MAX];

    // Receive file name to transfer from client
    recv(iSock, acFileName, NAME_MAX, 0);

    // Get full path name
    sprintf(acFilePath, "%s/%s", acFileDirectory, acFileName);

    printf("File requested: %s\n", acFilePath);

    transferFileToRemote(iSock, acFilePath);
    
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

    // Get file directory from command line or default to current directory
    //if (argc == 3)
    //{
    //    strcpy(acInterface, argv[1]);
    //    strcpy(acFileDirectory, argv[2]);
    //}
    if (argc == 2)
    {
        strcpy(acFileDirectory, argv[1]);
    }
    else if (argc == 1)
    {
        getcwd(acFileDirectory, sizeof(acFileDirectory));
    }
    else
    {
        printf("Usage: ftserver [INTERFACE] [DIR]\n");
        printf("Specify interface to bind to and directory containing files to serve\n");
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
