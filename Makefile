# Makefile for signal handling assignment

CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99

all: problem1_part1 problem1_part2 problem1_part3

problem1_part1: problem1_part1.c
	$(CC) $(CFLAGS) -o $@ $<

problem1_part2: problem1_part2.c
	$(CC) $(CFLAGS) -o $@ $<

problem1_part3: problem1_part3.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f problem1_part1 problem1_part2 problem1_part3 outputPart*.txt

test_part1:
	./signals_test.sh part1

test_part2:
	./signals_test.sh part2

test_part3:
	./signals_test.sh part3

# Targets that generate output files
outputPart1.txt: problem1_part1
	./signals_test.sh part1

outputPart2.txt: problem1_part2
	./signals_test.sh part2

outputPart3.txt: problem1_part3
	./signals_test.sh part3

# Generate all output files
outputs: outputPart1.txt outputPart2.txt outputPart3.txt

.PHONY: all clean test_part1 test_part2 test_part3 outputs
