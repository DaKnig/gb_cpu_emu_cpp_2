#include "debugger.hpp"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <vector>
using std::vector;
#include <algorithm>
using std::find;

static char* prompt=NULL;

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

static inline void draw_screen(struct SM83& cpu, char*, size_t) {
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

static inline void next_instr(struct SM83& cpu, char* line, size_t argc) {
    int times = 1;
    if (argc > 1)
	times = atoi(1+strchr(line, '\0'));
    struct SM83 copy = cpu;
    bool halt = cpu.misc.halt;
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

static inline void quiet_next(SM83& cpu, char* line, size_t argc) {
    int times = 1;
    if (argc > 1)
	times = atoi(1+strchr(line, '\0'));
    struct SM83 copy = cpu;
    bool halt = cpu.misc.halt;
    for (; times > 0; times--) {
	copy = cpu;
	halt = run_single_command(cpu);
    }
    if (copy.regs.pc == cpu.regs.pc) {
	printf("detected infinite loop at %04x\n", cpu.regs.pc);
    }
    cpu.misc.halt = halt;
}

static inline void exit_debugger(SM83&, char* line, size_t argc) {
    int status = 0;
    if (argc > 1)
	status = atoi(1+strchr(line, '\0'));
    exit(status);
}

void run_log(struct SM83& cpu, char* line, size_t argc) {
    bool halt;
    FILE* log_file=fopen(argc>1? strchr(line,'\0')+1: "/tmp/log.txt", "wx");
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
    puts("");
}

static inline void set_prompt(struct SM83&, char* line, size_t argc) {

    char* second_arg = line+strlen(line)+1;
    if (argc==1 || strcmp(second_arg,"--help")==0) { //print help
	puts("special formatting:\n"
	     " - %(af|bc|de|hl|pc|sp) - the double register, %04x\n"
	     " - %[hl] - the value hl points to in memory, %02x\n"
	     " - %(a|f|b|c|d|e|h|l) - the register, %02x\n"
	     " - %disasm - disassembly of the instruction at pc\n"
	     " - %mem((AF|BC|DE|HL|PC|SP)) - \"memory line\" around the mem "
	     "location pointed by the double register\n"
	     " - %mem(c) - prints the byte at 0xff00+c, %02x\n"
	     " - %%, \\n, \\t - like in printf\n");
	return;
    }

    char* line_tail = line+strlen(line)+1;
#ifdef DEBUG
    fprintf(stderr, "line_tail = %s\n", line_tail);
#endif
    free(prompt);
    for (const char* l=line_tail; *l; l++)
	argc -= *l == ' ';
    for (; argc>2; argc--) {
	*strchr(line_tail,'\0') = ' ';
    }
    prompt=strdup(line_tail);
    for (char* l=prompt; *l; l++) {
	if (strstr(l, "\\n")==l) {
	    l[0] = ' ';
	    l[1] = '\n';
	}
    }
}

static inline void get_prompt(struct SM83&, char*, size_t) {
    putchar('`');
    for (const char* l=prompt; *l; l++) {
	if (*l=='\n')
	    printf("\\n");
	else if (*l=='\t')
	    printf("\\t");
	else
	    putchar(*l);
    }
    puts("`");
}

static inline void print_regs_cmd(struct SM83& cpu, char*, size_t) {
    print_regs(cpu);
}

struct command {
    void (*fun) (struct SM83& cpu, char* line, size_t argc);
    const char* name;
    const char* help;
};

static inline int match_str(struct command commands[], const char* str) {
    /// return the index of the string that matches *str the best in *strings
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

static inline unsigned pretty_printer(const char* format, struct SM83& cpu) {
    unsigned chars = 0;

    const static char* double_regs[] { // double regs- stuff like %af or %pc
	"af","bc","de","hl","sp","pc",NULL};
    const char* regs[] { // 8bit regs- stuff like %a or %h
	"a","f","b","c","d","e","h","l",NULL};

    if (format==NULL)
	return 0;
    while (*format != '\0') {
	if (*format == '%') { // special formatting
	    format++;
	    if (*format == '%') { // %%
		putchar(*format++);
		chars++;
		goto next_char;
	    }
	    if (strstr(format, "mem(") == format) { // mem(reg|val)
		//output the full line in memory
		// partial matches would print something
		format+=sizeof("mem(")-1;
		char* endptr;
		unsigned offset = strtoul(format, &endptr, 0); // match int
		if (endptr != format) { // it was a number
		    format = endptr;
		    offset &= 0xffff;
		} else { // when this wasnt a number
		    // try to match a double reg
		    for (const char* reg = *double_regs; reg!=NULL; reg++) {
			if (strstr(format, reg)==format) {
			    offset = cpu.regs.registers[reg-*double_regs];
			    format+=2;
			    goto finish_matching;
			}
		    }
		    // try to match c for (ff00+c) instructions
		    if (format[0]=='c') {
			offset = cpu.regs.c + 0xff00;
			format++;
		    }
		}
	    finish_matching:
		if (format[0] != ')') { // to match mem(
		    goto next_char;
		}
		format++;
		print_mem_at_addr(cpu, offset);
		goto next_char;
	    }
	    if (strstr(format, "[hl]") == format) { // %[hl]
		printf("%02x",cpu.mem[cpu.regs.hl]);
		format+=sizeof("[hl]")-1;
		chars+=2;
		goto next_char;
	    }
	    if (strstr(format, "disasm") == format) { // %disasm
		disassemble_instruction(&cpu.mem[cpu.regs.pc]);
                // print disassembly
		format+=sizeof("disasm")-1;
		chars+=10+20; // rough estimate- machine code is 10 bytes
		goto next_char;
	    }
	    for (const char* reg = *double_regs; reg!=NULL; reg++) {
		if (strstr(format, reg)==format) {
		    printf("%04x", cpu.regs.registers[reg-*double_regs]);
		    format+=2;
		    chars+=4;
		    goto next_char;
		}
	    }
	    for (const char* reg = *regs; reg!=NULL; reg++) {
		if (strstr(format, reg)==format) {
		    // gotta check top-reg [abdh] or bottom-reg [fcel]
		    unsigned reg_num = reg-*regs;
		    if (reg_num%2==0) //top-reg
			printf("%02x", cpu.regs.registers[reg_num/2]>>8);
		    else //bottom-reg
			printf("%02x", cpu.regs.registers[reg_num/2]&0xff);
		    format++;
		    chars+=2;
		    goto next_char;
		}
	    }

	    putchar('%'); // if illegal formatting, print literally
	    goto next_char;
	} else { // regular chars
	    putchar(*format++);
	    chars++;
	}
    next_char:;
    }
    return chars;
}

void run_debugger(struct SM83& cpu) {
    char* line = (char*) calloc(20,1); strcpy(line, "next 0"); // no-op
    char* old_line = (char*) calloc(20, 1);
    size_t line_n = 15;

    struct command commands[] = {
	{next_instr, "next", "executes one instruction"},
	{continue_exec, "continue","continue executing until next stop"},
	{print_mem, "mem", "print the memory around specified address"},
	{print_regs_cmd, "regs", "print the values of the regs and flags"},
	{disasm, "disasm", "disassemble at specified address"},
	{draw_screen, "draw_screen", "print vram as ascii matrix"},
	{run_log, "run_log", "continue execution and log reg values into "
	 "a file"},
	{quiet_next, "qnext", "same as `next` without any output"},
	{exit_debugger, "exit", "call exit() with specified value, defaults"
	 "to 0"},
	{set_prompt, "set_prompt", "customize the prompt by using a format"
	 "stirng"},
	{get_prompt, "get_prompt", "print the format string for the prompt"},
	{NULL, NULL, NULL} // null delimited
    };

    prompt = strdup("\033[93m%pc\033[0m %disasm\n > ");

    puts("starting debugger");
    print_regs(cpu);
    do {
	free(old_line);
	old_line=strdup(line);
	// printf("\033[93m%04x\033[0m ", cpu.regs.pc); // print pc in yellow
	// disassemble_instruction(&cpu.mem[cpu.regs.pc]); // print disassembly
	// printf("\n > ");
	pretty_printer(prompt, cpu);
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
		for (i=0; commands[i].name != NULL; i++) {// find the command
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
    snprintf(command, 49, "python3 disassemble.py %02x %02x %02x %02x",
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
