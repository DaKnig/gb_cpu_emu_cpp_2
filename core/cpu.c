#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#include "cpu.h"

//#define DEBUG

static inline bool run_single_prefix_command(struct SM83* cpu);

bool run_single_command(struct SM83* cpu) {
    // add flags and all later
    uint8_t instr[4]; memcpy(instr, &cpu->mem[cpu->regs.pc], sizeof(instr));
    const uint8_t  imm8 = instr[1];
    const uint16_t imm16= instr[1] + (instr[2] << 8);

    const bool zf =     (cpu->regs.f>>7), nf = 1 & (cpu->regs.f>>6);
    const bool hf = 1 & (cpu->regs.f>>5), cf = 1 & (cpu->regs.f>>4);
    (void)hf, (void)nf;

	uint16_t* regpair_offset_bcdehlsp(uint8_t idx) {
		switch(idx) {
		case 0: return &cpu->regs.bc;
		case 1: return &cpu->regs.de;
		case 2: return &cpu->regs.hl;
		case 3: return &cpu->regs.sp;
		default: __builtin_unreachable();
		}
	}
 	uint8_t* regpair_offset_bcdehlhl(uint8_t idx) {
		switch(idx) {
		case 0: return &cpu->mem[cpu->regs.bc];
		case 1: return &cpu->mem[cpu->regs.de];
		case 2: return &cpu->mem[cpu->regs.hl++];
		case 3: return &cpu->mem[cpu->regs.hl--];
		default: __builtin_unreachable();
		}
	}


	if (instr[0] == 0) {
		cpu->regs.pc++;
		return 0;
	} else if (instr[0] == 0x10) {
		cpu->regs.pc += 2;
		return 1;
	/* } else if (instr[0] == 0x01 || instr[0] == 0x11 || */
	/* 		   instr[0] == 0x21 || instr[0] == 0x31) { */
	/* 	uint8_t regpair = regpair_offset_bcdehlsp(instr[0]>>4); */
	/* 	cpu->regs.registers[regpair] = imm16; */
	/* 	cpu->regs.pc += 3; */
	/* 	return 0; */
	}
    switch (instr[0]) {
#define LOAD(OPCODE, REG, VAL, SIZE)		\
    case OPCODE:				\
	cpu->regs. REG = VAL;			\
	cpu->regs.pc += SIZE;			\
	return 0

	case 0x01: case 0x11: case 0x21: case 0x31:
		uint16_t* regpair = regpair_offset_bcdehlsp(instr[0] >> 4);
		*regpair = imm16;
		cpu->regs.pc += 3;
		return 0;
	case 0x02: case 0x12: case 0x22: case 0x32:
		*regpair_offset_bcdehlhl(instr[0] >> 4) = cpu->regs.a;
		cpu->regs.pc++;
		return 0;
	case 0x03: case 0x13: case 0x23: case 0x33:
		++*regpair_offset_bcdehlsp(instr[0] >> 4);
		cpu->regs.pc++;
		return 0;
	case 0x0b: case 0x1b: case 0x2b: case 0x3b:
		--*regpair_offset_bcdehlsp(instr[0] >> 4);
		cpu->regs.pc++;
		return 0;

#define REG_INC_OR_DEC(OPCODE,REG,OP)					\
    case OPCODE:							\
	do {								\
	    uint32_t old_val = REG;					\
	    uint32_t new_val = OP old_val;				\
	    cpu->regs.f = cpu->regs.f & 0x10; /* only keep cf */	\
	    cpu->regs.f|= (new_val&0xff) == 0? 0x80 : 0x00;		\
	    cpu->regs.f|= # OP [0]=='-' ? 0x40 : 0x00;			\
	    if ((# OP [0] == '-' && new_val%16 == 0xf) ||		\
		(# OP [0] == '+' && new_val%16 == 0x0))	/*hf*/		\
		cpu->regs.f|= 0x20;					\
	    REG = new_val;						\
	    cpu->regs.pc++;						\
	    return 0;							\
	} while(0)

    REG_INC_OR_DEC(0x04, cpu->regs.b, ++);
    REG_INC_OR_DEC(0x14, cpu->regs.d, ++);
    REG_INC_OR_DEC(0x24, cpu->regs.h, ++);
    REG_INC_OR_DEC(0x34, cpu->mem[cpu->regs.hl], ++);

    REG_INC_OR_DEC(0x05, cpu->regs.b, --);
    REG_INC_OR_DEC(0x15, cpu->regs.d, --);
    REG_INC_OR_DEC(0x25, cpu->regs.h, --);
    REG_INC_OR_DEC(0x35, cpu->mem[cpu->regs.hl], --);

    LOAD(0x06, b, imm8, 2);
    LOAD(0x16, d, imm8, 2);
    LOAD(0x26, h, imm8, 2);
    case 0x36:
	cpu->mem[cpu->regs.hl] = imm8;
	cpu->regs.pc+=2;
	return 0;

    case 0x07: case 0x17: case 0x0f: case 0x1f: // RLCA RLA RRCA RRA
		cpu->regs.pc--;
		run_single_prefix_command(cpu);
		cpu->regs.f &= 0x10; // zero the zero
		return 0;
    // https://github.com/pinobatch/numism/blob/main/docs/gb_emu_testing.md
    case 0x27: {// DAA
	{
	    int16_t result = cpu->regs.af >> 8;
	    enum {GB_ZERO_FLAG=0x80, GB_SUBTRACT_FLAG=0x40,
		GB_HALF_CARRY_FLAG=0x20, GB_CARRY_FLAG=0x10};

	    cpu->regs.af &= ~(0xFF00 | GB_ZERO_FLAG);

	    if (cpu->regs.af & GB_SUBTRACT_FLAG) {
		if (cpu->regs.af & GB_HALF_CARRY_FLAG) {
		    result = (result - 0x06) & 0xFF;
		}

		if (cpu->regs.af & GB_CARRY_FLAG) {
		    result -= 0x60;
		}
	    }
	    else {
		if ((cpu->regs.af & GB_HALF_CARRY_FLAG) || (result & 0x0F) > 0x09) {
		    result += 0x06;
		}

		if ((cpu->regs.af & GB_CARRY_FLAG) || result > 0x9F) {
		    result += 0x60;
		}
	    }

	    if ((result & 0xFF) == 0) {
		cpu->regs.af |= GB_ZERO_FLAG;
	    }

	    if ((result & 0x100) == 0x100) {
		cpu->regs.af |= GB_CARRY_FLAG;
	    }

	    cpu->regs.af &= ~GB_HALF_CARRY_FLAG;
	    cpu->regs.af |= result << 8;
	}
	cpu->regs.pc++;
	return 0;
	}
    case 0x37: //SCF
	cpu->regs.f &= 0x8f; // only keep zf
	cpu->regs.f |= 1 << 4; //set cf

	cpu->regs.pc++;
	return 0;

    case 0x08: // ld (a16), sp
	cpu->mem[imm16] = cpu->regs.sp & 0xff;
	cpu->mem[imm16+1]=cpu->regs.sp >> 8;
	cpu->regs.pc+=3;
	return 0;

#define COND_JR(OPCODE, COND)				\
    case OPCODE:					\
	cpu->regs.pc += 2;				\
	cpu->regs.pc += (int8_t) (COND ? imm8 : 0);	\
	return 0

    COND_JR(0x18, 1 );
	case 0x20: case 0x28: case 0x30: case 0x38:
		cpu->regs.pc += 2;
		uint8_t cond = !(instr[0] & 8) - ((instr[0] & 16) ? cf : zf);
		/* uint8_t cond = instr[0] == ; */
		cpu->regs.pc += (int8_t) (cond ? imm8 : 0);
		return 0;

#define ADD_HL(OPCODE, REG)						\
    case OPCODE:							\
	do {								\
	    uint32_t new_val = cpu->regs.hl+cpu->regs. REG ;		\
	    cpu->regs.f &= 0x80;					\
	    cpu->regs.f |= (new_val>0xffff ? 0x10:0);			\
	    int bottom_12 = (cpu->regs.hl&0x0fff)+(cpu->regs. REG&0x0fff); \
	    cpu->regs.f |= (bottom_12>0x0fff ? 0x20:0);			\
	    cpu->regs.hl= new_val;					\
	    cpu->regs.pc++;						\
	    return 0;							\
	} while(0)

    ADD_HL(0x09, bc);
    ADD_HL(0x19, de);
    ADD_HL(0x29, hl);
    ADD_HL(0x39, sp);

#define LOAD_A(OPCODE, ADDR)			\
    case OPCODE:				\
	cpu->regs.a = cpu->mem[cpu->regs. ADDR];	\
	cpu->regs.pc ++;				\
	return 0

    LOAD_A(0x0a, bc);
    LOAD_A(0x1a, de);
    LOAD_A(0x2a, hl++);
    LOAD_A(0x3a, hl--);


    REG_INC_OR_DEC(0x0c, cpu->regs.c, ++);
    REG_INC_OR_DEC(0x1c, cpu->regs.e, ++);
    REG_INC_OR_DEC(0x2c, cpu->regs.l, ++);
    REG_INC_OR_DEC(0x3c, cpu->regs.a, ++);

    REG_INC_OR_DEC(0x0d, cpu->regs.c, --);
    REG_INC_OR_DEC(0x1d, cpu->regs.e, --);
    REG_INC_OR_DEC(0x2d, cpu->regs.l, --);
    REG_INC_OR_DEC(0x3d, cpu->regs.a, --);

    LOAD(0x0e, c, imm8, 2);
    LOAD(0x1e, e, imm8, 2);
    LOAD(0x2e, l, imm8, 2);
    LOAD(0x3e, a, imm8, 2);

    case 0x2f: // CPL
	cpu->regs.f |= 0x60;
	cpu->regs.a ^= 0xff;

	cpu->regs.pc++;
	return 0;
    case 0x3f: // CCF
	cpu->regs.f &= 0x90; // nf = hf = 0
	cpu->regs.f ^= 0x10; // invert cf

	cpu->regs.pc++;
	return 0;
#define LOAD_FROM_ALL_REGS(REG, OFFSET)		\
    LOAD(OFFSET+0, REG, cpu->regs.b, 1);		        \
    LOAD(OFFSET+1, REG, cpu->regs.c, 1);		        \
    LOAD(OFFSET+2, REG, cpu->regs.d, 1);		        \
    LOAD(OFFSET+3, REG, cpu->regs.e, 1);		        \
    LOAD(OFFSET+4, REG, cpu->regs.h, 1);		        \
    LOAD(OFFSET+5, REG, cpu->regs.l, 1);		        \
    case OFFSET+6:				\
	cpu->regs. REG = cpu->mem[cpu->regs.hl];	\
	cpu->regs.pc++;				\
	return 0;				\
    LOAD(OFFSET+7, REG, cpu->regs.a, 1);

    LOAD_FROM_ALL_REGS(b, 0x40);
    LOAD_FROM_ALL_REGS(c, 0x48);
    LOAD_FROM_ALL_REGS(d, 0x50);
    LOAD_FROM_ALL_REGS(e, 0x58);
    LOAD_FROM_ALL_REGS(h, 0x60);
    LOAD_FROM_ALL_REGS(l, 0x68);
    LOAD_FROM_ALL_REGS(a, 0x78);

#define STORE_TO_HL(OPCODE, REG)		\
    case OPCODE:				\
	cpu->mem[cpu->regs.hl] = cpu->regs. REG ;	\
	cpu->regs.pc++;				\
	return 0

    STORE_TO_HL(0x70, b);
    STORE_TO_HL(0x71, c);
    STORE_TO_HL(0x72, d);
    STORE_TO_HL(0x73, e);
    STORE_TO_HL(0x74, h);
    STORE_TO_HL(0x75, l);
    case 0x76: // HALT
	cpu->regs.pc++;
	return 1;
    STORE_TO_HL(0x77, a);

#define XX_FOR_ALL_REGS(XX)	 \
    XX(cpu->regs.b,0);		 \
    XX(cpu->regs.c,1);		 \
    XX(cpu->regs.d,2);		 \
    XX(cpu->regs.e,3);		 \
    XX(cpu->regs.h,4);		 \
    XX(cpu->regs.l,5);           \
    XX(cpu->mem[cpu->regs.hl],6);\
    XX(cpu->regs.a,7)

#define ADD(REG, OFFSET)					\
    case 0x80+OFFSET:						\
	cpu->regs.f = (cpu->regs.a+REG > 0xff) << 4;		\
	cpu->regs.f |= ((cpu->regs.a%16+REG%16)>=16)? 0x20: 0;	\
	cpu->regs.f |= (cpu->regs.a + REG)%256 == 0? 0x80: 0;	\
	cpu->regs.a += REG;					\
	cpu->regs.pc++;						\
	return 0

    XX_FOR_ALL_REGS(ADD);
#define ADC(REG, OFFSET)						\
    case 0x88+OFFSET:							\
	cpu->regs.f = (cpu->regs.a + REG + cf > 0xff)? 0x10: 0;		\
	cpu->regs.f |= ((cpu->regs.a%16 + REG%16 + cf)>=16)? 0x20: 0;	\
	cpu->regs.f |= (cpu->regs.a + REG + cf)%256 == 0? 0x80: 0;	\
	cpu->regs.a += REG + cf;					\
	cpu->regs.pc++;							\
	return 0
    XX_FOR_ALL_REGS(ADC);
#define SUB(REG, OFFSET)				\
    case 0x90+OFFSET:					\
    	cpu->regs.f = (cpu->regs.a < REG) << 4;		\
	cpu->regs.f |= (cpu->regs.a%16 < REG%16) << 5;	\
	cpu->regs.f |= 1<<6;				\
	cpu->regs.f |= (cpu->regs.a == REG) << 7;		\
	cpu->regs.a -= REG;				\
	cpu->regs.pc++;					\
	return 0
    XX_FOR_ALL_REGS(SUB);
#define SBC(REG,OFFSET)							\
    case 0x98+OFFSET:							\
	cpu->regs.f = (cpu->regs.a < REG+cf) << 4;			\
	cpu->regs.f |= (cpu->regs.a%16 < REG%16+cf) << 5;		\
	cpu->regs.f |= 1<<6;						\
	cpu->regs.f |= (cpu->regs.a - REG - cf)%256 == 0? 0x80: 0;	\
	cpu->regs.a -= REG + cf;					\
	cpu->regs.pc++;							\
	return 0
    XX_FOR_ALL_REGS(SBC);
#define AND(REG,OFFSET)				\
    case 0xa0+OFFSET:				\
	cpu->regs.a &= REG;			\
	cpu->regs.f = ((cpu->regs.a==0)<<7) | 0x20;\
	cpu->regs.pc++;				\
	return 0
    XX_FOR_ALL_REGS(AND);
#define XOR(REG,OFFSET)				\
    case 0xa8+OFFSET:				\
	cpu->regs.a ^= REG;			\
	cpu->regs.f = (cpu->regs.a == 0) << 7;	\
	cpu->regs.pc++;				\
	return 0
    XX_FOR_ALL_REGS(XOR);
#define OR(REG,OFFSET)				\
    case 0xb0+OFFSET:				\
	cpu->regs.a |= REG;			\
	cpu->regs.f = (cpu->regs.a == 0) << 7;	\
	cpu->regs.pc++;				\
	return 0
    XX_FOR_ALL_REGS(OR);
#define CP(REG,OFFSET)					\
    case 0xb8+OFFSET:					\
	cpu->regs.f = (cpu->regs.a < REG) << 4;		\
	cpu->regs.f |= (cpu->regs.a%16 < REG%16) << 5;	\
	cpu->regs.f |= 1<<6;				\
	cpu->regs.f |= (cpu->regs.a == REG) << 7;	\
	cpu->regs.pc++;					\
	return 0
    XX_FOR_ALL_REGS(CP);
#define COND_RET(OPCODE, COND)				\
    case OPCODE:					\
	if (COND) {					\
	    cpu->regs.pc = cpu->mem[cpu->regs.sp++];	\
	    cpu->regs.pc|= cpu->mem[cpu->regs.sp++]<<8;	\
	} else						\
	    cpu->regs.pc++;				\
	return 0
    COND_RET(0xc0, !zf);
    COND_RET(0xd0, !cf);
    COND_RET(0xc8, zf);
    COND_RET(0xd8, cf);
    case 0xe0:
	cpu->mem[0xff00|imm8] = cpu->regs.a;
	cpu->regs.pc+=2;
	return 0;
    case 0xf0:
	cpu->regs.a = cpu->mem[0xff00|imm8];
	cpu->regs.pc+=2;
	return 0;
#define POP(OPCODE, REG)				\
    case OPCODE:					\
	cpu->regs.REG = cpu->mem[cpu->regs.sp++];		\
	cpu->regs.REG|= cpu->mem[cpu->regs.sp++]<<8;	\
	cpu->regs.pc++;					\
	return 0

    POP(0xc1, bc);
    POP(0xd1, de);
    POP(0xe1, hl);
    case 0xf1:
	cpu->regs.af = cpu->mem[cpu->regs.sp++] & 0xf0;
	cpu->regs.af|= cpu->mem[cpu->regs.sp++]<<8;
	cpu->regs.pc++;
	return 0;
#define COND_JP(OPCODE, COND)				\
    case OPCODE:					\
	cpu->regs.pc = COND ? imm16 : cpu->regs.pc + 3;	\
	return 0

    COND_JP(0xc2, !zf);
    COND_JP(0xd2, !cf);
    COND_JP(0xca, zf);
    COND_JP(0xda, cf);
    case 0xe2:
	cpu->mem[0xff00|cpu->regs.c] = cpu->regs.a;
	cpu->regs.pc++;
	return 0;
    case 0xf2:
	cpu->regs.a = cpu->mem[0xff00|cpu->regs.c];
	cpu->regs.pc++;
	return 0;
    COND_JP(0xc3, 1);
    // some bad instructions - hang and error out.
    case 0xf3: // DI; we dont need this.
	cpu->regs.pc++;// acts as a noop
	return 0;
#define COND_CALL(OPCODE, COND)					\
    case OPCODE:						\
	cpu->regs.pc+=3;						\
	if (COND) {						\
	    cpu->mem[--cpu->regs.sp] = cpu->regs.pc >> 8;		\
	    cpu->mem[--cpu->regs.sp] = cpu->regs.pc;		\
	    cpu->regs.pc = imm16;				\
	}							\
	return 0
    COND_CALL(0xc4, !zf);
    COND_CALL(0xd4, !cf);
    COND_CALL(0xcc, zf);
    COND_CALL(0xdc, cf);
    COND_CALL(0xcd, 1);
#define PUSH(OPCODE, REG)				\
    case OPCODE:					\
	cpu->mem[--cpu->regs.sp] = cpu->regs.REG >> 8;	\
	cpu->mem[--cpu->regs.sp] = cpu->regs.REG;          \
	cpu->regs.pc++;					\
	return 0
    PUSH(0xc5, bc);
    PUSH(0xd5, de);
    PUSH(0xe5, hl);
    PUSH(0xf5, af);

#define OP_A_D8(OPCODE, OP, NF, HF)				\
    case OPCODE:						\
	do {							\
	    unsigned res = cpu->regs.a OP imm8;			\
	    int h = ((uint8_t) (cpu->regs.a%16 OP imm8%16)) > 15;\
	    cpu->regs.a = res;					\
	    cpu->regs.f = res>255? 0x10:0;			\
	    cpu->regs.f|=					\
		HF == 0? 0 :					\
		HF == 1? 0x20 :					\
		HF =='h'?					\
		  h? 0x20 : 0					\
		: 0xff;						\
	    cpu->regs.f|= h ? 0x20:0;				\
	    cpu->regs.f|= NF? 0x40:0;				\
	    cpu->regs.f|= cpu->regs.a==0? 0x80:0;			\
	    cpu->regs.pc += 2;					\
	    return 0;						\
	} while(0)

    OP_A_D8(0xc6,+,0,'h');
    OP_A_D8(0xce,+cf+,0,'h');
    OP_A_D8(0xd6,-,1,'h');
    OP_A_D8(0xde,-cf-,1,'h');
    OP_A_D8(0xe6,&,0,1);
    OP_A_D8(0xee,^,0,0);
    OP_A_D8(0xf6,|,0,0);
    case 0xfe: {
	unsigned res = cpu->regs.a - imm8;
	cpu->regs.f = res>255 ? 0x10:0;
	cpu->regs.f|= cpu->regs.a%16 < imm8%16 ? 0x20:0;
	cpu->regs.f|= 0x40;
	cpu->regs.f|= res%256 == 0 ? 0x80:0;
	cpu->regs.pc+=2;
	return 0;
    }
#define RST(VECTOR)						\
    case 0xc7+VECTOR:					        \
	cpu->regs.pc++;						\
	cpu->mem[--cpu->regs.sp] = cpu->regs.pc >> 8;		\
	cpu->mem[--cpu->regs.sp] = cpu->regs.pc;			\
	cpu->regs.pc = VECTOR;					\
	return 0
    RST(0x00); RST(0x08);
    RST(0x10); RST(0x18);
    RST(0x20); RST(0x28);
    RST(0x30); RST(0x38);

    case 0xe8: { // ADD SP, r8
	unsigned bottom_8_bits = (cpu->regs.sp & 0xff) + imm8;
	cpu->regs.f = bottom_8_bits > 255 ? 0x10:0;
	cpu->regs.f|= (bottom_8_bits&0xf) < (cpu->regs.sp&0xf) ? 0x20:0;

	cpu->regs.sp += (int8_t) imm8;
	cpu->regs.pc += 2;
	return 0;
    }
    case 0xf8: { // LD HL, SP + r8
	unsigned bottom_8_bits = (cpu->regs.sp & 0xff) + imm8;
	cpu->regs.f = bottom_8_bits > 255 ? 0x10:0;
	cpu->regs.f|= (bottom_8_bits&0xf) < (cpu->regs.sp&0xf) ? 0x20:0;

	cpu->regs.hl = cpu->regs.sp + (int8_t) imm8;
	cpu->regs.pc += 2;
	return 0;
    }
    COND_RET(0xc9, 1);
    COND_RET(0xd9, 1);//reti == ret because we dont do interrupts anyways
    case 0xe9:
	cpu->regs.pc = cpu->regs.hl;
	return 0;
    case 0xf9:
	cpu->regs.sp = cpu->regs.hl;
	cpu->regs.pc++;
	return 0;

    case 0xea: // ld (a16), a
	cpu->mem[imm16] = cpu->regs.a;
	cpu->regs.pc+=3;
	return 0;
    case 0xfa:
        cpu->regs.a = cpu->mem[imm16];
	cpu->regs.pc+=3;
	return 0;

    case 0xcb: // prefix
	return run_single_prefix_command(cpu);
    case 0xfb: //EI; no interrupts -> noop
	cpu->regs.pc++;
	return 0;
    }


    fprintf(stderr, "unhandled opcode: %02x\n", instr[0]);
    return 1;
}

#define APPLY_XX_TO_ALL_REGS(OFFSET,XX,F,VAL)	\
    XX(OFFSET+0, cpu->regs.b,F,VAL);		\
    XX(OFFSET+1, cpu->regs.c,F,VAL);		\
    XX(OFFSET+2, cpu->regs.d,F,VAL);		\
    XX(OFFSET+3, cpu->regs.e,F,VAL);		\
    XX(OFFSET+4, cpu->regs.h,F,VAL);		\
    XX(OFFSET+5, cpu->regs.l,F,VAL);		\
    XX(OFFSET+6,      (*hl_),F,VAL);		\
    XX(OFFSET+7, cpu->regs.a,F,VAL)

static inline bool run_single_prefix_command(struct SM83* cpu) {
    uint8_t* instr = &cpu->mem[cpu->regs.pc];
    assert(instr[0] == 0xcb || (instr[1]|0x18) == 0x1f);
    uint8_t* hl_ = &cpu->mem[cpu->regs.hl];

	int op = instr[1] & 7;
	int n = (instr[1] >> 3) & 7;

	uint8_t* reg;
	switch(op) {
	case 0: reg = &cpu->regs.b; break;
	case 1: reg = &cpu->regs.c; break;
	case 2: reg = &cpu->regs.d; break;
	case 3: reg = &cpu->regs.e; break;
	case 4: reg = &cpu->regs.h; break;
	case 5: reg = &cpu->regs.l; break;
	case 6: reg = &     (*hl_); break;
	case 7: reg = &cpu->regs.a; break;
	default: __builtin_unreachable();
	}

	int block = instr[1] >> 6;
	if (block) {
		int bit = 1 << n;
		if (block == 3) {        // SET instructions
			*reg |= bit;
		} else if (block == 2) { // RES instructions
			*reg &= ~bit;
		} else if (block == 1) { // BIT instructions
			cpu->regs.f&= 0x10;
			cpu->regs.f|= 0x20|((*reg & bit)?0:0x80);
		}
	} else {         // one of the 8 "tiny instructions"
		int opc = n; // more meaningful name in this case
		uint8_t bit7_cf = *reg & 0x80 ? 0x10 : 0;
		uint8_t bit0_cf = *reg & 1 ? 0x10 : 0;
		uint8_t old_f = cpu->regs.f;
		switch(opc) {
		case 0: // rlc
			*reg = (*reg << 1) | (*reg >> 7);
			break;
		case 1: // rrc
			*reg = (*reg >> 1) | (*reg << 7);
			break;
		case 2: // rl
			*reg = (*reg << 1) | (old_f & 0x10? 1 : 0);
			break;
		case 3: // rr
			*reg = (*reg >> 1) | (old_f & 0x10? 0x80 : 0);
			break;
		case 4: // sla
			*reg = *reg << 1;
			break;
		case 5: // sra
			*reg = (*reg >> 1) | (*reg & 0x80);
			break;
		case 6: // swap
			*reg = (*reg >> 4) | (*reg << 4);
			break;
		case 7: // srl
			*reg = *reg >> 1;
			break;
		}
		if (opc == 0 || opc == 2 || opc == 4) // rlc rl sla
			cpu->regs.f = bit7_cf;
		else if (opc == 6) // swap
			cpu->regs.f = 0;
		else // rrc rr sra srl
			cpu->regs.f = bit0_cf;
		cpu->regs.f |= *reg == 0? 0x80: 0x00;
	}
    cpu->regs.pc += 2;
    return 0;
}

void run_cpu(struct SM83* cpu) {
    bool halting;
    uint16_t prev_pc;
    do {
	prev_pc = cpu->regs.pc;
	halting = run_single_command(cpu);
    } while (!halting && prev_pc != cpu->regs.pc) ;
}

void print_regs(const struct SM83* cpu) {
    printf("af: %04x\nbc: %04x\nde: %04x\nhl: %04x\nsp: %04x\npc: %04x\n"
	   "z: %x, n: %x, h: %x, c: %x\n",
	   cpu->regs.af,
	   cpu->regs.bc,
	   cpu->regs.de,
	   cpu->regs.hl,
	   cpu->regs.sp,
	   cpu->regs.pc,
	   cpu->regs.zf, cpu->regs.nf, cpu->regs.hf, cpu->regs.cf);
}

void load_rom(struct SM83* cpu, const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
	fprintf(stderr, "can't open file `%s`\n", filename);
	perror("");
	return;
    }

    size_t bytes_read = fread(cpu->mem, 1, sizeof(cpu->mem), f);
    printf("read %zd bytes\n", bytes_read);

    fclose(f);
}

void clean_cpu(struct SM83* cpu) {
    memset(&cpu->mem, 0x00, sizeof(cpu->mem));
    cpu->regs.pc = 0x0100;
    // stuff set up by the bootrom
    cpu->regs.af = 0x01B0;
    cpu->regs.bc = 0x0013;
    cpu->regs.de = 0x00d8;
    cpu->regs.hl = 0x014d;
    cpu->regs.sp = 0xfffe;
    // setup constant [rLY] = 144 = 0x90 - for tests
    cpu->mem[0xff44] = 0x90;
    // start non halted and without breakpoints
    cpu->misc.halt = false;
}
