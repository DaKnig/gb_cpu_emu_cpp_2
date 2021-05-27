#include "debugger.hpp"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <vector>
using std::vector;
#include <algorithm>
using std::find;


static inline void print_mem_at_addr(struct SM83& cpu, unsigned offset) {
    printf("%04x ", offset&0xfff0);

    for (unsigned i=offset&0xfff0; (i>>4) == (offset>>4); i++) {
	printf("%02x ", cpu.mem[i]);
    }
    for (unsigned i=offset&0xfff0; (i>>4) == (offset>>4); i++) {
	putchar(isprint(cpu.mem[i])? (char) cpu.mem[i]: '.');
    }
    printf("\n%*c\n", 5+offset%16*3+1, '^');
}

static inline void print_mem(struct SM83& cpu, char* line, size_t argc) {

    if (argc == 1) {
	printf("usage: mem addr\n");
	return;
    }
    const char* second_arg = line+strlen(line)+1;
    uint16_t offset = strtoul(second_arg, NULL, 16);// the 2nd arg

    print_mem_at_addr(cpu, offset);
}

static inline size_t sep_args(char* line) { ////### UNSAFE, REWRITE THIS!
    size_t argc = 0;
    for(; *line != '\0'; line++)
	if (*line == ' ' || *line == '\n') {
	    *line = '\0';
	    argc++;
	}
    return argc;
}

static inline void draw_screen(struct SM83& cpu, char*, size_t){
    const uint8_t SCX_C = cpu.mem[0xff43]/8;
    const uint8_t SCY_C = cpu.mem[0xff42]/8;
    const uint16_t BG_MAP = 0x9800;
//    const uint8_t (*BG_MAP)[32][32] = (uint8_t*[32][32])&cpu.mem[0x9800];
    for (int y=SCY_C; y!=(SCY_C+18)%32; y=(y+1)%32) {
	printf("\n%02d ",y);
	for (int x=SCX_C; x!=(SCX_C+20)%32; x=(x+1)%32) {
	    char c = (char)cpu.mem[BG_MAP+y*0x20+x];
	    putchar(isprint(c)? c:' ');
	}
    }
    puts("\n   01234567890123456789");
}

static inline void next_instruction(struct SM83& cpu, char* line, size_t argc) {
    int times = 1;
    if (argc > 1)
	times = atoi(1+strchr(line, '\0'));
    struct SM83 copy;
    bool halt;
    for (; times > 0; times--) {
	copy = cpu;
	halt = run_single_command(cpu);
	print_mem_at_addr(cpu, cpu.regs.pc);
	if (times != 1) {
	    printf("\033[93m%04x\033[0m ", cpu.regs.pc);
	    disassemble_instruction(&cpu.mem[cpu.regs.pc]);
	    puts("");
	}
    }
    if (copy.regs.pc == cpu.regs.pc) {
	printf("detected infinite loop at %04x\n", cpu.regs.pc);
    }
    cpu.misc.halt = halt;
}
void run_log(struct SM83& cpu, char* line, size_t argc) {
    bool halt;
    FILE* log_file = fopen(argc>1? strchr(line,'\0')+1: "log.txt", "wx");
    if (!log_file) {
	perror("Can't open log file");
	return;
    }
    uint16_t prev_pc = cpu.regs.pc+1;
    do {
	prev_pc = cpu.regs.pc;
	fprintf(log_file,
		"A: %02X F: %02X B: %02X C: %02X "
		"D: %02X E: %02X H: %02X L: %02X "
		"SP: %04X PC: 00:%04X ",
//	       "(%02X %02X %02X %02X)\n",
	       cpu.regs.a, cpu.regs.f, cpu.regs.b, cpu.regs.c,
	       cpu.regs.d, cpu.regs.e, cpu.regs.h, cpu.regs.l,
	       cpu.regs.sp, cpu.regs.pc
	       //cpu.mem[cpu.pc], cpu.mem[cpu.pc+1], cpu.mem[cpu.pc+2],
	       );
	fputc('(', log_file);
	for (int i=0; i<3; i++)
	    fprintf(log_file, "%02X ", cpu.mem[cpu.regs.pc+i]);
	fprintf(log_file, "%02X)\n", cpu.mem[cpu.regs.pc+3]);

	halt = run_single_command(cpu);
    } while(!halt && prev_pc != cpu.regs.pc &&
	    find(
		cpu.misc.breakpoints.begin(),
		cpu.misc.breakpoints.end(), cpu.regs.pc) ==
	    cpu.misc.breakpoints.end());
    if (prev_pc == cpu.regs.pc) {
	printf("detected infinite loop at %04x\n", cpu.regs.pc);
    }
    fclose(log_file);
    cpu.misc.halt = halt;
}

