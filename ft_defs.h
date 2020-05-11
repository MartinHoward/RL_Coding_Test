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

#define FT_IP_PORT           9999
#define MAX_CLIENTS          5
#define MAX_BLOCK_SIZE       1024
#define MD5_HASH_SIZE        16
#define RSYNC_DATATYPE_BYTE  'C'
#define RSYNC_DATATYPE_BLOCK 'B'

typedef unsigned char   BYTE;
typedef int BOOL;

typedef enum
{
    FT_FILE_NOT_FOUND    = 0,
    FT_FILE_RSYNC        = 1,
    FT_FILE_NEW          = 2,
    FT_READY_RECEIVE     = 3,
    FT_READY_SEND        = 4,
    FT_TRANSFER_CONTINUE = 5,
    FT_TRANSFER_COMPLETE = 6,
    FT_RECEIVE_ACK       = 7
} teFtStatus;

typedef struct
{
    teFtStatus eStatus;
    int        iByteCount;
    BYTE       acDataBlock[MAX_BLOCK_SIZE];
} tsFtFileBlock;

typedef struct
{
    unsigned long ulHash;
    unsigned char acMd5Hash[MD5_HASH_SIZE];
} tsFtFileBlockHash;

unsigned long computeHashForBlock(BYTE *data_block, int block_size);
unsigned long computeRollingHash(unsigned long hash, BYTE byte_to_roll, BYTE byte_to_add, int block_size);
unsigned char *computeMd5HashForBlock(BYTE *data_block, int block_size, unsigned char* hash);
int sendStatus(int sock_fd, teFtStatus status);
teFtStatus waitForStatus(int sock_fd);
int transferFileToRemote(int sock_fd, char* file_path);
int receiveFileFromRemote(int sock_fd, char* file_name);

#endif
