CC=gcc
CFLAGS=-Wall -g -Iinclude

run: build
	./memmgr

build:
	$(CC) $(CFLAGS) main.c -o memmgr

compare: build
	./memmgr
	diff out.txt correct.txt

clean:
	rm -f memmgr out.txt