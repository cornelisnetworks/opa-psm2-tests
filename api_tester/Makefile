TOP = ..
CFLAGS += -Wall -g -I$(TOP) -I.
LIBS = -lpsm2 -lrt

BIN = api_tester
OBJS = api_tester.o
SHARED_OBJS = $(TOP)/libpsm2.o $(TOP)/psm2perf.o

all: shared_objs $(BIN)

shared_objs:
	$(MAKE) -C $(TOP) libpsm2.o psm2perf.o

$(BIN): $(OBJS) $(SHARED_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

api_tester.o: api_tester.c api_tester.h $(TOP)/libpsm2.h $(TOP)/psm2perf.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(BIN) $(OBJS)

clobber: clean
	rm -f *~

.PHONY: all shared_objs clean clobber
