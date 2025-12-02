CC = gcc
CFLAGS = -Wall -Wextra

all: calc

calc: src/main.o
	$(CC) -o calc src/main.o

clean:
	rm -f calc src/main.o
