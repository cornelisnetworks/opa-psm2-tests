#
#  This file is provided under a dual BSD/GPLv2 license.  When using or
#  redistributing this file, you may do so under either license.
#
#  Copyright(c) 2026 Cornelis Networks.
#

CFLAGS += -Wall -g -I.. -I.
LIBS = -lpsm2

SRCS = test_tool.c
OBJS = $(SRCS:.c=.o)
SHARED_OBJS = ../libpsm2.o ../psm2perf.o

TARGET = test_tool

all: $(TARGET)

$(TARGET): $(OBJS) $(SHARED_OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(SHARED_OBJS) $(LIBS)

$(OBJS): %.o: %.c test_tool.h ../libpsm2.h ../psm2perf.h
	$(CC) $(CFLAGS) -c $<

$(SHARED_OBJS):
	$(MAKE) -C .. $(notdir $@)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
