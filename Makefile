CC=gcc
CFLAGS=-g -Wall

myshell: shell1.c
	$(CC) $(CFLAGS) -o myshell shell1.c

clean:
	rm -f myshell