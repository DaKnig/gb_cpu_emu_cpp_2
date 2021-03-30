#include "cpu.hpp"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

bool run_single_command(struct SM83& cpu) {
    // add flags and all later
    uint8_t instr[4]; memcpy(instr, &cpu.mem[cpu.regs.pc], sizeof(instr));
    const uint8_t  imm8 = instr[1];
    const uint16_t imm16= instr[1] + (instr[2] << 8);

    const bool zf =     (cpu.regs.f>>7), nf = 1 & (cpu.regs.f>>6);
    const bool hf = 1 & (cpu.regs.f>>5), cf = 1 & (cpu.regs.f>>4);

    switch (instr[0]) {
    case 0x00:
	cpu.regs.pc++;
	return 0;
    case 0x10:
	cpu.regs.pc += 2;
	return 1;

#define LOAD(OPCODE, REG, VAL, SIZE)		\
    case OPCODE:				\
	cpu.regs. REG = VAL;			\
	cpu.regs.pc += SIZE;			\
	return 0

    LOAD(0x01, bc, imm16, 3);
    LOAD(0x11, de, imm16, 3);
    LOAD(0x21, hl, imm16, 3);
    LOAD(0x31, sp, imm16, 3);

#define INDIRECT_STORE_A(OPCODE, DEST)		\
    case OPCODE:				\
    	cpu.mem[cpu.regs. DEST ] = cpu.regs.a;	\
	cpu.regs.pc++;				\
	return 0
    INDIRECT_STORE_A(0x02, bc);
    INDIRECT_STORE_A(0x12, de);
    INDIRECT_STORE_A(0x22, hl++);
    INDIRECT_STORE_A(0x32, hl--);

#define REG_INC_OR_DEC(OPCODE,REG,OP)		\
    case OPCODE:				\
	cpu.regs. REG OP;			\
	cpu.regs.pc++;				\
	return 0

    REG_INC_OR_DEC(0x03, bc, ++);
    REG_INC_OR_DEC(0x13, de, ++);
    REG_INC_OR_DEC(0x23, hl, ++);
    REG_INC_OR_DEC(0x33, sp, ++);

    REG_INC_OR_DEC(0x04, b, ++);
    REG_INC_OR_DEC(0x14, d, ++);
    REG_INC_OR_DEC(0x24, h, ++);
    case 0x34:
	cpu.mem[cpu.regs.hl] ++;
	cpu.regs.pc++;
	return 0;

    REG_INC_OR_DEC(0x05, b, --);
    REG_INC_OR_DEC(0x15, d, --);
    REG_INC_OR_DEC(0x25, h, --);
    case 0x35:
	cpu.mem[cpu.regs.hl] --;
	cpu.regs.pc++;
	return 0;

    LOAD(0x06, b, imm8, 2);
    LOAD(0x16, d, imm8, 2);
    LOAD(0x26, h, imm8, 2);
    case 0x36:
	cpu.mem[cpu.regs.hl] = imm8;
	cpu.regs.pc+=2;
	return 0;

    // C <- [7 <- 0] <- [7]
    case 0x07: // RLCA
	cpu.regs.f &= 0x0f;
	cpu.regs.f |= (cpu.regs.a >> 7) << 4;
	cpu.regs.a = (cpu.regs.a << 1) + (cpu.regs.a >> 7);

	cpu.regs.pc++;
	return 0;
    // C <- [7 <- 0] <- C
    case 0x17: // RLA
	cpu.regs.f &= 0x0f;
	cpu.regs.f |= (cpu.regs.a >> 7) << 4;
	cpu.regs.a = (cpu.regs.a << 1) + cf;

	cpu.regs.pc++;
	return 0;
    // https://github.com/pinobatch/numism/blob/main/docs/gb_emu_testing.md
    case 0x27: {// DAA
	cpu.regs.f |= (nf == 0 && cpu.regs.a >= 0x9a) << 4;
	cpu.regs.f |= (nf == 0 && cpu.regs.a % 16 > 10) << 5;

	const uint8_t adjustment = 0x06 * hf + 0x60 * cf;
	const int res = cpu.regs.a + (nf ? -adjustment : adjustment);

	cpu.regs.a += res;

	cpu.regs.f &= 0x5f; // nf is unchanged; hf is zero; zf is calc'd
	cpu.regs.f |= (cpu.regs.a == 0) << 7; // what about cf???
	cpu.regs.f |= (res > 0x99 || res < 0) << 4; // I think???

	cpu.regs.pc++;
	return 0;
	}
    case 0x37: //SCF
	cpu.regs.f &= 0x8f; // only keep zf
	cpu.regs.f |= 1 << 4; //set cf

	cpu.regs.pc++;
	return 0;

    case 0x08:
	cpu.mem[imm16] = cpu.regs.sp;
	cpu.regs.pc+=3;
	return 0;

#define COND_JR(OPCODE, COND)				\
    case OPCODE:					\
	cpu.regs.pc += (int8_t) (COND ? imm8 : 2);	\
	return 0

    COND_JR(0x18, 1 );
    COND_JR(0x28, zf);
    COND_JR(0x38, cf);

    COND_JR(0x20,!zf);
    COND_JR(0x30,!cf);

#define ADD_HL(OPCODE, OPERAND)			\
    case OPCODE:				\
	cpu.regs.hl += cpu.regs. OPERAND ;	\
	cpu.regs.pc += 2;			\
	return 0

    ADD_HL(0x09, bc);
    ADD_HL(0x19, de);
    ADD_HL(0x29, hl);
    ADD_HL(0x39, sp);

#define LOAD_A(OPCODE, ADDR)			\
    case OPCODE:				\
	cpu.regs.a = cpu.mem[cpu.regs. ADDR];	\
	cpu.regs.pc ++;				\
	return 0

    LOAD_A(0x0a, bc);
    LOAD_A(0x1a, de);
    LOAD_A(0x2a, hl++);
    LOAD_A(0x3a, hl--);

    REG_INC_OR_DEC(0x0b, bc, --);
    REG_INC_OR_DEC(0x1b, de, --);
    REG_INC_OR_DEC(0x2b, hl, --);
    REG_INC_OR_DEC(0x3b, sp, --);

    REG_INC_OR_DEC(0x0c, c, ++);
    REG_INC_OR_DEC(0x1c, e, ++);
    REG_INC_OR_DEC(0x2c, l, ++);
    REG_INC_OR_DEC(0x3c, a, ++);

    REG_INC_OR_DEC(0x0d, c, --);
    REG_INC_OR_DEC(0x1d, e, --);
    REG_INC_OR_DEC(0x2d, l, --);
    REG_INC_OR_DEC(0x3d, a, --);

    LOAD(0x0e, c, imm8, 2);
    LOAD(0x1e, e, imm8, 2);
    LOAD(0x2e, l, imm8, 2);
    LOAD(0x3e, a, imm8, 2);

    // [0] -> [7 -> 0] -> C
    case 0x0f:
	cpu.regs.f &= 0x0f;
	cpu.regs.f |= (cpu.regs.a & 1) << 4;
	cpu.regs.a = (cpu.regs.a << 7) | (cpu.regs.a >> 1);

	cpu.regs.pc++;
	return 0;

    //C -> [7 -> 0] -> C
    case 0x1f: // RRA
	cpu.regs.f &= 0x0f;
	cpu.regs.f |= (cpu.regs.a & 1) << 4;
	cpu.regs.a = (cf << 7) | (cpu.regs.a >> 1);

	cpu.regs.pc++;
	return 0;

    case 0x2f: // CPL
	cpu.regs.f |= 0x60;
	cpu.regs.a ^= 0xff;

	cpu.regs.pc++;
	return 0;
    case 0x3f: // CCF
	cpu.regs.f &= 0x90; // nf = hf = 0
	cpu.regs.f ^= 0x10; // invert cf

	cpu.regs.pc++;
	return 0;
#define LOAD_FROM_ALL_REGS(REG, OFFSET)		\
    LOAD(OFFSET+0, REG, cpu.regs.b, 1);		        \
    LOAD(OFFSET+1, REG, cpu.regs.c, 1);		        \
    LOAD(OFFSET+2, REG, cpu.regs.d, 1);		        \
    LOAD(OFFSET+3, REG, cpu.regs.e, 1);		        \
    LOAD(OFFSET+4, REG, cpu.regs.h, 1);		        \
    LOAD(OFFSET+5, REG, cpu.regs.l, 1);		        \
    case OFFSET+6:				\
	cpu.regs. REG = cpu.mem[cpu.regs.hl];	\
	cpu.regs.pc++;				\
	return 0;				\
    LOAD(OFFSET+7, REG, cpu.regs.a, 1);

    LOAD_FROM_ALL_REGS(b, 0x40);
    LOAD_FROM_ALL_REGS(c, 0x48);
    LOAD_FROM_ALL_REGS(d, 0x50);
    LOAD_FROM_ALL_REGS(e, 0x58);
    LOAD_FROM_ALL_REGS(h, 0x60);
    LOAD_FROM_ALL_REGS(l, 0x68);
    LOAD_FROM_ALL_REGS(a, 0x78);

#define STORE_TO_HL(OPCODE, REG)		\
    case OPCODE:				\
	cpu.mem[cpu.regs.hl] = cpu.regs. REG ;	\
	cpu.regs.pc++;				\
	return 0

    STORE_TO_HL(0x70, b);
    STORE_TO_HL(0x71, c);
    STORE_TO_HL(0x72, d);
    STORE_TO_HL(0x73, e);
    STORE_TO_HL(0x74, h);
    STORE_TO_HL(0x75, l);
    case 0x76: // HALT
	cpu.regs.pc++;
	return 1;
    STORE_TO_HL(0x77, a);

#define XX_FOR_ALL_REGS(XX)			\
    XX(cpu.regs.b,0);				\
    XX(cpu.regs.c,1);				\
    XX(cpu.regs.d,2);				\
    XX(cpu.regs.e,3);				\
    XX(cpu.regs.h,4);				\
    XX(cpu.regs.l,5);				\
    XX(cpu.mem[cpu.regs.hl],6);			\
    XX(cpu.regs.a,7)

#define ADD(REG, OFFSET)				\
    case 0x80+OFFSET:					\
	cpu.regs.f = (cpu.regs.a+REG > 0xff) << 4;	\
	cpu.regs.f |= ((cpu.regs.a%16+REG%16)>16) << 5;	\
	cpu.regs.f |= (cpu.regs.a + REG == 0) << 7;	\
	cpu.regs.a += REG;				\
	cpu.regs.pc++;					\
	return 0

    XX_FOR_ALL_REGS(ADD);
#define ADC(REG, OFFSET)					\
    case 0x88+OFFSET:						\
	cpu.regs.f = (cpu.regs.a + REG + cf > 0xff) << 4;	\
	cpu.regs.f |= ((cpu.regs.a%16 + REG%16 + cf)>16) << 5;	\
	cpu.regs.f |= (cpu.regs.a + REG + cf == 0) << 7;	\
	cpu.regs.a += REG + cf;					\
	cpu.regs.pc++;						\
	return 0
    XX_FOR_ALL_REGS(ADC);
#define SUB(REG, OFFSET)				\
    case 0x90+OFFSET:					\
    	cpu.regs.f = (cpu.regs.a < REG) << 4;		\
	cpu.regs.f |= (cpu.regs.a%16 < REG%16) << 5;	\
	cpu.regs.f |= 1<<6;				\
	cpu.regs.f |= (cpu.regs.a == REG) << 7;		\
	cpu.regs.a -= REG;				\
	cpu.regs.pc++;					\
	return 0
    XX_FOR_ALL_REGS(SUB);
#define SBC(REG,OFFSET)					\
    case 0x98+OFFSET:					\
    	cpu.regs.f = (cpu.regs.a < REG+cf) << 4;	\
	cpu.regs.f |= (cpu.regs.a%16 < REG%16+cf) << 5;	\
	cpu.regs.f |= 1<<6;				\
	cpu.regs.f |= (cpu.regs.a == REG+cf) << 7;	\
	cpu.regs.a -= REG + cf;				\
	cpu.regs.pc++;					\
	return 0
    XX_FOR_ALL_REGS(SBC);
#define AND(REG,OFFSET)				\
    case 0xa0+OFFSET:				\
	cpu.regs.a &= REG;			\
	cpu.regs.f = ((cpu.regs.a==0)<<7) | 0x20;\
	cpu.regs.pc++;				\
	return 0
    XX_FOR_ALL_REGS(AND);
#define XOR(REG,OFFSET)				\
    case 0xa8+OFFSET:				\
	cpu.regs.a ^= REG;			\
	cpu.regs.f = (cpu.regs.a == 0) << 7;	\
	cpu.regs.pc++;				\
	return 0
    XX_FOR_ALL_REGS(XOR);
#define OR(REG,OFFSET)				\
    case 0xb0+OFFSET:				\
	cpu.regs.a |= REG;			\
	cpu.regs.f = (cpu.regs.a == 0) << 7;	\
	cpu.regs.pc++;				\
	return 0
    XX_FOR_ALL_REGS(OR);
#define CP(REG,OFFSET)					\
    	cpu.regs.f = (cpu.regs.a < REG) << 4;		\
	cpu.regs.f |= (cpu.regs.a%16 < REG%16) << 5;	\
	cpu.regs.f |= 1<<6;				\
	cpu.regs.f |= (cpu.regs.a == REG) << 7;		\
	cpu.regs.pc++;					\
	return 0
    XX_FOR_ALL_REGS(CP);
#define COND_RET(OPCODE, COND)				\
    case OPCODE:					\
	if (COND) {					\
	    cpu.regs.pc = cpu.mem[cpu.regs.sp++];	\
	    cpu.regs.pc|= cpu.mem[cpu.regs.sp++]<<4;	\
	} else						\
	    cpu.regs.pc++;				\
	return 0
    COND_RET(0xc0, !zf);
    COND_RET(0xd0, !cf);
    COND_RET(0xc8, zf);
    COND_RET(0xd8, cf);
    case 0xe0:
	cpu.mem[0xff00|imm8] = cpu.regs.a;
	cpu.regs.pc+=2;
	return 0;
    case 0xf0:
	cpu.regs.a = cpu.mem[0xff00|imm8];
	cpu.regs.pc+=2;
	return 0;
#define POP(OPCODE, REG)				\
    case OPCODE:					\
	cpu.regs.REG = cpu.mem[cpu.regs.sp++];		\
	cpu.regs.REG|= cpu.mem[cpu.regs.sp++]<<8;	\
	cpu.regs.pc++;					\
	return 0

    POP(0xc1, bc);
    POP(0xd1, de);
    POP(0xe1, hl);
    case 0xf1:
	cpu.regs.af = cpu.mem[cpu.regs.sp++] & 0xf0;
	cpu.regs.af|= cpu.mem[cpu.regs.sp++]<<8;
	cpu.regs.pc++;
	return 0;
#define COND_JP(OPCODE, COND)				\
    case OPCODE:					\
	cpu.regs.pc = COND ? imm16 : cpu.regs.pc + 3;	\
	return 0

    COND_JP(0xc2, !zf);
    COND_JP(0xd2, !cf);
    COND_JP(0xca, zf);
    COND_JP(0xda, cf);
    case 0xe2:
	cpu.mem[0xff00|cpu.regs.c] = cpu.regs.a;
	cpu.regs.pc++;
	return 0;
    case 0xf2:
	cpu.regs.a = cpu.mem[0xff00|cpu.regs.c];
	cpu.regs.pc++;
	return 0;
    COND_JP(0xc3, 1);
    // some bad instructions - hang and error out.
    case 0xf3: // DI; we dont need this.
	cpu.regs.pc++;// acts as a noop
	return 0;
#define COND_CALL(OPCODE, COND)					\
    case OPCODE:						\
	if (COND) {						\
	    cpu.mem[--cpu.regs.sp] = (cpu.regs.pc+3) >> 8;	\
	    cpu.mem[--cpu.regs.sp] = (cpu.regs.pc+3);		\
	    cpu.regs.pc = imm16;				\
	} else							\
	    cpu.regs.pc += 3;					\
	return 0
    COND_CALL(0xc4, !zf);
    COND_CALL(0xd4, !cf);
    COND_CALL(0xcc, zf);
    COND_CALL(0xdc, cf);
    COND_CALL(0xcd, 1);
#define PUSH(OPCODE, REG)				\
    case OPCODE:					\
	cpu.mem[--cpu.regs.sp] = cpu.regs.REG >> 8;	\
	cpu.mem[--cpu.regs.sp] = cpu.regs.REG;          \
	cpu.regs.pc++;					\
	return 0
    PUSH(0xc5, bc);
    PUSH(0xd5, de);
    PUSH(0xe5, hl);
    PUSH(0xf5, af);

#define OP_A_D8(OPCODE, OP, NF)					\
    case OPCODE: do {						\
	int res = cpu.regs.a OP imm8;				\
	int h = (uint8_t) (cpu.regs.a%16 OP imm8%16) > 16;	\
	cpu.regs.a = res;					\
	cpu.regs.f = (res>255 || res<0) << 4;			\
	cpu.regs.f|= h << 5;					\
	cpu.regs.f|= NF<< 6;					\
	cpu.regs.f|= (cpu.regs.a == 0) << 7;			\
	return 0;						\
    } while(0)

    OP_A_D8(0xc6,+,0);
    OP_A_D8(0xce,+cf+,0);
    OP_A_D8(0xd6,-,1);
    OP_A_D8(0xde,-cf-,1);
    OP_A_D8(0xe6,&,0);
    OP_A_D8(0xee,^,0);
    OP_A_D8(0xf6,|,0);
    case 0xfe: {
	int res = cpu.regs.a - imm8;
	cpu.regs.f = (res>255 || res<0) << 4;
	cpu.regs.f|= (cpu.regs.a%16 < imm8%16) << 5;
	cpu.regs.f|= (res == 0) << 7;
	return 0;
    }
#define RST(VECTOR)				\
    case 0xc7+VECTOR:			        \
	cpu.regs.pc = VECTOR;			\
	return 0
    RST(0x00); RST(0x08);
    RST(0x10); RST(0x18);
    RST(0x20); RST(0x28);
    RST(0x30); RST(0x38);

    case 0xe8: // ADD SP, r8
	cpu.regs.sp += (int8_t) imm8;
	cpu.regs.pc += 2;
	return 0;
    case 0xf8: //`LD HL, SP + r8
	cpu.regs.hl = cpu.regs.sp + (int8_t) imm8;
	cpu.regs.pc += 2;
	return 0;
    COND_RET(0xc9, 1);
    COND_RET(0xd9, 1);//reti == ret because we dont do interrupts anyways
    case 0xe9:
	cpu.regs.pc = cpu.regs.hl;
	cpu.regs.pc++;
	return 0;
    case 0xf9:
	cpu.regs.sp = cpu.regs.hl;
	cpu.regs.pc++;
	return 0;

    case 0xea: // ld (a16), a
	cpu.mem[imm16] = cpu.regs.a;
	cpu.regs.pc+=3;
	return 0;
    case 0xfa:
        cpu.regs.a = cpu.mem[imm16];
	cpu.regs.pc+=3;
	return 0;

    case 0xcb: // prefix
	fprintf(stderr, "0xcb prefix operations are not handled yet\n");
	return 1;
    case 0xfb: //EI; no interrupts -> noop
	cpu.regs.pc++;
	return 0;
    }


    fprintf(stderr, "unhandled opcode: %02x\n", instr[0]);
    return 1;
}

