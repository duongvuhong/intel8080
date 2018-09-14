#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CC_PRINT /* for CC_* macro */
#include "common.h"
#define LOW_ENDIAN /* Intel family is low-endian */
#include "intel_8080.h"
#include "emulator.h"

#define S_BIT  0x80
#define Z_BIT  0x40
#define AC_BIT 0x10
#define P_BIT  0x04
#define CY_BIT 0x01

#define __i8080_memory_read_word(addr) \
	(((uint16_t)i8080_memory_read((addr) + 1) << 8) | i8080_memory_read(addr))
#define __i8080_memory_write_word(addr, word) do { \
	i8080_memory_write(addr, (word) & 0xff); \
	i8080_memory_write((addr) + 1, ((word) >> 8) & 0xff); \
} while (0)

static const uint8_t i8080_instruction_size[256] = {
/*  x0 x1 x2 x3 x4 x5 x6 x7 x8 x9 xA xB xC xD xE xF */
	1, 3, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1, /* 0x */
	1, 3, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1, /* 1x */
	1, 3, 3, 1, 1, 1, 2, 1, 1, 1, 3, 1, 1, 1, 2, 1, /* 2x */
	1, 3, 3, 1, 1, 1, 2, 1, 1, 1, 3, 1, 1, 1, 2, 1, /* 3x */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 4x */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 5x */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 6x */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 7x */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 8x */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 9x */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* Ax */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* Bx */
	1, 1, 3, 0, 3, 1, 2, 0, 1, 1, 3, 0, 3, 0, 2, 0, /* Cx */
	1, 1, 3, 2, 3, 1, 2, 0, 1, 1, 3, 2, 3, 0, 2, 0, /* Dx */
	1, 1, 3, 1, 3, 1, 2, 0, 1, 1, 3, 1, 3, 0, 2, 0, /* Ex */
	1, 1, 3, 1, 3, 1, 2, 0, 1, 1, 3, 1, 3, 0, 2, 0, /* Fx */
};

static const uint8_t i8080_instruction_cycle[256] = {
	4, 10,  7,  5,  5,  5,  7,  4,  4, 10,  7,  5,  5,  5,  7,  4,
	4, 10,  7,  5,  5,  5,  7,  4,  4, 10,  7,  5,  5,  5,  7,  4,
	4, 10, 16,  5,  5,  5,  7,  4,  4, 10, 16,  5,  5,  5,  7,  4,
	4, 10, 13,  5, 10, 10, 10,  4,  4, 10, 13,  5,  5,  5,  7,  4,
	5,  5,  5,  5,  5,  5,  7,  5,  5,  5,  5,  5,  5,  5,  7,  5,
	5,  5,  5,  5,  5,  5,  7,  5,  5,  5,  5,  5,  5,  5,  7,  5,
	5,  5,  5,  5,  5,  5,  7,  5,  5,  5,  5,  5,  5,  5,  7,  5,
	7,  7,  7,  7,  7,  7,  7,  7,  5,  5,  5,  5,  5,  5,  7,  5,
	4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
	4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
	4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
	4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
	5, 10, 10, 10, 11, 11,  7, 11,  5, 10, 10, 10, 11, 17,  7, 11,
	5, 10, 10, 10, 11, 11,  7, 11,  5, 10, 10, 10, 11, 17,  7, 11,
	5, 10, 10, 18, 11, 11,  7, 11,  5,  5, 10,  5, 11, 17,  7, 11,
	5, 10, 10,  4, 11, 11,  7, 11,  5,  5, 10,  4, 11, 17,  7, 11,
};

static const uint8_t parity[256] = {
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
};

static const uint8_t half_carry[8] = {
	0, 0, 1, 0, 1, 0, 1, 1,
};

static const uint8_t sub_half_carry[8] = {
	0, 1, 1, 1, 0, 0, 0, 1,
};

void i8080_reset(struct intel_8080 *i8080)
{
	i8080->sp = 0;      /* reset stack pointer */
	i8080->pc = 0xF800; /* reset program counter */

	/* reset registers */
	memset(i8080->regs, 0, I8080_PAIR_REGISTERS * sizeof(pair_reg));
	i8080->regs[PSW_REG_INDEX].byte.h |= 0x02; /* bit 1: 0 - bit 3: 0 - bit 5: 0 */
}

