# Cameron Pickett 2012
# UbiComp Lab - University of Washington
# Electrisense

CC       = gcc
XCC      = mipsel-openwrt-linux-gcc
CFLAGS  += -Wall -g
LDFLAGS += -lcurl

BINS = bin/x86_posttest bin/posttest bin/x86_buffertest bin/buffertest

.PHONY: clean clobber
.SECONDARY:

all: $(BINS)

bin/%: obj/%.o
	$(XCC) -o $@ $^ $(LDFLAGS)
bin/x86_%: obj/x86_%.o
	$(CC) -o $@ $^ $(LDFLAGS)

obj/%.o: src/%.c $(wildcard src/%.h)
	$(XCC) -c $(CFLAGS) -o $@ $<
obj/x86_%.o: src/%.c $(wildcard src/%.h)
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	@-rm -f obj/*

clobber: clean
	@-rm -f bin/*
