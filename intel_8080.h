#ifndef _INTEL_8080_H
#define _INTEL_8080_H

#include <stdint.h>

#define I8080_PAIR_REGISTERS 4
#define I8080_STACK_LEVEL 16

#define BC_REG_INDEX  0
#define DE_REG_INDEX  1
#define HL_REG_INDEX  2
#define PSW_REG_INDEX 3

typedef union {
	struct {
#ifdef LOW_ENDIAN
		uint8_t l;
		uint8_t h;
#else
		uint8_t h;
		uint8_t l;
#endif
	} byte;
	uint16_t word;
} pair_reg;

struct intel_8080 {
	pair_reg regs[I8080_PAIR_REGISTERS];
	uint16_t sp;
	uint16_t pc;
};

extern void i8080_reset(struct intel_8080 *);
extern int i8080_execute(register struct intel_8080 *, register int);
extern void i8080_interrupt(register struct intel_8080 *, register uint8_t);

#endif
