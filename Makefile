# the compiler: gcc for C program, define as g++ for C++
CC = gcc

# compiler flags:
CFLAGS  = -Wall -pthread -lssl -lcrypto

all: ftserver ftclient

ftserver: ftserver.c ft_common.c ft_defs.h
	$(CC) $(CFLAGS) -o ftserver ftserver.c ft_common.c

ftclient: ftclient.c ft_common.c ft_defs.h
	$(CC) $(CFLAGS) -o ftclient ftclient.c ft_common.c

clean: 
	rm ftserver ftclient

