# the compiler: gcc for C program, define as g++ for C++
CC = gcc

# compiler flags:
CFLAGS  = -Wall -pthread

all: FTServer FTClient

FTServer: FTServer.c 
	$(CC) $(CFLAGS) -o FTServer FTServer.c

FTClient: FTClient.c
	$(CC) $(CFLAGS) -o FTClient FTClient.c

clean: 
	rm FTServer FTClient

