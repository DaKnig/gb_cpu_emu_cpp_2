CFLAGS=-O0 -ggdb -Wall
CC=g++
OBJS=objs/cpu.o objs/main.o objs/debugger.o

bin/main: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

objs/%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -f objs/*.o bin/main
