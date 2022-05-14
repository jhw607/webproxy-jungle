CC = gcc
make:   echoclient
test: echoclient.o
    ./echoclient ec2-52-79-233-155.ap-northeast-2.compute.amazonaws.com 9190
csapp.o: csapp.c csapp.h
    $(CC) -c csapp.c
echoclient.o: echoclient.c csapp.h
    $(CC) -c echoclient.c
echoclient: echoclient.o csapp.o
    $(CC) echoclient.o csapp.o -o echoclient
clean:
    rm -rf *.o echoclient