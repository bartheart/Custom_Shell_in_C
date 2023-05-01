CC = gcc
CFLAGS = -Wall -Werror -pedantic -std=c99

Major2: major2.o 
	$(CC) $(CFLAGS) -o Major2 major2.o
major2.o: major2.c major2.h
	$(CC) $(CFLAGS) -c major2.c

clean:
	rm -f Major2 *.o
