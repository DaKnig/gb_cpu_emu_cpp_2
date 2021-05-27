CFLAGS=-O0 -ggdb -Wall --std=gnu++17 -Werror
CC=g++
OBJS=objs/cpu.o objs/debugger.o
TESTS=bin/test_blargg

all: bin/main

tests: $(TESTS)

bin/%: $(OBJS) objs/%.o
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

objs/%.o: %.cpp objs
	$(CC) $(CFLAGS) -c -o $@ $<

objs/%.o: tests/%.cpp objs
	$(CC) $(CFLAGS) -c -o $@ $<

objs:
	mkdir objs

.PHONY: clean
clean:
	rm -rf objs/* bin/*
	rm -if *~ */*~

# Do not remove intermediate files (like assets, c->asm files, etc)
# .PRECIOUS:
