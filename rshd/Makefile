CC=g++
CFLAGS = -c -Wall -std=c++11

rshd: rshd.o
	$(CC) rshd.o -o rshd

rshd.o: rshd.cpp
	$(CC) $(CFLAGS) rshd.cpp

clean:
	rm -f *o rshd