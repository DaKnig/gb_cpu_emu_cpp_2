CFLAGS=-O0 -ggdb -Wall
CC=g++

bin/main: objs/cpu.o objs/main.o
	$(CC) $(CFLAGS) -o $@ $^

objs/%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -f objs/*.o bin/main
