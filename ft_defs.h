#ifndef FT_DEFS_H
#define FT_DEFS_H

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <limits.h>

#define FALSE           0
#define TRUE            !FALSE

#define FT_IP_PORT      9999
#define MAX_CLIENTS     5
#define MAX_BLOCK_SIZE  256

typedef enum
{
    FT_FILE_NOT_FOUND    = 0,
    FT_READY_RECEIVE     = 1,
    FT_READY_SEND        = 2,
    FT_TRANSFER_CONTINUE = 3,
    FT_TRANSFER_COMPLETE = 4,
    FT_RECEIVE_ACK       = 5
} teFtStatus;

typedef struct
{
    teFtStatus eStatus;
    int        iByteCount;
    char       acDataBlock[MAX_BLOCK_SIZE];
} tsFtFileBlock;

typedef int BOOL;

int sendStatus(int sock_fd, teFtStatus status);
teFtStatus waitForStatus(int sock_fd);
void transferFileToRemote(int sock_fd, char* file_path);
void receiveFileFromRemote(int sock_fd, char* file_name, BOOL update);

#endif
