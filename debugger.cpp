#include "debugger.hpp"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <vector>
using std::vector;
#include <algorithm>
using std::find;


static inline void print_mem(const struct SM83& cpu, int offset) {
    
    printf("%04x ", offset&0xfff0);
    
    for (int i=offset&0xfff0; (i>>4) == (offset>>4); i++) {
	printf("%02x ", cpu.mem[i]);
    }
    for (int i=offset&0xfff0; (i>>4) == (offset>>4); i++) {
	putchar(isprint(cpu.mem[i])? (char) cpu.mem[i]: '.');
    }
    printf("\n%*c\n", 5+offset%16*3+1, '^');
}

static inline size_t sep_args(char* line) {
    size_t argc = 0;
    for(; *line != '\0'; line++)
	if (*line == ' ' || *line == '\n') {
	    *line = '\0';
	    argc++;
	}
    return argc;
}

void run_debugger(struct SM83& cpu) {
    bool halt = false;
    (void) halt;
    char* line = (char*) malloc(20);
    char* old_line = (char*) calloc(20, 1);
    size_t line_n = 15;

    std::vector<uint16_t> breakpoints;

    puts("starting debugger");
    print_regs(cpu);
    do {
	free(old_line);
	old_line=strdup(line);
	printf("\033[93m%04x\033[0m ", cpu.regs.pc);
	disassemble_instruction(&cpu.mem[cpu.regs.pc]);
	printf("\n > ");
	getline(&line, &line_n, stdin);
	
	if (line[0] == '\n')
	    strcpy(line, old_line);
	size_t argc = sep_args(line);
	
	if (strcmp(line, "next") == 0 ||
	    strcmp(line, "n") == 0) {
	    int times = 1;
	    if (argc > 1)
		times = atoi(1+strchr(line, '\0'));
	    struct SM83 copy;
	    for (; times > 0; times--) {
		copy = cpu;
		halt = run_single_command(cpu);
		print_mem(cpu, cpu.regs.pc);
		if (times != 1) {
		    printf("\033[93m%04x\033[0m ", cpu.regs.pc);
		    disassemble_instruction(&cpu.mem[cpu.regs.pc]);
		    puts("");
		}
	    }
	    if (copy.regs.pc == cpu.regs.pc) {
		printf("detected infinite loop at %04x\n", cpu.regs.pc);
	    }
	} else if (strcmp(line, "continue") == 0 ||
		   strcmp(line, "c") == 0) {
	    uint16_t prev_pc = cpu.regs.pc+1;
	    while(!halt && prev_pc != cpu.regs.pc &&
		  find(breakpoints.begin(), breakpoints.end(), cpu.regs.pc) ==
		    breakpoints.end()){
		prev_pc = cpu.regs.pc;
		halt = run_single_command(cpu);
	    }
	    if (prev_pc == cpu.regs.pc) {
		printf("detected infinite loop at %04x\n", cpu.regs.pc);
	    }
	} else if (strcmp(line, "mem") == 0) {
	    if (argc == 1) {
		printf("usage: mem addr");
		continue;
	    }
	    const char* second_arg = line+strlen(line)+1;
	    uint16_t offset = strtoul(second_arg, NULL, 16);// the 2nd arg
	    print_mem(cpu, offset);

	} else if (strcmp(line, "regs") == 0 ||
		   strcmp(line, "r") == 0) {
	    print_regs(cpu);
	} else if (strcmp(line, "disasm") == 0 ||
		   strcmp(line, "d") == 0) {
	    char* second_arg = line+strlen(line)+1;
	    uint16_t addr;
	    if (argc == 1)
		addr = cpu.regs.pc;
	    else
		addr = strtoul(second_arg, NULL, 16);
	    disassemble_instruction(&cpu.mem[addr]);
	} else if (strcmp(line, "breakpoints") == 0) {
	    printf("breakpoints:\n");
	    for (size_t i=0; i<breakpoints.size(); i++)
		printf("%3zd %04x\n", i, breakpoints[i]);
	} else if (strcmp(line, "break") == 0 ||
		   strcmp(line, "b") == 0) {
	    if (argc == 1) {
		puts("usage: break addr");
		continue;
	    }
	    uint16_t bp = strtoul(line+strlen(line)+1, NULL, 16);
	    breakpoints.push_back(bp);
	} else if (strcmp(line, "rembreak") == 0 ||
		   strcmp(line, "rb") == 0) {
	    if (argc == 1) {
		puts("usage: rembreak breakpoint_number");
		continue;
	    }
	    size_t i = strtoul(line+strlen(line)+1, NULL, 16) & 0xffff;
	    if (i < breakpoints.size())
		breakpoints.erase(breakpoints.begin()+i);
	}
    } while (!feof(stdin));
}

int disassemble_instruction(uint8_t instr[4]) {
    char command[50] = "";
    snprintf(command, 49, "python disassemble.py %02x %02x %02x %02x",
	     (int)instr[0], (int)instr[1], (int)instr[2], (int)instr[3]);
#ifdef DEBUG
    puts(command);
#endif
    FILE* out_stream=popen(command, "r");
    if (out_stream == NULL)
	return -1;
    char disasm[100]={0}; fread(disasm, 1, 99, out_stream);
    strchr(disasm, '\0')[-1] = '\0';
    int ret_val = WEXITSTATUS(pclose(out_stream)); 
    // for more info - man 3 pclose -> wait4 -> waitpid.
#ifdef DEBUG
    printf("[python returned %d]\n", ret_val);
#endif
    for (int i=0; i<ret_val; i++) {
	printf("%02x ", instr[i]);
    }
    printf("%*c%s", 3*(3-ret_val), ' ', disasm);
    return ret_val;
}
