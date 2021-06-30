#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cpu.hpp"
#include <ctype.h>

#define ELEM(X) (sizeof(X) / sizeof(X[0]))

static inline void draw_screen(struct SM83& cpu, char screen[20][30]){
    const uint8_t SCX_C = cpu.mem[0xff43]/8;
    const uint8_t SCY_C = cpu.mem[0xff42]/8;
    const uint16_t BG_MAP = 0x9800;

    for (int y=SCY_C, i=0; y!=(SCY_C+18)%32; y=(y+1)%32, i++) {

	sprintf(screen[i], "%02d ",y);
	for (int x=SCX_C, j=3; x!=(SCX_C+20)%32; x=(x+1)%32, j++) {
	    char c = (char)cpu.mem[BG_MAP+y*0x20+x];
	    screen[i][j] = isgraph(c)? c:' ';
	}
    }
    sprintf(screen[18], "   01234567890123456789");
}

int main(int, char*[]) {

    struct SM83 cpu;

    const char test_rom_names[][50] = {
	"cpu_instrs/individual/01-special.gb",
//	"cpu_instrs/individual/02-interrupts.gb", //I don't emulate interrupts
	"cpu_instrs/individual/03-op sp,hl.gb",
	"cpu_instrs/individual/04-op r,imm.gb",
	"cpu_instrs/individual/05-op rp.gb",
	"cpu_instrs/individual/06-ld r,r.gb",
	"cpu_instrs/individual/07-jr,jp,call,ret,rst.gb",
	"cpu_instrs/individual/08-misc instrs.gb",
	"cpu_instrs/individual/09-op r,r.gb",
	"cpu_instrs/individual/10-bit ops.gb",
	"cpu_instrs/individual/11-op a,(hl).gb",
    };

    // loop over test_rom_names, display which did not display "Passed"
    for (unsigned i=0; i<ELEM(test_rom_names); i++) {
	clean_cpu(cpu);
	load_rom(cpu, test_rom_names[i]);

	run_cpu(cpu);

	char screen[20][30] = {{0}};
	draw_screen(cpu, screen);

	char* relevant_line = screen[16];
	if (strstr(relevant_line, "Failed") != NULL) {
	    for (unsigned i=0;i<ELEM(screen);i++)
		puts(screen[i]);
	} else if (strstr(relevant_line, "Passed") != NULL) {
	    printf("%s Passed!\n", test_rom_names[i]);
	} else {
	    printf("error: `%s` doesnt have `Passed` or `Failed`",
		   relevant_line);
	}
    }
}
