#include <stdio.h>

#include "cpu.hpp"
#include "debugger.hpp"

int main(int argc, char* argv[]) {
    struct SM83 cpu;
    clean_cpu(cpu);

    if (argv[1]) {
	load_rom(cpu, argv[1]);
    } else {
	fprintf(stderr, "usage: %s filename\n", argv[0]);
    }
    run_debugger(cpu);
}
