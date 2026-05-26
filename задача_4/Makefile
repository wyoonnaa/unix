CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = 

all: server client test_client

server: server.c
	$(CC) $(CFLAGS) -o server server.c $(LDFLAGS)

client: client.c
	$(CC) $(CFLAGS) -o client client.c $(LDFLAGS)

test_client: test_client.c
	$(CC) $(CFLAGS) -o test_client test_client.c $(LDFLAGS)

clean:
	rm -f server client test_client *.log *.txt client_*.log test_numbers.txt /tmp/brownian_bot.sock result.txt

run: all
	./runme.sh

.PHONY: all clean run
