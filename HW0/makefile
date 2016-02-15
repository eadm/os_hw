CC=gcc
CFLAGS=-c -Wall

hello: main.o
	$(CC) main.o -o hello

main.o: main.c
	$(CC) $(CFLAGS) main.c

clean:
	rm *o hello