#define I8080_LXI(reg, addr)  (reg = __i8080_memory_read_word(addr))
#define I8080_STAX(addr, val) (i8080_memory_write(addr, val))
#define I8080_MVI(reg, addr)  (reg = i8080_memory_read(addr))
#define I8080_INX(reg)        ((reg)++)
#define I8080_INR(reg, flag) do {                                           \
	(reg)++;                                                                \
	/* sign flag */                                                         \
	(flag) = ((flag) & ~S_BIT) | ((reg) & 0x80 ? S_BIT : 0x00);             \
	/* zero flag */                                                         \
	(flag) = ((flag) & ~Z_BIT) | (!(reg) ? Z_BIT : 0x00);                   \
	/* AC flag */                                                           \
	(flag) = ((flag) & ~AC_BIT) | (!((reg) & 0x0f) ? AC_BIT : 0x00);        \
	/* parity flag */                                                       \
	(flag) = ((flag) & ~P_BIT) | (parity[(reg)] ? P_BIT : 0x00);            \
} while (0)
#define I8080_DCR(reg, flag) do {                                           \
	(reg)--;                                                                \
	/* sign flag */                                                         \
	(flag) = ((flag) & ~S_BIT) | ((reg) & 0x80 ? S_BIT : 0x00);             \
	/* zero flag */                                                         \
	(flag) = ((flag) & ~Z_BIT) | (!(reg) ? Z_BIT : 0x00);                   \
	/* AC flag */                                                           \
	(flag) = ((flag) & ~AC_BIT) | (((reg) & 0x0f) != 0x0f ? AC_BIT : 0x00); \
	/* parity flag */                                                       \
	(flag) = ((flag) & ~P_BIT) | (parity[(reg)] ? P_BIT : 0x00);            \
} while (0)
#define I8080_DAD(hl, reg, flag) do {                                       \
	uint32_t tmp = (uint32_t)(hl) + (reg);                                  \
	hl = tmp & 0xffff;                                                      \
	flag = (flag & ~1) | (tmp >> 16);                                       \
} while (0)
#define I8080_LDAX(reg, addr) (reg = i8080_memory_read(addr))
#define I8080_DCX(reg)        ((reg)--)
#define I8080_ADD(reg, val, flag) do {                                      \
	uint16_t tmp = (uint16_t)(reg) + (val);                                 \
	int index = (((reg) & 0x88) >> 1) | (((val) & 0x88) >> 2)               \
				| ((tmp & 0x88) >> 3);                                      \
	reg = tmp & 0xff;                                                       \
	/* sign flag */                                                         \
	flag = ((flag) & ~S_BIT) | ((reg) & 0x80 ? S_BIT : 0x00);               \
	/* zero flag */                                                         \
	flag = ((flag) & ~Z_BIT) | (!(reg) ? Z_BIT : 0x00);                     \
	/* AC flag */                                                           \
	flag = ((flag) & ~AC_BIT) | half_carry[index & 0x7] ? AC_BIT : 0x00;    \
	/* parity flag */                                                       \
	flag = ((flag) & ~P_BIT) | parity[(reg)] ? P_BIT : 0x00;                \
	/* carry flag */                                                        \
	flag = ((flag) & ~CY_BIT) | ((tmp & 0x0100) ? CY_BIT : 0x00);           \
} while(0)
#define I8080_ADC(reg, val, flag) do {                                      \
	uint16_t tmp = (uint16_t)(reg) + (val) + ((flag) & CY_BIT);             \
	int index = (((reg) & 0x88) >> 1) | (((val) & 0x88) >> 2)               \
				| ((tmp & 0x88) >> 3);                                      \
	reg = tmp & 0xff;                                                       \
	/* sign flag */                                                         \
	flag = ((flag) & ~S_BIT) | ((reg) & 0x80 ? S_BIT : 0x00);               \
	/* zero flag */                                                         \
	flag = ((flag) & ~Z_BIT) | (!(reg) ? Z_BIT : 0x00);                     \
	/* AC flag */                                                           \
	flag = ((flag) & ~AC_BIT) | half_carry[index & 0x7] ? AC_BIT : 0x00;    \
	/* parity flag */                                                       \
	flag = ((flag) & ~P_BIT) | parity[(reg)] ? P_BIT : 0x00;                \
	/* carry flag */                                                        \
	flag = ((flag) & ~CY_BIT) | ((tmp & 0x0100) ? CY_BIT : 0x00);           \
} while(0)
#define I8080_SUB(reg, val, flag) do {                                      \
	uint16_t tmp = (uint16_t)(reg) - (val);                                 \
	int index = (((reg) & 0x88) >> 1) | (((val) & 0x88) >> 2)               \
				| ((tmp & 0x88) >> 3);                                      \
	reg = tmp & 0xff;                                                       \
	/* sign flag */                                                         \
	flag = ((flag) & ~S_BIT) | ((reg) & 0x80 ? S_BIT : 0x00);               \
	/* zero flag */                                                         \
	flag = ((flag) & ~Z_BIT) | (!(reg) ? Z_BIT : 0x00);                     \
	/* AC flag */                                                           \
	flag = ((flag) & ~AC_BIT)                                               \
			| !sub_half_carry[index & 0x7] ? AC_BIT : 0x00;                 \
	/* parity flag */                                                       \
	flag = ((flag) & ~P_BIT) | parity[(reg)] ? P_BIT : 0x00;                \
	/* carry flag */                                                        \
	flag = ((flag) & ~CY_BIT) | ((tmp & 0x0100) ? CY_BIT : 0x00);           \
} while(0)
#define I8080_SBB(reg, val, flag) do {                                      \
	uint16_t tmp = (uint16_t)(reg) - (val) - ((flag) & CY_BIT);             \
	int index = (((reg) & 0x88) >> 1) | (((val) & 0x88) >> 2)               \
				| ((tmp & 0x88) >> 3);                                      \
	reg = tmp & 0xff;                                                       \
	/* sign flag */                                                         \
	flag = ((flag) & ~S_BIT) | ((reg) & 0x80 ? S_BIT : 0x00);               \
	/* zero flag */                                                         \
	flag = ((flag) & ~Z_BIT) | (!(reg) ? Z_BIT : 0x00);                     \
	/* AC flag */                                                           \
	flag = ((flag) & ~AC_BIT)                                               \
			| !sub_half_carry[index & 0x7] ? AC_BIT : 0x00;                 \
	/* parity flag */                                                       \
	flag = ((flag) & ~P_BIT) | parity[(reg)] ? P_BIT : 0x00;                \
	/* carry flag */                                                        \
	flag = ((flag) & ~CY_BIT) | ((tmp & 0x0100) ? CY_BIT : 0x00);           \
} while(0)
#define I8080_ANA(reg, val, flag) do {                                      \
	/* AC flag */                                                           \
	flag = ((flag) & AC_BIT) | (((reg) | val) & 0x08 ? AC_BIT : 0x00);      \
	(reg) &= (val);                                                         \
	/* sign flag */                                                         \
	flag = ((flag) & ~S_BIT) | ((reg) & 0x80 ? S_BIT : 0x00);               \
	/* zero flag */                                                         \
	flag = ((flag) & ~Z_BIT) | (!(reg) ? Z_BIT : 0x00);                     \
	/* parity flag */                                                       \
	flag = ((flag) & ~P_BIT) | parity[(reg)] ? P_BIT : 0x00;                \
	/* clear carry flag */                                                  \
	flag = ((flag) & ~CY_BIT);                                              \
} while(0)
#define I8080_XRA(reg, val, flag) do {                                      \
	(reg) ^= (val);                                                         \
	/* sign flag */                                                         \
	flag = ((flag) & ~S_BIT) | ((reg) & 0x80 ? S_BIT : 0x00);               \
	/* zero flag */                                                         \
	flag = ((flag) & ~Z_BIT) | (!(reg) ? Z_BIT : 0x00);                     \
	/* clear AC flag */                                                     \
	flag = ((flag) & AC_BIT) | (((reg) | val) & 0x08 ? AC_BIT : 0x00);      \
	/* parity flag */                                                       \
	flag = ((flag) & ~P_BIT) | parity[(reg)] ? P_BIT : 0x00;                \
	/* clear carry flag */                                                  \
	flag = ((flag) & ~CY_BIT);                                              \
} while(0)
#define I8080_ORA(reg, val, flag) do {                                      \
	(reg) |= (val);                                                         \
	/* sign flag */                                                         \
	flag = ((flag) & ~S_BIT) | ((reg) & 0x80 ? S_BIT : 0x00);               \
	/* zero flag */                                                         \
	flag = ((flag) & ~Z_BIT) | (!(reg) ? Z_BIT : 0x00);                     \
	/* clear AC flag */                                                     \
	flag = ((flag) & AC_BIT) | (((reg) | val) & 0x08 ? AC_BIT : 0x00);      \
	/* parity flag */                                                       \
	flag = ((flag) & ~P_BIT) | parity[(reg)] ? P_BIT : 0x00;                \
	/* clear carry flag */                                                  \
	flag = ((flag) & ~CY_BIT);                                              \
} while(0)
#define I8080_CMP(reg, val, flag) do {                                      \
	uint16_t tmp = (uint16_t)(reg) - (val);                                 \
	int index = (((reg) & 0x88) >> 1) | (((val) & 0x88) >> 2)               \
				| ((tmp & 0x88) >> 3);                                      \
	/* sign flag */                                                         \
	flag = ((flag) & ~S_BIT) | ((tmp) & 0x80 ? S_BIT : 0x00);               \
	/* zero flag */                                                         \
	flag = ((flag) & ~Z_BIT) | (!((tmp) & 0xff) ? Z_BIT : 0x00);            \
	/* AC flag */                                                           \
	flag = ((flag) & ~AC_BIT)                                               \
			| !sub_half_carry[index & 0x7] ? AC_BIT : 0x00;                 \
	/* parity flag */                                                       \
	flag = ((flag) & ~P_BIT) | parity[(index) & 0x100] ? P_BIT : 0x00;      \
	/* carry flag */                                                        \
	flag = ((flag) & ~CY_BIT) | ((tmp & 0x0100) ? CY_BIT : 0x00);           \
} while(0)
#define I8080_POP(sp, reg) do {                                             \
	(reg) = __i8080_memory_read_word(sp);                                   \
	(sp) += 2;                                                              \
} while (0)
#define I8080_PUSH(sp, reg) do {                                            \
	(sp) -= 2;                                                              \
	__i8080_memory_write_word((sp), (reg));                                 \
} while (0)
#define I8080_RST(addr, sp, pc) do {                                        \
	I8080_PUSH((sp), (pc));                                                 \
	(pc) = (addr);                                                          \
} while (0)
#define I8080_RET(sp, pc) I8080_POP((sp), (pc))
#define I8080_CALL(sp, pc) do {                                             \
	I8080_PUSH((sp), (pc));                                                 \
	(pc) = __i8080_memory_read_word((pc));                                  \
} while (0)

