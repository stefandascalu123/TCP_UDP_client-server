CC=gcc
CFLAGS=-Wall -Werror -O3

server: server.c
	$(CC) $(CFLAGS) $^ -o $@

subscriber: subscriber.c
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f server subscriber