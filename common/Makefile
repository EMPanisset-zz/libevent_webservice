TOP=..
CC=gcc

SRCS=list.c

CFLAGS=-Wall -pipe -g -std=c11

CFLAGS += -I.

LDFLAGS=

OBJS=$(SRCS:.c=.o)

%.o: %.c Makefile $(wildcard *.h)
	$(CC) -c $(CFLAGS) -o $@ $<

all: list-test

list-test: list-test.o $(OBJS)

clean:
	rm -f *.a *.o *.so list-test
