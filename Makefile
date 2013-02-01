# Cameron Pickett 2012
# UbiComp Lab - University of Washington
# Electrisense

CC      = gcc
XCC     = mipsel-openwrt-linux-gcc
CFLAGS  = -Wall -g
LDFLAGS = -lcurl

.PHONY: clean clobber
.SECONDARY:

all: bin/posttest bin/x86_posttest

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
