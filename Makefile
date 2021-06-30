CFLAGS=-Wall -std=gnu++17 -Werror -march=native -mtune=native
#CFLAGS+=-ggdb -Og
CFLAGS+=-O3
#CFLAGS+=-O3 -flto
CFLAGS+=-fprofile-arcs -pg
#CFLAGS+=-fprofile-use
#CFLAGS+=-fbranch-probabilities

CC:=c++
OBJS:=objs/core/cpu.o objs/debugger/debugger.o
INCLUDES:=core/
TESTS:=bin/test/test_blargg

all: bin/debugger/debugger_main $(TESTS)

.SECONDARY:

bin/%: objs/%.o $(OBJS) | bin/debugger/ bin/test/
	$(CC) $(CFLAGS) -o $@ $^

objs/%.o: %.cpp Makefile | objs/core/ objs/debugger/ objs/test/
	$(CC) $(CFLAGS) -I$(INCLUDES) -c -o $@ $<

%/:
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf objs bin
	rm -if *~ */*~
