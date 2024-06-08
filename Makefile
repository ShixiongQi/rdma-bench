CC=gcc
CFLAGS=-Wall -Werror -Wno-stringop-truncation -O3
INCLUDES=
LDFLAGS=-libverbs
LIBS=-pthread

SRCS=main.c client.c config.c ib.c server.c setup_ib.c sock.c
OBJS=$(SRCS:.c=.o)
PROG=rdma-tutorial

all: $(PROG)

debug: CFLAGS=-Wall -Werror -g -DDEBUG
debug: $(PROG)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)

clean:
	$(RM) *.o *~ $(PROG)