void print_regs(const struct SM83 cpu) {
    printf("af: %04x\nbc: %04x\nde: %04x\nhl: %04x\npc: %04x\nsp: %04x\n"
	   "z: %x, n: %x, h: %x, c: %x\n",
	   cpu.regs.af,
	   cpu.regs.bc,
	   cpu.regs.de,
	   cpu.regs.hl,
	   cpu.regs.pc,
	   cpu.regs.sp,
	   cpu.regs.zf, cpu.regs.nf, cpu.regs.hf, cpu.regs.cf);
}

void load_rom(struct SM83& cpu, const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
	fprintf(stderr, "can't open file `%s`\n", filename);
	perror("");
	return;
    }

    size_t bytes_read = fread(cpu.mem, 1, sizeof(cpu.mem), f);
    printf("read %zd bytes\n", bytes_read);

    fclose(f);
}

void clean_cpu(struct SM83& cpu) {
    memset(&cpu, 0xff, sizeof(cpu));
    cpu.regs.pc = 0x100;
}

static inline void print_mem(const struct SM83& cpu, int offset) {
    
    printf("%04x ", offset&0xfff0);
    
    for (int i=offset&0xfff0; (i>>4) == (offset>>4); i++) {
	printf("%02x ", cpu.mem[i]);
    }
    for (int i=offset&0xfff0; (i>>4) == (offset>>4); i++) {
	char c[2] = {'.', (char) cpu.mem[i]};
	putchar(c[ isprint(cpu.mem[i]) != 0 ]);
    }
    printf("\n%*c\n", 5+offset%16*3+1, '^');
}

