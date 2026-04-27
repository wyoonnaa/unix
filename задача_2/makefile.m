CC = gcc
CFLAGS = -Wall -Wextra -std=c99

all: file_lock

file_lock: file_lock.c
	$(CC) $(CFLAGS) -o file_lock file_lock.c

clean:
	rm -f file_lock *.lck stats.txt

.PHONY: all clean