int i8080_execute(register struct intel_8080 *i8080, register int cycles)
{
	uint8_t opcode;
	pair_reg *regs = i8080->regs;
	uint16_t *sp = &i8080->sp;
	uint16_t *pc = &i8080->pc;

#define B_REG    (regs[BC_REG_INDEX].byte.h)
#define C_REG    (regs[BC_REG_INDEX].byte.l)
#define BC_REG   (regs[BC_REG_INDEX].word)
#define D_REG    (regs[DE_REG_INDEX].byte.h)
#define E_REG    (regs[DE_REG_INDEX].byte.l)
#define DE_REG   (regs[DE_REG_INDEX].word)
#define H_REG    (regs[HL_REG_INDEX].byte.h)
#define L_REG    (regs[HL_REG_INDEX].byte.l)
#define HL_REG   (regs[HL_REG_INDEX].word)
#define A_REG    (regs[PSW_REG_INDEX].byte.h)
#define FLAG_REG (regs[PSW_REG_INDEX].byte.l)
#define PSW_REG  (regs[PSW_REG_INDEX].word)

	while (cycles > 0) {
		opcode = i8080_memory_read(*pc);
		switch (opcode) {
		case 0x00: /* NOP */
		case 0x08:
		case 0x10:
		case 0x18:
		case 0x20:
		case 0x28:
		case 0x30:
		case 0x38:
			break;
		case 0x01: /* LXI B, d16 */
			I8080_LXI(BC_REG, *pc + 1);
			break;
		case 0x02: /* STAX B */
			I8080_STAX(BC_REG, A_REG);
			break;
		case 0x03: /* INX B */
			I8080_INX(BC_REG);
			break;
		case 0x04: /* INR B */
			I8080_INR(B_REG, FLAG_REG);
			break;
		case 0x05: /* DCR B */
			I8080_DCR(B_REG, FLAG_REG);
			break;
		case 0x06: /* MVI B, d8 */
			I8080_MVI(B_REG, *pc + 1);
			break;
		case 0x07: /* RLC */
			FLAG_REG = (FLAG_REG & ~CY_BIT) | (A_REG >> 7);
			A_REG = (A_REG << 1) | (A_REG >> 7);
			break;
		case 0x09: /* DAD B */
			I8080_DAD(HL_REG, BC_REG, FLAG_REG);
			break;
		case 0x0A: /* LDAX B */
			I8080_LDAX(A_REG, BC_REG);
			break;
		case 0x0B: /* DCX B */
			I8080_DCX(BC_REG);
			break;
		case 0x0C: /* INR C */
			I8080_INR(C_REG, FLAG_REG);
			break;
		case 0x0D: /* DCR C */
			I8080_DCR(C_REG, FLAG_REG);
			break;
			break;
		case 0x0E: /* MVI C, d8 */
			I8080_MVI(C_REG, *pc + 1);
			break;
		case 0x0F: /* RRC */
			FLAG_REG = (FLAG_REG & ~CY_BIT) | (A_REG & 0x01);
			A_REG = (A_REG >> 1) | (A_REG << 7);
			break;
		case 0x11: /* LXI D, d16 */
			I8080_LXI(DE_REG, *pc + 1);
			break;
		case 0x12: /* STAX D */
			I8080_STAX(DE_REG, A_REG);
			break;
		case 0x13: /* INX D */
			I8080_INX(DE_REG);
			break;
		case 0x14: /* INR D */
			I8080_INR(D_REG, FLAG_REG);
			break;
		case 0x15: /* DCR D */
			I8080_DCR(D_REG, FLAG_REG);
			break;
		case 0x16: /* MVI D, d8 */
			I8080_MVI(D_REG, *pc + 1);
			break;
		case 0x17: /* RAL */
		{
			uint8_t b = A_REG >> 7;
			A_REG = (A_REG << 1) | (FLAG_REG & CY_BIT);
			FLAG_REG = (FLAG_REG & ~CY_BIT) | b;
		}
			break;
		case 0x19: /* DAD D */
			I8080_DAD(HL_REG, DE_REG, FLAG_REG);
			break;
		case 0x1A: /* LDAX D */
			I8080_LDAX(A_REG, DE_REG);
			break;
		case 0x1B: /* DCX D */
			I8080_DCX(DE_REG);
			break;
		case 0x1C: /* INR E */
			I8080_INR(E_REG, FLAG_REG);
			break;
		case 0x1D: /* DCR E */
			I8080_DCR(E_REG, FLAG_REG);
			break;
		case 0x1E: /* MVI E, d8 */
			I8080_MVI(E_REG, *pc + 1);
			break;
		case 0x1F: /* RAR */
			FLAG_REG = (FLAG_REG & ~CY_BIT) | (A_REG & 0x01);
			A_REG = (A_REG >> 1) | (A_REG & 0x80);
			break;
		case 0x21: /* LXI H, d16 */
			I8080_LXI(HL_REG, *pc);
			break;
		case 0x22: /* SHLD a16 */
			__i8080_memory_write_word(*pc + 1, HL_REG);
			break;
		case 0x23: /* INX H */
			I8080_INX(HL_REG);
			break;
		case 0x24: /* INR H */
			I8080_INR(H_REG, FLAG_REG);
			break;
		case 0x25: /* DCR H */
			I8080_DCR(H_REG, FLAG_REG);
			break;
		case 0x26: /* MVI H, d8 */
			I8080_MVI(L_REG, *pc + 1);
			break;
		case 0x27: /* DAA */
			/* todo */
			break;
		case 0x29: /* DAD H */
			I8080_DAD(HL_REG, HL_REG, FLAG_REG);
			break;
		case 0x2A: /* LHLD a16 */
			HL_REG = __i8080_memory_read_word(*pc + 1);
			break;
		case 0x2B: /* DCX H */
			I8080_DCX(HL_REG);
			break;
		case 0x2C: /* INR L */
			I8080_INR(L_REG, FLAG_REG);
			break;
		case 0x2D: /* DCR L */
			I8080_DCR(L_REG, FLAG_REG);
			break;
		case 0x2E: /* MVI L, d8 */
			I8080_MVI(L_REG, *pc + 1);
			break;
		case 0x2F: /* CMA */
			A_REG = !A_REG;
			break;
		case 0x31: /* LXI SP, d16 */
			I8080_LXI(*sp, *pc + 1);
			break;
		case 0x32: /* STA a16 */
		{
			uint16_t addr = ((uint16_t)i8080_memory_read(*pc + 1) << 8) | i8080_memory_read(*pc + 2);
			i8080_memory_write(addr, A_REG);
		}
			break;
		case 0x33: /* INX SP */
			I8080_INX(*sp);
			break;
		case 0x34: /* INR M */
		{
			uint8_t reg = i8080_memory_read(HL_REG);
			I8080_INR(reg, FLAG_REG);
			i8080_memory_write(HL_REG, reg);
		}
			break;
		case 0x35: /* DCR M */
		{
			uint8_t reg = i8080_memory_read(HL_REG);
			I8080_DCR(reg, FLAG_REG);
			i8080_memory_write(HL_REG, reg);
		}
			break;
		case 0x36: /* MVI M, d8 */
			i8080_memory_write(HL_REG, i8080_memory_read(*pc + 1));
			break;
		case 0x37: /* STC */
			FLAG_REG |= CY_BIT;
			break;
		case 0x39: /* DAD SP */
			I8080_DAD(HL_REG, *sp, FLAG_REG);
			break;
		case 0x3A: /* LDA a16 */
			A_REG = i8080_memory_read(((uint16_t)(*pc + 1) << 8) | (*pc + 2));
			break;
		case 0x3B: /* DCX SP */
			I8080_DCX(*sp);
			break;
		case 0x3C: /* INR A */
			I8080_INR(A_REG, FLAG_REG);
			break;
		case 0x3D: /* DCR A */
			I8080_DCR(A_REG, FLAG_REG);
			break;
		case 0x3E: /* MVI A, d8 */
			I8080_MVI(A_REG, *pc + 1);
			break;
		case 0x3F: /* CMC */
			FLAG_REG |= ~(FLAG_REG & CY_BIT);
		case 0x40: /* MOV B, B */
			break;
		case 0x41: /* MOV B, C */
			B_REG = C_REG;
			break;
		case 0x42: /* MOV B, D */
			B_REG = D_REG;
			break;
		case 0x43: /* MOV B, E */
			B_REG = E_REG;
			break;
		case 0x44: /* MOV B, H */
			B_REG = H_REG;
			break;
		case 0x45: /* MOV B, L */
			B_REG = L_REG;
			break;
		case 0x46: /* MOV B, M */
			B_REG = i8080_memory_read(HL_REG);
			break;
		case 0x47: /* MOV B, A */
			B_REG = A_REG;
			break;
		case 0x48: /* MOV C, B */
			C_REG = B_REG;
			break;
		case 0x49: /* MOV C, C */
			break;
		case 0x4A: /* MOV C, D */
			C_REG = D_REG;
			break;
		case 0x4B: /* MOV C, E */
			C_REG = E_REG;
			break;
		case 0x4C: /* MOV C, H */
			C_REG = H_REG;
			break;
		case 0x4D: /* MOV C, L */
			C_REG = L_REG;
			break;
		case 0x4E: /* MOV C, M */
			C_REG = i8080_memory_read(HL_REG);
			break;
		case 0x4F: /* MOV C, A */
			C_REG = A_REG;
			break;
		case 0x50: /* MOV D, B */
			D_REG = B_REG;
			break;
		case 0x51: /* MOV D, C */
			D_REG = C_REG;
			break;
		case 0x52: /* MOV D, D */
			break;
		case 0x53: /* MOV D, E */
			D_REG = E_REG;
			break;
		case 0x54: /* MOV D, H */
			D_REG = H_REG;
			break;
		case 0x55: /* MOV D, L */
			D_REG = L_REG;
			break;
		case 0x56: /* MOV D, M */
			D_REG = i8080_memory_read(HL_REG);
			break;
		case 0x57: /* MOV D, A */
			D_REG = A_REG;
			break;
		case 0x58: /* MOV E, B */
			E_REG = D_REG;
			break;
		case 0x59: /* MOV E, C */
			E_REG = C_REG;
			break;
		case 0x5A: /* MOV E, D */
			E_REG = D_REG;
			break;
		case 0x5B: /* MOV E, E */
			break;
		case 0x5C: /* MOV E, H */
			E_REG = H_REG;
			break;
		case 0x5D: /* MOV E, L */
			E_REG = L_REG;
			break;
		case 0x5E: /* MOV E, M */
			E_REG = i8080_memory_read(HL_REG);
			break;
		case 0x5F: /* MOV E, A */
			E_REG = A_REG;
			break;
		case 0x60: /* MOV H, B */
			H_REG = B_REG;
			break;
		case 0x61: /* MOV H, C */
			H_REG = C_REG;
			break;
		case 0x62: /* MOV H, D */
			H_REG = D_REG;
			break;
		case 0x63: /* MOV H, E */
			H_REG = E_REG;
			break;
		case 0x64: /* MOV H, H */
			break;
		case 0x65: /* MOV H, L */
			H_REG = L_REG;
			break;
		case 0x66: /* MOV H, M */
			H_REG = i8080_memory_read(HL_REG);
			break;
		case 0x67: /* MOV H, A */
			H_REG = A_REG;
			break;
		case 0x68: /* MOV L, B */
			L_REG = D_REG;
			break;
		case 0x69: /* MOV L, C */
			L_REG = C_REG;
			break;
		case 0x6A: /* MOV L, D */
			L_REG = D_REG;
			break;
		case 0x6B: /* MOV L, E */
			L_REG = E_REG;
			break;
		case 0x6C: /* MOV L, H */
			L_REG = H_REG;
			break;
		case 0x6D: /* MOV L, L */
			break;
		case 0x6E: /* MOV L, M */
			L_REG = i8080_memory_read(HL_REG);
			break;
		case 0x6F: /* MOV L, A */
			L_REG = A_REG;
			break;
		case 0x70: /* MOV M, B */
			i8080_memory_write(HL_REG, B_REG);
			break;
		case 0x71: /* MOV M, C */
			i8080_memory_write(HL_REG, C_REG);
			break;
		case 0x72: /* MOV M, D */
			i8080_memory_write(HL_REG, D_REG);
			break;
		case 0x73: /* MOV M, E */
			i8080_memory_write(HL_REG, E_REG);
			break;
		case 0x74: /* MOV M, H */
			i8080_memory_write(HL_REG, H_REG);
			break;
		case 0x75: /* MOV M, L */
			i8080_memory_write(HL_REG, L_REG);
			break;
		case 0x76: /* HLT */
			break;
		case 0x77: /* MOV M, A */
			i8080_memory_write(HL_REG, A_REG);
			break;
		case 0x78: /* MOV A, B */
			A_REG = D_REG;
			break;
		case 0x79: /* MOV A, C */
			A_REG = C_REG;
			break;
		case 0x7A: /* MOV A, D */
			A_REG = D_REG;
			break;
		case 0x7B: /* MOV A, E */
			A_REG = E_REG;
			break;
		case 0x7C: /* MOV A, H */
			A_REG = H_REG;
			break;
		case 0x7D: /* MOV A, L */
			A_REG = L_REG;
			break;
		case 0x7E: /* MOV A, M */
			A_REG = i8080_memory_read(HL_REG);
			break;
		case 0x7F: /* MOV A, A */
			break;
		case 0x80: /* ADD B */
			I8080_ADD(A_REG, B_REG, FLAG_REG);
			break;
		case 0x81: /* ADD C */
			I8080_ADD(A_REG, C_REG, FLAG_REG);
			break;
		case 0x82: /* ADD D */
			I8080_ADD(A_REG, D_REG, FLAG_REG);
			break;
		case 0x83: /* ADD E */
			I8080_ADD(A_REG, E_REG, FLAG_REG);
			break;
		case 0x84: /* ADD H */
			I8080_ADD(A_REG, H_REG, FLAG_REG);
			break;
		case 0x85: /* ADD L */
			I8080_ADD(A_REG, L_REG, FLAG_REG);
			break;
		case 0x86: /* ADD M */
		{
			uint8_t val = i8080_memory_read(HL_REG);
			I8080_ADD(A_REG, val, FLAG_REG);
		}
			break;
		case 0x87: /* ADD A */
			I8080_ADD(A_REG, A_REG, FLAG_REG);
			break;
		case 0x88: /* ADC B */
			I8080_ADC(A_REG, D_REG, FLAG_REG);
			break;
		case 0x89: /* ADC C */
			I8080_ADC(A_REG, C_REG, FLAG_REG);
			break;
		case 0x8A: /* ADC D */
			I8080_ADC(A_REG, D_REG, FLAG_REG);
			break;
		case 0x8B: /* ADC E */
			I8080_ADC(A_REG, E_REG, FLAG_REG);
			break;
		case 0x8C: /* ADC H */
			I8080_ADC(A_REG, H_REG, FLAG_REG);
			break;
		case 0x8D: /* ADC L */
			I8080_ADC(A_REG, L_REG, FLAG_REG);
			break;
		case 0x8E: /* ADC M */
		{
			uint8_t val = i8080_memory_read(HL_REG);
			I8080_ADC(A_REG, val, FLAG_REG);
		}
			break;
		case 0x8F: /* ADC A */
			I8080_ADC(A_REG, A_REG, FLAG_REG);
			break;
		case 0x90: /* SUB B */
			I8080_SUB(A_REG, B_REG, FLAG_REG);
			break;
		case 0x91: /* SUB C */
			I8080_SUB(A_REG, C_REG, FLAG_REG);
			break;
		case 0x92: /* SUB D */
			I8080_SUB(A_REG, D_REG, FLAG_REG);
			break;
		case 0x93: /* SUB E */
			I8080_SUB(A_REG, E_REG, FLAG_REG);
			break;
		case 0x94: /* SUB H */
			I8080_SUB(A_REG, H_REG, FLAG_REG);
			break;
		case 0x95: /* SUB L */
			I8080_SUB(A_REG, L_REG, FLAG_REG);
			break;
		case 0x96: /* SUB M */
		{
			uint8_t val = i8080_memory_read(HL_REG);
			I8080_SUB(A_REG, val, FLAG_REG);
		}
			break;
		case 0x97: /* SUB A */
			I8080_SUB(A_REG, A_REG, FLAG_REG);
			break;
		case 0x98: /* SBB B */
			I8080_SBB(A_REG, D_REG, FLAG_REG);
			break;
		case 0x99: /* SBB C */
			I8080_SBB(A_REG, C_REG, FLAG_REG);
			break;
		case 0x9A: /* SBB D */
			I8080_SBB(A_REG, D_REG, FLAG_REG);
			break;
		case 0x9B: /* SBB E */
			I8080_SBB(A_REG, E_REG, FLAG_REG);
			break;
		case 0x9C: /* SBB H */
			I8080_SBB(A_REG, H_REG, FLAG_REG);
			break;
		case 0x9D: /* SBB L */
			I8080_SBB(A_REG, L_REG, FLAG_REG);
			break;
		case 0x9E: /* SBB M */
		{
			uint8_t val = i8080_memory_read(HL_REG);
			I8080_SBB(A_REG, val, FLAG_REG);
		}
			break;
		case 0x9F: /* SBB A */
			I8080_SBB(A_REG, A_REG, FLAG_REG);
			break;
		case 0xA0: /* ANA B */
			I8080_ANA(A_REG, B_REG, FLAG_REG);
			break;
		case 0xA1: /* ANA C */
			I8080_ANA(A_REG, C_REG, FLAG_REG);
			break;
		case 0xA2: /* ANA D */
			I8080_ANA(A_REG, D_REG, FLAG_REG);
			break;
		case 0xA3: /* ANA E */
			I8080_ANA(A_REG, E_REG, FLAG_REG);
			break;
		case 0xA4: /* ANA H */
			I8080_ANA(A_REG, H_REG, FLAG_REG);
			break;
		case 0xA5: /* ANA L */
			I8080_ANA(A_REG, L_REG, FLAG_REG);
			break;
		case 0xA6: /* ANA M */
		{
			uint8_t val = i8080_memory_read(HL_REG);
			I8080_ANA(A_REG, val, FLAG_REG);
		}
			break;
		case 0xA7: /* ANA A */
			I8080_ANA(A_REG, A_REG, FLAG_REG);
			break;
		case 0xA8: /* XRA B */
			I8080_XRA(A_REG, D_REG, FLAG_REG);
			break;
		case 0xA9: /* XRA C */
			I8080_XRA(A_REG, C_REG, FLAG_REG);
			break;
		case 0xAA: /* XRA D */
			I8080_XRA(A_REG, D_REG, FLAG_REG);
			break;
		case 0xAB: /* XRA E */
			I8080_XRA(A_REG, E_REG, FLAG_REG);
			break;
		case 0xAC: /* XRA H */
			I8080_XRA(A_REG, H_REG, FLAG_REG);
			break;
		case 0xAD: /* XRA L */
			I8080_XRA(A_REG, L_REG, FLAG_REG);
			break;
		case 0xAE: /* XRA M */
		{
			uint8_t val = i8080_memory_read(HL_REG);
			I8080_XRA(A_REG, val, FLAG_REG);
		}
			break;
		case 0xAF: /* XRA A */
			I8080_XRA(A_REG, A_REG, FLAG_REG);
			break;
		case 0xB0: /* ORA B */
			I8080_ORA(A_REG, B_REG, FLAG_REG);
			break;
		case 0xB1: /* ORA C */
			I8080_ORA(A_REG, C_REG, FLAG_REG);
			break;
		case 0xB2: /* ORA D */
			I8080_ORA(A_REG, D_REG, FLAG_REG);
			break;
		case 0xB3: /* ORA E */
			I8080_ORA(A_REG, E_REG, FLAG_REG);
			break;
		case 0xB4: /* ORA H */
			I8080_ORA(A_REG, H_REG, FLAG_REG);
			break;
		case 0xB5: /* ORA L */
			I8080_ORA(A_REG, L_REG, FLAG_REG);
			break;
		case 0xB6: /* ORA M */
		{
			uint8_t val = i8080_memory_read(HL_REG);
			I8080_ORA(A_REG, val, FLAG_REG);
		}
			break;
		case 0xB7: /* ORA A */
			I8080_ORA(A_REG, A_REG, FLAG_REG);
			break;
		case 0xB8: /* CMP B */
			I8080_CMP(A_REG, D_REG, FLAG_REG);
			break;
		case 0xB9: /* CMP C */
			I8080_CMP(A_REG, C_REG, FLAG_REG);
			break;
		case 0xBA: /* CMP D */
			I8080_CMP(A_REG, D_REG, FLAG_REG);
			break;
		case 0xBB: /* CMP E */
			I8080_CMP(A_REG, E_REG, FLAG_REG);
			break;
		case 0xBC: /* CMP H */
			I8080_CMP(A_REG, H_REG, FLAG_REG);
			break;
		case 0xBD: /* CMP L */
			I8080_CMP(A_REG, L_REG, FLAG_REG);
			break;
		case 0xBE: /* CMP M */
		{
			uint8_t val = i8080_memory_read(HL_REG);
			I8080_CMP(A_REG, val, FLAG_REG);
		}
			break;
		case 0xBF: /* CMP A */
			I8080_CMP(A_REG, A_REG, FLAG_REG);
			break;
		case 0xC0: /* RNZ */
			if (!(FLAG_REG & Z_BIT)) {
				I8080_RET(*sp, *pc);
				cycles -= 6;
				goto dcr_cycle;
			}
			break;
		case 0xC1: /* POP B */
			I8080_POP(*sp, BC_REG);
			break;
		case 0xC2: /* JNZ a16 */
			if (!(FLAG_REG & Z_BIT)) {
				*pc = __i8080_memory_read_word(*pc);
				goto dcr_cycle;
			}
			break;
		case 0xC3: /* JMP a16 */
		case 0xCB:
			*pc = __i8080_memory_read_word(*pc);
			goto dcr_cycle;
		case 0xC4: /* CNZ a16 */
			if (!(FLAG_REG & Z_BIT)) {
				I8080_CALL(*sp, *pc);
				cycles -= 6;
				goto dcr_cycle;
			}
			break;
		case 0xC5: /* PUSH B */
			I8080_PUSH(*sp, BC_REG);
			break;
		case 0xC6: /* ADI d8 */
		{
			uint8_t b = i8080_memory_read(*pc);
			I8080_ADD(A_REG, b, FLAG_REG);
		}
			break;
		case 0xC7: /* RST 0 */
			I8080_RST(0x0000, *sp, *pc);
			break;
		case 0xC8: /* RZ */
			if (FLAG_REG & Z_BIT) {
				I8080_POP(*sp, *pc);
				cycles -= 6;
				goto dcr_cycle;
			}
			break;
		case 0xC9: /* RET */
		case 0xD9:
			I8080_RET(*sp, *pc);
			goto dcr_cycle;
			break;
		case 0xCA: /* JZ a16 */
			if (FLAG_REG & Z_BIT) {
				*pc = __i8080_memory_read_word(*pc);
				goto dcr_cycle;
			}
			break;
		case 0xCC: /* CZ a16 */
			if (FLAG_REG & Z_BIT) {
				I8080_CALL(*sp, *pc);
				cycles -= 6;
				goto dcr_cycle;
			}
			break;
		case 0xCD: /* CALL a16 */
		case 0xDD:
		case 0xED:
		case 0xFD:
			I8080_CALL(*sp, *pc);
			break;
		case 0xCE: /* ACI d8 */
		{
			uint8_t b = i8080_memory_read(*pc) + (FLAG_REG & CY_BIT);
			I8080_ADD(A_REG, b, FLAG_REG);
		}
			break;
		case 0xCF: /* RST 1 */
			I8080_RST(0x0008, *sp, *pc);
			break;
		case 0xD0: /* RNC */
			if (!(FLAG_REG & CY_BIT)) {
				I8080_POP(*sp, *pc);
				cycles -= 6;
				goto dcr_cycle;
			}
			break;
		case 0xD1: /* POP D */
			I8080_POP(*sp, DE_REG);
			break;
		case 0xD2: /* JNC a16 */
			if (!(FLAG_REG & CY_BIT)) {
				*pc = i8080_memory_read(*pc);
				goto dcr_cycle;
			}
			break;
		case 0xD3: /* OUT d8 */
			i8080_io_output(i8080_memory_read(*pc), A_REG);
			break;
		case 0xD4: /* CNC a16 */
			if (!(FLAG_REG & CY_BIT)) {
				I8080_CALL(*sp, *pc);
				cycles -= 6;
				goto dcr_cycle;
			}
			break;
		case 0xD5: /* PUSH D */
			I8080_PUSH(*sp, DE_REG);
			break;
		case 0xD6: /* SUI d8 */
		{
			uint8_t b = i8080_memory_read(*pc);
			I8080_SUB(A_REG, b, FLAG_REG);
		}
			break;
		case 0xD7: /* RST 2 */
			I8080_RST(0x0010, *sp, *pc);
			break;
		case 0xD8: /* RC */
			if (FLAG_REG & CY_BIT) {
				I8080_POP(*sp, *pc);
				cycles -= 6;
				goto dcr_cycle;
			}
			break;
		case 0xDA: /* JC d16 */
			if (FLAG_REG & CY_BIT) {
				*pc = __i8080_memory_read_word(*pc);
				goto dcr_cycle;
			}
			break;
		case 0xDB: /* IN d8 */
			i8080_io_input(i8080_memory_read(*pc));
			break;
		case 0xDC: /* CC a16 */
			if (FLAG_REG & CY_BIT) {
				I8080_CALL(*sp, *pc);
				cycles -= 6;
				goto dcr_cycle;
			}
			break;
		case 0xDE: /* SBI d8 */
		{
			uint8_t b = i8080_memory_read(*pc) + (FLAG_REG & CY_BIT);
			I8080_SUB(A_REG, b, FLAG_REG);
		}
			break;
		case 0xDF: /* RST 3 */
			I8080_RST(0x0018, *sp, *pc);
			break;
		case 0xE0: /* RPO */
			if (!(FLAG_REG & P_BIT)) {
				I8080_POP(*sp, *pc);
				cycles -= 6;
				goto dcr_cycle;
			}
			break;
		case 0xE1: /* POP H */
			I8080_POP(*sp, HL_REG);
			break;
		case 0xE2: /* JPO a16 */
			if (!(FLAG_REG & P_BIT)) {
				*pc = __i8080_memory_read_word(*pc);
				goto dcr_cycle;
			}
			break;
		case 0xE3: /* XTHL */
		{
			uint16_t tmp = __i8080_memory_read_word(*sp);
			__i8080_memory_write_word(*sp, HL_REG);
			HL_REG = tmp;
		}
			break;
		case 0xE4: /* CPO a16 */
			if (!(FLAG_REG & P_BIT)) {
				I8080_CALL(*sp, *pc);
				cycles -= 6;
				goto dcr_cycle;
			}
			break;
		case 0xE5: /* PUSH H */
			I8080_PUSH(*sp, HL_REG);
			break;
		case 0xE6: /* ANI d8 */
		{
			uint8_t b = i8080_memory_read(*pc);
			I8080_ANA(A_REG, b, FLAG_REG);
		}
			break;
		case 0xE7: /* RST 4 */
			I8080_RST(0x0020, *sp, *pc);
			break;
		case 0xE8: /* RPE */
			if (FLAG_REG & P_BIT) {
				I8080_POP(*sp, *pc);
				cycles -= 6;
				goto dcr_cycle;
			}
			break;
		case 0xE9: /* PCHL */
			*pc = HL_REG;
			break;
		case 0xEA: /* JPE a16 */
			if (FLAG_REG & P_BIT) {
				*pc = __i8080_memory_read_word(*pc);
				goto dcr_cycle;
			}
			break;
		case 0xEB: /* XCHG */
		{
			uint16_t tmp = HL_REG;
			HL_REG = DE_REG;
			DE_REG = tmp;
		}
			break;
		case 0xEC: /* CPE a16 */
			if (FLAG_REG & P_BIT) {
				I8080_CALL(*sp, *pc);
				cycles -= 6;
				goto dcr_cycle;
			}
			break;
		case 0xEE: /* XRI d8 */
		{
			uint8_t b = i8080_memory_read(*pc);
			I8080_XRA(A_REG, b, FLAG_REG);
		}
			break;
		case 0xEF: /* RST 5 */
			I8080_RST(0x0028, *sp, *pc);
			break;
		case 0xF0: /* RP */
			if (!(FLAG_REG & S_BIT)) {
				I8080_POP(*sp, *pc);
				cycles -= 6;
				goto dcr_cycle;
			}
			break;
		case 0xF1: /* POP PSW */
			I8080_POP(*sp, PSW_REG);
			break;
		case 0xF2: /* JP a16 */
			if (!(FLAG_REG & S_BIT)) {
				*pc = __i8080_memory_read_word(*pc);
				goto dcr_cycle;
			}
			break;
		case 0xF3: /* DI */
			i8080_interrupt(i8080, 0);
			break;
		case 0xF4: /* CP a16 */
			if (!(FLAG_REG & S_BIT)) {
				I8080_CALL(*sp, *pc);
				cycles -= 6;
				goto dcr_cycle;
			}
			break;
		case 0xF5: /* PUSH PSW */
			I8080_PUSH(*sp, PSW_REG);
			break;
		case 0xF6: /* ORI d8 */
		{
			uint8_t b = i8080_memory_read(*pc);
			I8080_ORA(A_REG, b, FLAG_REG);
		}
			break;
		case 0xF7: /* RST 6 */
			I8080_RST(0x0030, *sp, *pc);
			break;
		case 0xF8: /* RM */
			if (FLAG_REG & S_BIT) {
				I8080_POP(*sp, *pc);
				cycles -= 6;
				goto dcr_cycle;
			}
			break;
		case 0xF9: /* SPHL */
			*sp = HL_REG;
			break;
		case 0xFA: /* JM a16 */
			if (FLAG_REG & S_BIT) {
				*pc = __i8080_memory_read_word(*pc);
				goto dcr_cycle;
			}
			break;
		case 0xFB: /* EI */
			i8080_interrupt(i8080, 1);
			break;
		case 0xFC: /* CM a16 */
			if (FLAG_REG & S_BIT) {
				I8080_CALL(*sp, *pc);
				cycles -= 6;
				goto dcr_cycle;
			}
			break;
		case 0xFE: /* CPI d8 */
		{
			uint8_t b = i8080_memory_read(*pc);
			I8080_CMP(A_REG, b, FLAG_REG);
		}
			break;
		case 0xFF: /* RST 7 */
			I8080_RST(0x0038, *sp, *pc);
			break;

		default:
			CC_ERROR("Unknown instruction @ 0x%04X: 0x%02X\n", *pc, opcode);
			exit(1);
		}

		*pc += i8080_instruction_size[opcode];
dcr_cycle:
		cycles -= i8080_instruction_cycle[opcode];
	}

	return cycles;
}

void i8080_interrupt(register struct intel_8080 *i8080 UNUSED,
					 register uint8_t state UNUSED)
{

}
