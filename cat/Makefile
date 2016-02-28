CC=gcc
CFLAGS = -c -Wall

cat: cat.o
	$(CC) cat.o -o cat

cat.o: cat.c
	$(CC) $(CFLAGS) cat.c

clean:
	rm *o cat
