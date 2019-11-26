#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CC_VERBOSE
#include "common.h"
#define LOW_ENDIAN /* Intel family is low-endian */
#include "intel_8080.h"

#define unknown_opcode(op, pc)                                       \
do {                                                                 \
	CC_ERROR("Unknown instruction @ 0x%04X: 0x%02X\n", pc, opcode);  \
} while (0)

#define regs               (i8080->regs)
#define sp                 (i8080->sp)
#define pc                 (i8080->pc)
#define cycles             (i8080->cycles)
#define halted             (i8080->halted)
#define memory_read_b      (i8080->memory_read_b)
#define memory_write_b     (i8080->memory_write_b)
#define port_in            (i8080->port_in)
#define port_out           (i8080->port_out)

#define reg_b    (regs[BC_REG_INDEX].byte.h)
#define reg_c    (regs[BC_REG_INDEX].byte.l)
#define reg_bc   (regs[BC_REG_INDEX].word)
#define reg_d    (regs[DE_REG_INDEX].byte.h)
#define reg_e    (regs[DE_REG_INDEX].byte.l)
#define reg_de   (regs[DE_REG_INDEX].word)
#define reg_h    (regs[HL_REG_INDEX].byte.h)
#define reg_l    (regs[HL_REG_INDEX].byte.l)
#define reg_hl   (regs[HL_REG_INDEX].word)
#define reg_a    (regs[PSW_REG_INDEX].byte.h)
#define flag     ((condition_bits_t *)&regs[PSW_REG_INDEX].byte.l)
#define reg_psw  (regs[PSW_REG_INDEX].word)

