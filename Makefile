CFLAGS=-O0 -ggdb -Wall --std=gnu++17 -Werror
CC=g++
OBJS=objs/cpu.o objs/main.o objs/debugger.o

bin/main: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

objs/%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -f objs/*.o bin/main
