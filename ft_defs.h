#ifndef FT_DEFS_H
#define FT_DEFS_H

#define FT_IP_PORT      9999
#define MAX_CLIENTS     5
#define MAX_BLOCK_SIZE  256

typedef enum
{
    FT_FILE_NOT_FOUND    = 0,
    FT_READY_RECEIVE     = 1,
    FT_TRANSFER_CONTINUE = 2,
    FT_TRANSFER_COMPLETE = 3,
    FT_RECEIVE_ACK       = 4
} teFtStatus;

typedef struct
{
    teFtStatus eStatus;
    int        iByteCount;
    char       acDataBlock[MAX_BLOCK_SIZE];
} tsFtMessage;

teFtStatus waitForStatus(int sock_fd);
int sendStatus(int sock_fd, teFtStatus status);
int transferFileToRemote(int sock_fd, char* file_path);

#endif
