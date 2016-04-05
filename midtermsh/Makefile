CC=gcc
CFLAGS = -c -Wall

midtermsh: midtermsh.o
	$(CC) midtermsh.o -o midtermsh

midtermsh.o: midtermsh.cpp
	$(CC) $(CFLAGS) midtermsh.cpp

clean:
	rm -f *o midtermsh