void run_debugger(struct SM83& cpu) {
    bool halt = false;
    char* line = (char*) malloc(20);
    char* old_line = (char*) malloc(20);
    size_t line_n = 15;
    puts("starting debugger");
    print_regs(cpu);
    do {
	free(old_line);
	old_line=strdup(line);
	printf("> ");
	getline(&line, &line_n, stdin);
	if (line[0] == '\n')
	    strcpy(line, old_line);
	if (strcmp(line, "next\n") == 0 ||
	    strcmp(line, "n\n") == 0) {
	    
	    struct SM83 copy = cpu;
	    halt = run_single_command(cpu);
	    print_mem(cpu, cpu.regs.pc);
	    if (copy.regs.pc == cpu.regs.pc) {
		printf("detected infinite loop at %04x\n", cpu.regs.pc);
	    }
	}
	if (strstr(line, "mem ") == line) {
	    line+=4;
	    if (*line == '\0') {
		printf("usage: mem addr");
		continue;
	    }
	    int offset;
	    sscanf(line, "%i", &offset);
	    offset &= 0xffff;
	    print_mem(cpu, offset);

	    line-=4;
	}
	if (strcmp(line, "regs\n") == 0 ||
	    strcmp(line, "r\n") == 0) {
	    print_regs(cpu);
	    
	}
    } while (true);
}

void diassemble_instruction(uint8_t instr[4]) {
    
}
