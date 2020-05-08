# the compiler: gcc for C program, define as g++ for C++
CC = gcc

# compiler flags:
CFLAGS  = -Wall -pthread

all: ftserver ftclient

ftserver: ftserver.c 
	$(CC) $(CFLAGS) -o ftserver ftserver.c

ftclient: ftclient.c
	$(CC) $(CFLAGS) -o ftclient ftclient.c

clean: 
	rm ftserver ftclient

