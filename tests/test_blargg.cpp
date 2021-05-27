#include <stdlib.h>
#include <stdio.h>
#include "../cpu.hpp"
#include <ctype.h>

static inline void draw_screen(struct SM83& cpu, char screen[32][32]){
    const uint8_t SCX_C = cpu.mem[0xff43]/8;
    const uint8_t SCY_C = cpu.mem[0xff42]/8;
    const uint16_t BG_MAP = 0x9800;

    for (int y=SCY_C; y!=(SCY_C+18)%32; y=(y+1)%32) {
	
	sprintf(screen[y-SCY_C], "%02d ",y);
	for (int x=SCX_C; x!=(SCX_C+20)%32; x=(x+1)%32) {
	    char c = (char)cpu.mem[BG_MAP+y*0x20+x];
	    screen[y-SCY_C][x-SCX_C] = isprint(c)? c:' ';
	}
    }
    puts("\n   01234567890123456789");
}

int main(int argc, char* argv[]) {

    if (argc==1) {
	fprintf(stderr, "usage: %s test_rom.gb\n", argv[0]);
	exit(1);
    }

    struct SM83 cpu;
    clean_cpu(cpu);
    load_rom(cpu, argv[1]);

    run_cpu(cpu);

    // for now, print the screen
    char screen[32][32];
    draw_screen(cpu, screen);
}
