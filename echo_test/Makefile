# Makefile for Proxy Lab 
#
# You may modify this file any way you like (except for the handin
# rule). You instructor will type "make" on your specific Makefile to
# build your proxy from sources.

CC = gcc

make: echo_server echo_client

ctest: 
	./echo_client 127.0.0.1 9190

stest: 
	./echo_server  9190

csapp.o: csapp.c csapp.h
	$(CC) -c csapp.c

echo_server.o: echo_server.c csapp.h
	$(CC)  -c echo_server.c

echo_server: echo_server.o csapp.o
	$(CC) echo_server.o csapp.o -o echo_server

echo_client.o: echo_client.c csapp.h
	$(CC)  -c echo_client.c

echo_client: echo_client.o csapp.o
	$(CC) echo_client.o csapp.o -o echo_client

# Creates a tarball in ../proxylab-handin.tar that you can then
# hand in. DO NOT MODIFY THIS!

clean:
	rm -f *~ *.o echo_server