static const uint8_t i8080_instruction_size[256] = {
/* x0 x1 x2 x3 x4 x5 x6 x7 x8 x9 xA xB xC xD xE xF */
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
	1, 1, 3, 3, 3, 1, 2, 1, 1, 1, 3, 3, 3, 3, 2, 1, /* Cx */
	1, 1, 3, 2, 3, 1, 2, 1, 1, 1, 3, 2, 3, 3, 2, 1, /* Dx */
	1, 1, 3, 1, 3, 1, 2, 1, 1, 1, 3, 1, 3, 3, 2, 1, /* Ex */
	1, 1, 3, 1, 3, 1, 2, 1, 1, 1, 3, 1, 3, 3, 2, 1, /* Fx */
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

static int parity(unsigned int x)
{
	int n = 0;

	while (x > 0) {
		if (x & 0x01)
			n++;
		x >>= 1;
	}

	return (n & 0x01) ? 0 : 1;
}

#define __mem_read_b(addr) memory_read_b(addr)

#define __mem_read_w(addr) (memory_read_b(addr + 1) << 8 | memory_read_b(addr))

#define __mem_write_b(addr, byte) memory_write_b(addr, byte)

#define __mem_write_w(addr, word)                               \
do {                                                            \
	memory_write_b(addr, ((word) & 0xFF));                      \
	memory_write_b(addr + 1, ((word) >> 8) & 0xFF);             \
} while (0)

#define __i8080_stack_push(v)                                   \
do {                                                            \
	__mem_write_w(sp - 2, v);                                   \
	sp -= 2;                                                    \
} while (0)

#define __i8080_stack_pop(r)                                    \
do {                                                            \
	r = __mem_read_w(sp);                                       \
	sp += 2;                                                    \
} while (0)

#define __i8080_call(addr)                                      \
do {                                                            \
	__i8080_stack_push(pc + 3);                                 \
	pc = (addr);                                                \
} while (0)

#define __i8080_return() __i8080_stack_pop(pc)

#define __i8080_reposition_pc(op) (pc += i8080_instruction_size[op])

enum bits_group {
	BITS_GROUP1, /* Z, S, P, AC */
	BITS_GROUP2, /* Z, S, P, CY, AC */
	BITS_GROUP3, /* CY */
};

static void __i8080_state_cb(condition_bits_t *flg, uint32_t res, enum bits_group gr)
{
	switch (gr) {
	case BITS_GROUP1:
		flg->z = ((res & 0xFF) == 0);
		flg->s = ((res & 0x80) == 0x80);
		flg->p = parity(res & 0xFF);
		flg->ac = ((res & 0x10) == 0x10);
		break;
	case BITS_GROUP2:
		flg->z = ((res & 0xFF) == 0);
		flg->s = ((res & 0x80) == 0x80);
		flg->p = parity(res & 0xFF);
		flg->cy = ((res & 0xFF00) > 0);
		flg->ac = ((res & 0x10) == 0x10);
		break;
	case BITS_GROUP3:
		flg->cy = ((res & 0xFFFF0000) > 0);
		break;
	default:
		CC_WARNING("unknown bits group\n");
	}
}

#define __i8080_add(reg)                                        \
do {                                                            \
	uint16_t res = reg_a + (reg);                               \
	__i8080_state_cb(flag, res, BITS_GROUP2);                   \
	reg_a = (uint8_t)res;                                       \
} while (0)

#define __i8080_adc(reg)  __i8080_add((reg) + flag->cy)

#define __i8080_adi(byte) __i8080_add(byte)

#define __i8080_aci(byte) __i8080_add((byte) + flag->cy)

#define __i8080_sub(reg)                                        \
do {                                                            \
	uint16_t res = reg_a - (reg);                               \
	__i8080_state_cb(flag, res, BITS_GROUP2);                   \
	reg_a = (uint8_t)res;                                       \
} while (0)

#define __i8080_sbb(reg)  __i8080_sub(((reg) + flag->cy))

#define __i8080_sui(byte) __i8080_sub(byte)

#define __i8080_sbi(byte) __i8080_sub(((byte) + flag->cy))

#define __i8080_ana(reg)                                        \
do {                                                            \
	uint16_t res = reg_a & (reg);                               \
	__i8080_state_cb(flag, res, BITS_GROUP2);                   \
	reg_a = (uint8_t)res;                                       \
} while (0)

#define __i8080_ani(byte) __i8080_ana(byte)

#define __i8080_xra(reg)                                        \
do {                                                            \
	uint16_t res = reg_a ^ (reg);                               \
	__i8080_state_cb(flag, res, BITS_GROUP2);                   \
	reg_a = (uint8_t)res;                                       \
} while (0)

#define __i8080_xri(byte) __i8080_xra(byte)

#define __i8080_ora(reg)                                        \
do {                                                            \
	uint16_t res = reg_a | (reg);                               \
	__i8080_state_cb(flag, res, BITS_GROUP2);                   \
	reg_a = (uint8_t)res;                                       \
} while (0)

#define __i8080_ori(byte) __i8080_ora(byte)

#define __i8080_cmp(reg)                                        \
do {                                                            \
	uint16_t res = reg_a - (reg);                               \
	__i8080_state_cb(flag, res, BITS_GROUP2);                   \
} while (0)

#define __i8080_cpi(byte) __i8080_cmp(byte)

static int intel_8080_execute(i8080_t *i8080)
{
	int cyc = 0;
	uint8_t opcode;
	uint8_t byte;
	uint16_t offset;
	uint32_t result;

	opcode = __mem_read_b(pc);

	switch (opcode) {
	case 0x00: case 0x08: case 0x10: case 0x18: /* NOP */
	case 0x20: case 0x28: case 0x30: case 0x38: /* NOP */
		break;
	case 0x01: /* LXI B, d16 */
		reg_bc = __mem_read_w(pc + 1);
		break;
	case 0x02: /* STAX B */
		__mem_write_b(reg_bc, reg_a);
		break;
	case 0x03: /* INX B */
		reg_bc++;
		break;
	case 0x04: /* INR B */
		byte = ++reg_b;
		__i8080_state_cb(flag, byte, BITS_GROUP1);
		break;
	case 0x05: /* DCR B */
		byte = --reg_b;
		__i8080_state_cb(flag, byte, BITS_GROUP1);
		break;
	case 0x06: /* MVI B, d8 */
		reg_b = __mem_read_b(pc + 1);
		break;
	case 0x07: /* RLC */
		byte = (reg_a >> 7) & 0x01;
		reg_a <<= 1;
		reg_a |= byte;
		flag->cy = byte;
		break;
	case 0x09: /* DAD B */
		result = reg_hl + reg_bc;
		reg_hl = (uint16_t)result;
		__i8080_state_cb(flag, result, BITS_GROUP3);
		break;
	case 0x0A: /* LDAX B */
		reg_a = __mem_read_b(reg_bc);
		break;
	case 0x0B: /* DCX B */
		reg_bc--;
		break;
	case 0x0C: /* INR C */
		byte = ++reg_c;
		__i8080_state_cb(flag, byte, BITS_GROUP1);
		break;
	case 0x0D: /* DCR C */
		byte = --reg_c;
		__i8080_state_cb(flag, byte, BITS_GROUP1);
		break;
	case 0x0E: /* MVI C, d8 */
		reg_c = __mem_read_b(pc + 1);
		break;
	case 0x0F: /* RRC */
		byte = reg_a & 0x01;
		reg_a >>= 1;
		reg_a |= (byte << 7);
		flag->cy = byte;
		break;
	case 0x11: /* LXI D, d16 */
		reg_de = __mem_read_w(pc + 1);
		break;
	case 0x12: /* STAX D */
		__mem_write_b(reg_de, reg_a);
		break;
	case 0x13: /* INX D */
		reg_de++;
		break;
	case 0x14: /* INR D */
		byte = ++reg_d;
		__i8080_state_cb(flag, byte, BITS_GROUP1);
		break;
	case 0x15: /* DCR D */
		byte = --reg_d;
		__i8080_state_cb(flag, byte, BITS_GROUP1);
		break;
	case 0x16: /* MVI D, d8 */
		reg_d = __mem_read_b(pc + 1);
		break;
	case 0x17: /* RAL */
		byte = (reg_a >> 7) & 0x01;
		reg_a <<= 1;
		reg_a |= flag->cy;
		flag->cy = byte;
		break;
	case 0x19: /* DAD D */
		result = reg_hl + reg_de;
		reg_hl = (uint16_t)result;
		__i8080_state_cb(flag, result, BITS_GROUP3);
		break;
	case 0x1A: /* LDAX D */
		reg_a = __mem_read_b(reg_de);
		break;
	case 0x1B: /* DCX D */
		reg_de--;
		break;
	case 0x1C: /* INR E */
		byte = ++reg_e;
		__i8080_state_cb(flag, byte, BITS_GROUP1);
		break;
	case 0x1D: /* DCR E */
		byte = --reg_e;
		__i8080_state_cb(flag, byte, BITS_GROUP1);
		break;
	case 0x1E: /* MVI E, d8 */
		reg_e = __mem_read_b(pc + 1);
		break;
	case 0x1F: /* RAR */
		flag->cy = reg_a & 0x01;
		byte = (reg_a >> 7) & 0x01;
		reg_a >>= 1;
		reg_a |= (byte << 7);
		break;
	case 0x21: /* LXI H, d16 */
		reg_hl = __mem_read_w(pc + 1);
		break;
	case 0x22: /* SHLD a16 */
		offset = __mem_read_w(pc + 1);
		__mem_write_w(offset, reg_hl);
		break;
	case 0x23: /* INX H */
		reg_hl++;
		break;
	case 0x24: /* INR H */
		byte = ++reg_h;
		__i8080_state_cb(flag, byte, BITS_GROUP1);
		break;
	case 0x25: /* DCR H */
		byte = --reg_h;
		__i8080_state_cb(flag, byte, BITS_GROUP1);
		break;
	case 0x26: /* MVI H, d8 */
		reg_h = __mem_read_b(pc + 1);
		break;
	case 0x27: /* DAA */
		/* TODO: Special */
		break;
	case 0x29: /* DAD H */
		result = reg_hl + reg_hl;
		reg_hl = (uint16_t)result;
		__i8080_state_cb(flag, result, BITS_GROUP3);
		break;
	case 0x2A: /* LHLD a16 */
		offset = __mem_read_w(pc + 1);
		reg_hl = __mem_read_w(offset);
		break;
	case 0x2B: /* DCX H */
		reg_hl--;
		break;
	case 0x2C: /* INR L */
		byte = ++reg_l;
		__i8080_state_cb(flag, byte, BITS_GROUP1);
		break;
	case 0x2D: /* DCR L */
		byte = --reg_l;
		__i8080_state_cb(flag, byte, BITS_GROUP1);
		break;
	case 0x2E: /* MVI L, d8 */
		reg_l = __mem_read_b(pc + 1);
		break;
	case 0x2F: /* CMA */
		reg_a = !reg_a;
		break;
	case 0x31: /* LXI SP, d16 */
		sp = __mem_read_w(pc + 1);
		break;
	case 0x32: /* STA a16 */
		offset = __mem_read_w(pc + 1);
		__mem_write_b(offset, reg_a);
		break;
	case 0x33: /* INX SP */
		sp++;
		break;
	case 0x34: /* INR M */
		byte = __mem_read_b(reg_hl) + 1;
		__mem_write_b(reg_hl, byte);
		__i8080_state_cb(flag, byte, BITS_GROUP1);
		break;
	case 0x35: /* DCR M */
		byte = __mem_read_b(reg_hl) - 1;
		__mem_write_b(reg_hl, byte);
		__i8080_state_cb(flag, byte, BITS_GROUP1);
		break;
	case 0x36: /* MVI M, d8 */
		byte = __mem_read_b(pc + 1);
		__mem_write_b(reg_hl, byte);
		break;
	case 0x37: /* STC */
		flag->cy = 1;
		break;
	case 0x39: /* DAD SP */
		result = reg_hl + sp;
		reg_hl = (uint16_t)result;
		__i8080_state_cb(flag, result, BITS_GROUP3);
		break;
	case 0x3A: /* LDA a16 */
		offset = __mem_read_w(pc + 1);
		reg_a = __mem_read_b(offset);
		break;
	case 0x3B: /* DCX SP */
		sp--;
		break;
	case 0x3C: /* INR A */
		byte = ++reg_a;
		__i8080_state_cb(flag, byte, BITS_GROUP1);
		break;
	case 0x3D: /* DCR A */
		byte = --reg_a;
		__i8080_state_cb(flag, byte, BITS_GROUP1);
		break;
	case 0x3E: /* MVI A, d8 */
		reg_a = __mem_read_b(pc + 1);
		break;
	case 0x3F: /* CMC */
		flag->cy = !flag->cy;
		break;
	case 0x40: /* MOV B, B */
		reg_b = reg_b;
		break;
	case 0x41: /* MOV B, C */
		reg_b = reg_c;
		break;
	case 0x42: /* MOV B, D */
		reg_b = reg_d;
		break;
	case 0x43: /* MOV B, E */
		reg_b = reg_e;
		break;
	case 0x44: /* MOV B, H */
		reg_b = reg_h;
		break;
	case 0x45: /* MOV B, L */
		reg_b = reg_l;
		break;
	case 0x46: /* MOV B, M */
		reg_b = __mem_read_b(reg_hl);
		break;
	case 0x47: /* MOV B, A */
		reg_b = reg_a;
		break;
	case 0x48: /* MOV C, B */
		reg_c = reg_b;
		break;
	case 0x49: /* MOV C, C */
		reg_c = reg_c;
		break;
	case 0x4A: /* MOV C, D */
		reg_c = reg_d;
		break;
	case 0x4B: /* MOV C, E */
		reg_c = reg_e;
		break;
	case 0x4C: /* MOV C, H */
		reg_c = reg_h;
		break;
	case 0x4D: /* MOV C, L */
		reg_c = reg_l;
		break;
	case 0x4E: /* MOV C, M */
		reg_c = __mem_read_b(reg_hl);
		break;
	case 0x4F: /* MOV C, A */
		reg_c = reg_a;
		break;
	case 0x50: /* MOV D, B */
		reg_d = reg_b;
		break;
	case 0x51: /* MOV D, C */
		reg_d = reg_c;
		break;
	case 0x52: /* MOV D, D */
		reg_d = reg_d;
		break;
	case 0x53: /* MOV D, E */
		reg_d = reg_e;
		break;
	case 0x54: /* MOV D, H */
		reg_d = reg_h;
		break;
	case 0x55: /* MOV D, L */
		reg_d = reg_l;
		break;
	case 0x56: /* MOV D, M */
		reg_d = __mem_read_b(reg_hl);
		break;
	case 0x57: /* MOV D, A */
		reg_d = reg_a;
		break;
	case 0x58: /* MOV E, B */
		reg_e = reg_d;
		break;
	case 0x59: /* MOV E, C */
		reg_e = reg_c;
		break;
	case 0x5A: /* MOV E, D */
		reg_e = reg_d;
		break;
	case 0x5B: /* MOV E, E */
		reg_e = reg_e;
		break;
	case 0x5C: /* MOV E, H */
		reg_e = reg_h;
		break;
	case 0x5D: /* MOV E, L */
		reg_e = reg_l;
		break;
	case 0x5E: /* MOV E, M */
		reg_e = __mem_read_b(reg_hl);
		break;
	case 0x5F: /* MOV E, A */
		reg_e = reg_a;
		break;
	case 0x60: /* MOV H, B */
		reg_h = reg_b;
		break;
	case 0x61: /* MOV H, C */
		reg_h = reg_c;
		break;
	case 0x62: /* MOV H, D */
		reg_h = reg_d;
		break;
	case 0x63: /* MOV H, E */
		reg_h = reg_e;
		break;
	case 0x64: /* MOV H, H */
		reg_h = reg_h;
		break;
	case 0x65: /* MOV H, L */
		reg_h = reg_l;
		break;
	case 0x66: /* MOV H, M */
		reg_h = __mem_read_b(reg_hl);
		break;
	case 0x67: /* MOV H, A */
		reg_h = reg_a;
		break;
	case 0x68: /* MOV L, B */
		reg_l = reg_b;
		break;
	case 0x69: /* MOV L, C */
		reg_l = reg_c;
		break;
	case 0x6A: /* MOV L, D */
		reg_l = reg_d;
		break;
	case 0x6B: /* MOV L, E */
		reg_l = reg_e;
		break;
	case 0x6C: /* MOV L, H */
		reg_l = reg_h;
		break;
	case 0x6D: /* MOV L, L */
		reg_l = reg_l;
		break;
	case 0x6E: /* MOV L, M */
		reg_l = __mem_read_b(reg_hl);
		break;
	case 0x6F: /* MOV L, A */
		reg_l = reg_a;
		break;
	case 0x70: /* MOV M, B */
		__mem_write_b(reg_hl, reg_b);
		break;
	case 0x71: /* MOV M, C */
		__mem_write_b(reg_hl, reg_c);
		break;
	case 0x72: /* MOV M, D */
		__mem_write_b(reg_hl, reg_d);
		break;
	case 0x73: /* MOV M, E */
		__mem_write_b(reg_hl, reg_e);
		break;
	case 0x74: /* MOV M, H */
		__mem_write_b(reg_hl, reg_h);
		break;
	case 0x75: /* MOV M, L */
		__mem_write_b(reg_hl, reg_l);
		break;
	case 0x76: /* HLT */ /* Special */
		halted = TRUE;
		break;
	case 0x77: /* MOV M, A */
		__mem_write_b(reg_hl, reg_a);
		break;
	case 0x78: /* MOV A, B */
		reg_a = reg_b;
		break;
	case 0x79: /* MOV A, C */
		reg_a = reg_c;
		break;
	case 0x7A: /* MOV A, D */
		reg_a = reg_d;
		break;
	case 0x7B: /* MOV A, E */
		reg_a = reg_e;
		break;
	case 0x7C: /* MOV A, H */
		reg_a = reg_h;
		break;
	case 0x7D: /* MOV A, L */
		reg_a = reg_l;
		break;
	case 0x7E: /* MOV A, M */
		reg_a = __mem_read_b(reg_hl);
		break;
	case 0x7F: /* MOV A, A */
		reg_a = reg_a;
		break;
	case 0x80: /* ADD B */
		__i8080_add(reg_b);
		break;
	case 0x81: /* ADD C */
		__i8080_add(reg_c);
		break;
	case 0x82: /* ADD D */
		__i8080_add(reg_d);
		break;
	case 0x83: /* ADD E */
		__i8080_add(reg_e);
		break;
	case 0x84: /* ADD H */
		__i8080_add(reg_h);
		break;
	case 0x85: /* ADD L */
		__i8080_add(reg_l);
		break;
	case 0x86: /* ADD M */
		__i8080_add(__mem_read_b(reg_hl));
		break;
	case 0x87: /* ADD A */
		__i8080_add(reg_a);
		break;
	case 0x88: /* ADC B */
		__i8080_adc(reg_b);
		break;
	case 0x89: /* ADC C */
		__i8080_adc(reg_c);
		break;
	case 0x8A: /* ADC D */
		__i8080_adc(reg_d);
		break;
	case 0x8B: /* ADC E */
		__i8080_adc(reg_e);
		break;
	case 0x8C: /* ADC H */
		__i8080_adc(reg_h);
		break;
	case 0x8D: /* ADC L */
		__i8080_adc(reg_l);
		break;
	case 0x8E: /* ADC M */
		__i8080_adc(__mem_read_b(reg_hl));
		break;
	case 0x8F: /* ADC A */
		__i8080_adc(reg_a);
		break;
	case 0x90: /* SUB B */
		__i8080_sub(reg_b);
		break;
	case 0x91: /* SUB C */
		__i8080_sub(reg_c);
		break;
	case 0x92: /* SUB D */
		__i8080_sub(reg_d);
		break;
	case 0x93: /* SUB E */
		__i8080_sub(reg_e);
		break;
	case 0x94: /* SUB H */
		__i8080_sub(reg_h);
		break;
	case 0x95: /* SUB L */
		__i8080_sub(reg_l);
		break;
	case 0x96: /* SUB M */
		__i8080_sub(__mem_read_b(reg_hl));
		break;
	case 0x97: /* SUB A */
		__i8080_sub(reg_a);
		break;
	case 0x98: /* SBB B */
		__i8080_sbb(reg_b);
		break;
	case 0x99: /* SBB C */
		__i8080_sbb(reg_c);
		break;
	case 0x9A: /* SBB D */
		__i8080_sbb(reg_d);
		break;
	case 0x9B: /* SBB E */
		__i8080_sbb(reg_b);
		break;
	case 0x9C: /* SBB H */
		__i8080_sbb(reg_b);
		break;
	case 0x9D: /* SBB L */
		__i8080_sbb(reg_b);
		break;
	case 0x9E: /* SBB M */
		__i8080_sbb(__mem_read_b(reg_hl));
		break;
	case 0x9F: /* SBB A */
		__i8080_sbb(reg_a);
		break;
	case 0xA0: /* ANA B */
		__i8080_ana(reg_b);
		break;
	case 0xA1: /* ANA C */
		__i8080_ana(reg_c);
		break;
	case 0xA2: /* ANA D */
		__i8080_ana(reg_d);
		break;
	case 0xA3: /* ANA E */
		__i8080_ana(reg_e);
		break;
	case 0xA4: /* ANA H */
		__i8080_ana(reg_h);
		break;
	case 0xA5: /* ANA L */
		__i8080_ana(reg_l);
		break;
	case 0xA6: /* ANA M */
		__i8080_ana(__mem_read_b(reg_hl));
		break;
	case 0xA7: /* ANA A */
		__i8080_ana(reg_a);
		break;
	case 0xA8: /* XRA B */
		__i8080_xra(reg_d);
		break;
	case 0xA9: /* XRA C */
		__i8080_xra(reg_c);
		break;
	case 0xAA: /* XRA D */
		__i8080_xra(reg_d);
		break;
	case 0xAB: /* XRA E */
		__i8080_xra(reg_e);
		break;
	case 0xAC: /* XRA H */
		__i8080_xra(reg_h);
		break;
	case 0xAD: /* XRA L */
		__i8080_xra(reg_l);
		break;
	case 0xAE: /* XRA M */
		__i8080_xra(__mem_read_b(reg_hl));
		break;
	case 0xAF: /* XRA A */
		__i8080_xra(reg_a);
		break;
	case 0xB0: /* ORA B */
		__i8080_ora(reg_b);
		break;
	case 0xB1: /* ORA C */
		__i8080_ora(reg_c);
		break;
	case 0xB2: /* ORA D */
		__i8080_ora(reg_d);
		break;
	case 0xB3: /* ORA E */
		__i8080_ora(reg_e);
		break;
	case 0xB4: /* ORA H */
		__i8080_ora(reg_h);
		break;
	case 0xB5: /* ORA L */
		__i8080_ora(reg_l);
		break;
	case 0xB6: /* ORA M */
		__i8080_ora(__mem_read_b(reg_hl));
		break;
	case 0xB7: /* ORA A */
		__i8080_ora(reg_a);
		break;
	case 0xB8: /* CMP B */
		__i8080_cmp(reg_d);
		break;
	case 0xB9: /* CMP C */
		__i8080_cmp(reg_c);
		break;
	case 0xBA: /* CMP D */
		__i8080_cmp(reg_d);
		break;
	case 0xBB: /* CMP E */
		__i8080_cmp(reg_e);
		break;
	case 0xBC: /* CMP H */
		__i8080_cmp(reg_h);
		break;
	case 0xBD: /* CMP L */
		__i8080_cmp(reg_l);
		break;
	case 0xBE: /* CMP M */
		__i8080_cmp(__mem_read_b(reg_hl));
		break;
	case 0xBF: /* CMP A */
		__i8080_cmp(reg_a);
		break;
	case 0xC0: /* RNZ */
		if (!flag->z) {
			__i8080_return();
			cyc = 6;
			goto ret;
		}
		break;
	case 0xC1: /* POP B */
		__i8080_stack_pop(reg_bc);
		break;
	case 0xC2: /* JNZ a16 */
		if (!flag->z) {
			pc = __mem_read_w(pc + 1);
			goto ret;
		}
		break;
	case 0xC3: case 0xCB: /* JMP a16 */
		pc = __mem_read_w(pc + 1);
		goto ret;
	case 0xC4: /* CNZ a16 */
		if (!flag->z) {
			offset = __mem_read_w(pc + 1);
			__i8080_call(offset);
			cyc = 6;
			goto ret;
		}
		break;
	case 0xC5: /* PUSH B */
		__i8080_stack_push(reg_bc);
		break;
	case 0xC6: /* ADI d8 */
		byte = __mem_read_b(pc + 1);
		__i8080_adi(byte);
		break;
	case 0xC7: /* RST 0 */
		__i8080_call(0x0000);
		goto ret;
		break;
	case 0xC8: /* RZ */
		if (flag->z) {
			__i8080_return();
			cyc = 6;
			goto ret;
		}
		break;
	case 0xC9: case 0xD9: /* RET */
		__i8080_return();
		goto ret;
	case 0xCA: /* JZ a16 */
		if (flag->z) {
			pc = __mem_read_w(pc + 1);
			goto ret;
		}
		break;
	case 0xCC: /* CZ a16 */
		if (flag->z) {
			offset = __mem_read_w(pc + 1);
			__i8080_call(offset);
			cyc = 6;
			goto ret;
		}
		break;
	case 0xCD: case 0xDD: case 0xED: case 0xFD: /* CALL a16 */
		offset = __mem_read_w(pc + 1);
		__i8080_call(offset);
		goto ret;
		break;
	case 0xCE: /* ACI d8 */
		byte = __mem_read_b(pc + 1);
		__i8080_aci(byte);
		break;
	case 0xCF: /* RST 1 */
		__i8080_call(0x0008);
		goto ret;
		break;
	case 0xD0: /* RNC */
		if (!flag->cy) {
			__i8080_return();
			cyc = 6;
			goto ret;
		}
		break;
	case 0xD1: /* POP D */
		__i8080_stack_pop(reg_de);
		break;
	case 0xD2: /* JNC a16 */
		if (!flag->cy) {
			pc = __mem_read_w(pc + 1);
			goto ret;
		}
		break;
	case 0xD3: /* OUT d8 */ /* Special */
		byte = __mem_read_b(pc + 1);
		port_out(i8080, byte, reg_a);
		break;
	case 0xD4: /* CNC a16 */
		if (!flag->cy) {
			offset = __mem_read_w(pc + 1);
			__i8080_call(offset);
			cyc = 6;
			goto ret;
		}
		break;
	case 0xD5: /* PUSH D */
		__i8080_stack_push(reg_de);
		break;
	case 0xD6: /* SUI d8 */
		byte = __mem_read_b(pc + 1);
		__i8080_sui(byte);
		break;
	case 0xD7: /* RST 2 */
		__i8080_call(0x0010);
		goto ret;
	case 0xD8: /* RC */
		if (flag->cy) {
			__i8080_return();
			cyc = 6;
			goto ret;
		}
		break;
	case 0xDA: /* JC d16 */
		if (flag->cy) {
			pc = __mem_read_w(pc + 1);
			goto ret;
		}
		break;
	case 0xDB: /* IN d8 */ /* Special */
		byte = __mem_read_b(pc + 1);
		reg_a = port_in(i8080, byte);
		break;
	case 0xDC: /* CC a16 */
		if (flag->cy) {
			offset = __mem_read_w(pc + 1);
			__i8080_call(offset);
			cyc = 6;
			goto ret;
		}
		break;
	case 0xDE: /* SBI d8 */
		byte = __mem_read_b(pc + 1);
		__i8080_sbi(byte);
		break;
	case 0xDF: /* RST 3 */
		__i8080_call(0x0018);
		goto ret;
	case 0xE0: /* RPO */
		if (!flag->p) {
			__i8080_return();
			cyc = 6;
			goto ret;
		}
		break;
	case 0xE1: /* POP H */
		__i8080_stack_pop(reg_hl);
		break;
	case 0xE2: /* JPO a16 */
		if (!flag->p) {
			pc = __mem_read_w(pc + 1);
			goto ret;
		}
		break;
	case 0xE3: /* XTHL */
		offset = reg_hl;
		reg_hl = __mem_read_w(sp);
		__mem_write_w(sp, offset);
		break;
	case 0xE4: /* CPO a16 */
		if (!flag->p) {
			offset = __mem_read_w(pc + 1);
			__i8080_call(offset);
			cyc = 6;
			goto ret;
		}
		break;
	case 0xE5: /* PUSH H */
		__i8080_stack_push(reg_hl);
		break;
	case 0xE6: /* ANI d8 */
		byte = __mem_read_b(pc + 1);
		__i8080_ani(byte);
		break;
	case 0xE7: /* RST 4 */
		__i8080_call(0x0020);
		goto ret;
	case 0xE8: /* RPE */
		if (flag->p) {
			__i8080_return();
			cyc = 6;
			goto ret;
		}
		break;
	case 0xE9: /* PCHL */
		pc = reg_hl;
		goto ret;
	case 0xEA: /* JPE a16 */
		if (flag->p) {
			pc = __mem_read_w(pc + 1);
			goto ret;
		}
		break;
	case 0xEB: /* XCHG */
		offset = reg_hl;
		reg_hl = reg_de;
		reg_de = offset;
		break;
	case 0xEC: /* CPE a16 */
		if (flag->p) {
			offset = __mem_read_w(pc + 1);
			__i8080_call(offset);
			cyc = 6;
			goto ret;
		}
		break;
	case 0xEE: /* XRI d8 */
		byte = __mem_read_b(pc + 1);
		__i8080_xri(byte);
		break;
	case 0xEF: /* RST 5 */
		__i8080_call(0x0028);
		goto ret;
	case 0xF0: /* RP */
		if (!flag->s) {
			__i8080_return();
			cyc = 6;
			goto ret;
		}
		break;
	case 0xF1: /* POP PSW */
		__i8080_stack_pop(reg_psw);
		break;
	case 0xF2: /* JP a16 */
		if (!flag->s) {
			pc = __mem_read_w(pc + 1);
			goto ret;
		}
		break;
	case 0xF3: /* DI */
		/* TODO: Special */
		break;
	case 0xF4: /* CP a16 */
		if (!flag->s) {
			offset = __mem_read_w(pc + 1);
			__i8080_call(offset);
			cyc = 6;
			goto ret;
		}
		break;
	case 0xF5: /* PUSH PSW */
		__i8080_stack_push(reg_psw);
		break;
	case 0xF6: /* ORI d8 */
		byte = __mem_read_b(pc + 1);
		__i8080_ori(byte);
		break;
	case 0xF7: /* RST 6 */
		__i8080_call(0x0030);
		goto ret;
	case 0xF8: /* RM */
		if (flag->s) {
			__i8080_return();
			cyc = 6;
			goto ret;
		}
		break;
	case 0xF9: /* SPHL */
		sp = reg_hl;
		break;
	case 0xFA: /* JM a16 */
		if (flag->s) {
			pc = __mem_read_w(pc + 1);
			goto ret;
		}
		break;
	case 0xFB: /* EI */
		/* TODO: Special */
		break;
	case 0xFC: /* CM a16 */
		if (flag->s) {
			offset = __mem_read_w(pc + 1);
			__i8080_call(offset);
			cyc = 6;
			goto ret;
		}
		break;
	case 0xFE: /* CPI d8 */
		byte = __mem_read_b(pc + 1);
		__i8080_cpi(byte);
		break;
	case 0xFF: /* RST 7 */
		__i8080_call(0x0038);
		goto ret;
		break;
	default:
		unknown_opcode(opcode, pc);
		return -1;
	}

	__i8080_reposition_pc(opcode);

ret:
	cyc += i8080_instruction_cycle[opcode];

	return cyc;
}

void intel_8080_reset(i8080_t *i8080)
{
	memset(regs, 0, sizeof(pair_register_t) * I8080_PAIR_REGISTERS);
	flag->unused1 = 1;
	flag->unused3 = 0;
	flag->unused5 = 0;

	sp = 0;
	pc = 0;

	cycles = 0;
	halted = FALSE;

	memory_read_b = NULL;
	memory_write_b = NULL;
	port_in = NULL;
	port_out = NULL;
}

void intel_8080_step(i8080_t *i8080)
{
	int ret;

	if (!halted) {
        ret = intel_8080_execute(i8080);
		if (ret == -1)
			exit(42);

		cycles += ret;
	}
}
