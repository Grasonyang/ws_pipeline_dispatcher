CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g -O2 -D_POSIX_C_SOURCE=200809L
LDFLAGS =

SRCS_SERVER = rtp_server.c gap_fsm.c
OBJS_SERVER = $(SRCS_SERVER:.c=.o)

TARGET = rtp_server

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS_SERVER)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

rtp_server.o: rtp_server.c rtp.h gap_fsm.h
gap_fsm.o:    gap_fsm.c    gap_fsm.h rtp.h

clean:
	rm -f $(OBJS_SERVER) $(TARGET)
