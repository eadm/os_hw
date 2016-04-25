CC=g++
CFLAGS = -c -Wall -std=c++11

simplesh: simplesh.o
	$(CC) simplesh.o -o simplesh

simplesh.o: simplesh.cpp
	$(CC) $(CFLAGS) simplesh.cpp

clean:
	rm -f *o simplesh