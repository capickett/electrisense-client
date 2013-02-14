# Cameron Pickett 2012
# UbiComp Lab - University of Washington
# Electrisense

CC       = gcc
XCC      = mipsel-openwrt-linux-gcc
CFLAGS  += -Wall -g
LDFLAGS += -lcurl

OBJS = obj/main.o obj/consumer.o obj/relay.o
X86OBJS = obj/x86_main.o obj/x86_consumer.o obj/x86_relay.o
BINS = bin/client bin/x86_client

.PHONY: clean
.SECONDARY:

all: $(BINS)
obj/main.o obj/x86_main.o: src/main.c src/main.h
obj/consumer.o obj/x86_consumer.o: src/consumer/consumer.c src/consumer/consumer.h
obj/relay.o obj/x86_relay.o: src/relay/relay.c src/relay/relay.h

docs:
	doxygen

bin/client: $(OBJS)
	$(XCC) -o $@ $(OBJS) $(LDFLAGS)

bin/x86_client: $(X86OBJS)
	$(CC) -o $@ $(X86OBJS) $(LDFLAGS)

$(OBJS):
	$(XCC) -c $(CFLAGS) -o $@ $<

$(X86OBJS):
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	@-rm -f obj/*
	@-rm -f bin/*
