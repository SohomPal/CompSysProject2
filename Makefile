CC = gcc
CFLAGS = -std=gnu99 -Wall -g

all: part1 part3

part1: problem1_part1.c
	$(CC) $(CFLAGS) -o part1 problem1_part1.c

part3: problem1_part3.c
	$(CC) $(CFLAGS) -o part3 problem1_part3.c

clean:
	rm -f part1 part3 *.o output.txt
