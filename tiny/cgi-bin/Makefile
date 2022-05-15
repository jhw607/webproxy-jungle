CC = gcc
CFLAGS = -O2 -Wall -I ..

# make: adder

# adder: 
# 	./adder 127.0.0.1 9190

all: adder

adder: adder.c
	$(CC) $(CFLAGS) -o adder adder.c

clean:
	rm -f adder *~
