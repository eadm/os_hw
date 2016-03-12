CC=gcc
CFLAGS=-c -Wall

hello_world: hello_world.o
	$(CC) hello_world.o -o hello_world

hello_world.o: hello_world.c
	$(CC) $(CFLAGS) hello_world.c

clean:
	rm -f *o hello_world
