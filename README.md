# RL_Coding_Test

This GIT repo contains the implementation for the RL coding test. It implements a file server and client. It implements both straight file transfer and rsync in the case where the file already resides on the client and we wish to update to the latest version.

Executables:

ftserver:

A very unoriginal name for the server that transfers requested files or Rsync files for updating files on the client. You can specify a directory to contain files to service or let it use the current directory by default.

Usage: ftserver [DIR]
Specify directory containing files to serve, defauft is current working directory

It will bind to all interfaces on the host at port 9999. I used a multithreaded approach in my implementation. I could have gone single threaded and used the select function to monitor all connections. The multithreaded approach is cleaner. I did not implement a limit on the number of clients for simplicity.


ftclient:

The client that downloads or requests an Rsync from the server. It will store files in the working directory.

Usage: ftclient [FILE] [HOST]
Specify file to transfer from server

Both parameters are required. If the requested file already resides in the working directory it will generate a hash file containing both a simple hash and MD5 for each block and send it to the server and wait for an Rsync file and then use that to update the local file to the same as the server's copy.


Transfer protocol:

I went with pipelining to reduce latency that can occur by signalling between client and server for each block.

Receiver											Sender
========											======

Filename   	----------------------------------->
		   	<----------------------------------- READY SEND
READY REC  	----------------------------------->
		   	<----------------------------------- Block 0 (TRANSFER_CONTINUE)
		   	<----------------------------------- Block 1 (TRANSFER_CONTINUE)
		   	<----------------------------------- Block 2 (TRANSFER_CONTINUE)
						   .
						   .
						   .
		   	<----------------------------------- Block n-1 (TRANSFER_CONTINUE)
		   	<----------------------------------- Block n   (TRANSFER_COMPLETE)
RECEIPT ACK ----------------------------------->
						
		
Source files:

ftclient.c		Original code that implements client 
ftserver.c		Original code that implements server
ft_common.c		Common code used by server and client
ft_defs.h		Header file containing definitions used by above source files

The following code was pulled from RFC 1321. I had issues installing the OpenSSL library on my Mac so I decided to make use of this implementation.
md5.h
md5.c		


Building:

Simply run make on the command line. There should be no external dependencies especially since I steered away from using OpenSSL.


Rsync:

I implemented the Rsync algorithm to keep files in sync without transferring the complete file each time. I make use of a simple hash that is inexpensive to roll. The MD5 hash is used to vastly minimise the chance of false positives.

The mechanism works as follows:

Client											Server
======											======

Generate hash file
Send hash file to server -------------------->
												Generate Rsync file based on hash file
						 <--------------------  Send Rsync file to client
Client builds new
version based on 
Rsync file


Not yet implemented:

1. A means of comparing the MD5 hash between the files on server and client before doing an Rsync
2. Some improved error management

