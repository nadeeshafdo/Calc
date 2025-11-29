CC = gcc
CFLAGS = -Wall -Wextra

all: calc

calc: main.o
	$(CC) -o calc main.o

clean:
	rm -f calc main.o
