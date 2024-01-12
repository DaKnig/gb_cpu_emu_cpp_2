CFLAGS=-Wall -Werror -march=native -mtune=native
#CFLAGS+=-ggdb -Og
CFLAGS+=-Oz
#CFLAGS+=-O3 -flto
#CFLAGS+=-fprofile-arcs -pg
#CFLAGS+=-fprofile-use
#CFLAGS+=-fbranch-probabilities
CSTD=-std=gnu2x
CXXSTD=-std=gnu++23

CC:=cc
CXX:=g++
OBJS:=objs/core/cpu.o objs/debugger/debugger.o
INCLUDES:=core/
TESTS:=bin/test/test_blargg

all: main $(TESTS)
main: bin/debugger/debugger_main
	ln --force -s $^ $@

.SECONDARY:

bin/%: objs/%.o $(OBJS) | bin/debugger/ bin/test/
	$(CXX) $(CXXSTD) $(CFLAGS) -o $@ $^

objs/%.o: %.cpp Makefile | objs/core/ objs/debugger/ objs/test/
	$(CXX) $(CXXSTD) $(CFLAGS) -I$(INCLUDES) -c -o $@ $<

objs/%.o: %.c Makefile | objs/core/ objs/debugger/ objs/test/
	$(CC) $(CSTD) $(CFLAGS) -I$(INCLUDES) -c -o $@ $<

%/:
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf objs bin
	rm main
	rm -if *~ */*~
