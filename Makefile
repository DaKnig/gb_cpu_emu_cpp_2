CFLAGS=-Wall -std=gnu++17 -Werror -march=native -mtune=native
#CFLAGS+=-ggdb -Og
CFLAGS+=-O3
#CFLAGS+=-O3 -flto
CFLAGS+=-fprofile-arcs -pg
#CFLAGS+=-fprofile-use
#CFLAGS+=-fbranch-probabilities

CC=g++
OBJS=objs/cpu.o objs/debugger.o
TESTS=bin/test_blargg

all: bin/main tests

.SECONDARY:

tests: $(TESTS)

bin/%: $(OBJS) objs/%.o | bin
	$(CC) $(CFLAGS) -o $@ $^

objs/%.o: %.cpp Makefile | objs
	$(CC) $(CFLAGS) -c -o $@ $<

objs/%.o: test/%.cpp Makefile | objs
	$(CC) $(CFLAGS) -c -o $@ $<

objs:
	mkdir $@

bin:
	mkdir $@

.PHONY: clean
clean:
	rm -rf objs/* bin/*
	rm -if *~ */*~
