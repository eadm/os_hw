CC=gcc
CFLAGS = -c -Wall

sigusr: sigusr.o
	$(CC) sigusr.o -o sigusr

sigusr.o: sigusr.c
	$(CC) $(CFLAGS) sigusr.c

clean:
	rm -f *o sigusr