static inline void continue_exec(struct SM83& cpu, char* line, size_t argc) {
    (void) argc;
    (void) line;
    uint16_t prev_pc = cpu.regs.pc+1;
    if (cpu.misc.halt)
	return;
    bool hit_breakpoint = false;
    while(!cpu.misc.halt && prev_pc != cpu.regs.pc && !hit_breakpoint) {
	prev_pc = cpu.regs.pc;
	cpu.misc.halt = run_single_command(cpu);
	hit_breakpoint = find(
	    cpu.misc.breakpoints.begin(),
	    cpu.misc.breakpoints.end(), cpu.regs.pc) !=
	    cpu.misc.breakpoints.end();

    }
    if (prev_pc == cpu.regs.pc) {
	printf("detected infinite loop at %04x\n", cpu.regs.pc);
    }
}

static inline void disasm(struct SM83& cpu, char* line, size_t argc) {
    char* second_arg = line+strlen(line)+1;
    uint16_t addr;
    if (argc == 1)
	addr = cpu.regs.pc;
    else
	addr = strtoul(second_arg, NULL, 16);
    disassemble_instruction(&cpu.mem[addr]);
}

static inline void print_regs_cmd(struct SM83& cpu, char* line, size_t argc) {
    print_regs(cpu);
}

struct command {
    void (*fun) (struct SM83& cpu, char* line, size_t argc);
    const char* name;
    const char* help;
};

static inline int match_str(struct command commands[], const char* str) {
    /// return the index of the string that matches *str* the best in *strings
    /// return -1 upon failure

    int ret_val = -1;
    for (unsigned i=0; commands[i].name != NULL; i++) {
	if (strstr(commands[i].name, str) == commands[i].name) {
	    // can later upgrade to something better
	    ret_val = i;
	}
    }
    return ret_val;
}

void run_debugger(struct SM83& cpu) {
    char* line = (char*) calloc(20,1);
    char* old_line = (char*) calloc(20, 1);
    size_t line_n = 15;



    struct command commands[] = {
	{next_instruction, "next", "executes one instruction"},
	{continue_exec, "continue","continue executing until next stop"},
	{print_mem, "mem", "print the memory around specified address"},
	{print_regs_cmd, "regs", "print the values of the regs and flags"},
	{disasm, "disasm", "disassemble at specified address"},
	{draw_screen, "draw_screen", "print vram as ascii matrix"},
	{run_log, "run_log", "continue execution and log reg values into "
	 "a file"},
	{NULL, NULL, NULL} // null delimited
    };

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

	int matching_index = match_str(commands, line);
	if (matching_index != -1) {
	    commands[matching_index].fun(cpu, line, argc);
	} else if (strstr("help", line)) {
	    if (argc == 1) {
		printf("list of commands:\n");
		for (size_t i=0; commands[i].name != NULL; i++) {
		    printf("\033[1m%s\033[0m -- %s.\n",
			commands[i].name, commands[i].help);
		}
	    } else {
		char* second_arg = line+strlen(line)+1;
		size_t i;
		for (i=0; commands[i].name != NULL; i++) { // find the command
		    if (strstr(commands[i].name, second_arg) ==
			commands[i].name) {
			break;
		    }
		}
		printf("%s.\n", commands[i].help);
	    }
	} else if (strcmp(line, "breakpoints") == 0) {
	    printf("breakpoints:\n");
	    for (size_t i=0; i<cpu.misc.breakpoints.size(); i++)
		printf("%3zd %04x\n", i, cpu.misc.breakpoints[i]);
	} else if (strcmp(line, "break") == 0 ||
		   strcmp(line, "b") == 0) {
	    if (argc == 1) {
		puts("usage: break addr");
		continue;
	    }
	    uint16_t bp = strtoul(line+strlen(line)+1, NULL, 16);
	    cpu.misc.breakpoints.push_back(bp);
	} else if (strcmp(line, "rembreak") == 0 ||
		   strcmp(line, "rb") == 0) {
	    if (argc == 1) {
		puts("usage: rembreak breakpoint_number");
		continue;
	    }
	    size_t i = strtoul(line+strlen(line)+1, NULL, 16);
	    if (i < cpu.misc.breakpoints.size())
		cpu.misc.breakpoints.erase(cpu.misc.breakpoints.begin()+i);
	} else {
	    printf("unrecognized command `%s`\n", line);
	}
    } while (!feof(stdin));
}

int disassemble_instruction(const uint8_t instr[4]) {
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
