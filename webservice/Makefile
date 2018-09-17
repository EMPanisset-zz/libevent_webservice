TOP=..
CC=gcc

COMMON_DIR=$(TOP)/common
HTTP-PARSER_DIR=$(TOP)/http-parser

SRCS=$(HTTP-PARSER_DIR)/http_parser.c pthread.c pthread_rwlock.c pthread_mutex.c hashtable.c
SRCS += worker.c tcp_socket.c http_service.c http_session.c session.c
SRCS += $(COMMON_DIR)/list.c $(COMMON_DIR)/slist.c

CFLAGS=-Wall -pipe -g -std=gnu99

CFLAGS += -I. -I$(COMMON_DIR) -I$(TOP)/libevent/include -I$(TOP)/http-parser -I$(TOP)/jansson/src

LDFLAGS=-lpthread

OBJS=$(SRCS:.c=.o)

%.o: %.c Makefile $(wildcard *.h)
	$(CC) -c $(CFLAGS) -o $@ $<

PROGS=tigera_webserver thread_test worker_test

LIBS=../libevent/.libs/libevent.a ../libevent/.libs/libevent_pthreads.a ../jansson/src/.libs/libjansson.a

all: $(LIBS) $(PROGS)

tigera_webserver: tigera_webserver.o $(OBJS) 
	$(CC) $^ $(LDFLAGS) $(LIBS) -o $@

thread_test: thread_test.o pthread.o pthread_rwlock.o 
	$(CC) $^ $(LDFLAGS) -o $@

worker_test: worker_test.o pthread.o pthread_rwlock.o worker.o
	$(CC) $^ $(LDFLAGS) $(LIBS) -o $@

../libevent/.libs/libevent.a: | ../libevent
	cd $| && \
	./autogen.sh && \
	CFLAGS="-m64" ./configure --host=x86_64 --disable-debug-mode --disable-samples --disable-libevent-regress && \
	make -j3

../jansson/src/.libs/libjansson.a: | ../jansson
	cd $| && \
	autoreconf -ivf && \
	CFLAGS="-m64" ./configure --host=x86_64 && \
	make -j3

clean:
	rm -f *.a *.o *.so $(COMMON_DIR)/*.o $(PROGS)
	cd ../libevent; make clean
