# the compiler: gcc for C program, define as g++ for C++
CC = gcc

# compiler flags:
CFLAGS  = -Wall -pthread

all: ftserver ftclient

ftserver: ftserver.c ft_common.c ft_defs.h
	$(CC) $(CFLAGS) -o ftserver ftserver.c ft_common.c md5.c

ftclient: ftclient.c ft_common.c ft_defs.h
	$(CC) $(CFLAGS) -o ftclient ftclient.c ft_common.c md5.c

clean: 
	rm ftserver ftclient

