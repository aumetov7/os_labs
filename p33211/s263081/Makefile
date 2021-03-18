CC=gcc
c_std=gnu11
CC_COMPILE_FLAGS=-Wall -Werror -Wpedantic -std=$(c_std)
CC_LIBS=-lpthread -pthread

all: build

main.o: main.c
	$(CC) $(CC_COMPILE_FLAGS) -c main.c

build: main.o
	$(CC) $(CC_LIBS) main.o -o main

clean:
	rm -f main main.o *.bin