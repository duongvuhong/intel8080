#ifndef _INTEL_8080_H
#define _INTEL_8080_H

#include <stdint.h>

#define I8080_PAIR_REGISTERS 4

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
} pair_register_t;

typedef struct {
	uint8_t z:1;
	uint8_t s:1;
	uint8_t p:1;
	uint8_t ac:1;
	uint8_t cy:1;
	uint8_t padding:3;
} condition_bits_t;

#define I8080_MEMORY_SIZE (64 * 1024)

typedef struct {
	pair_register_t regs[I8080_PAIR_REGISTERS];
	uint16_t sp;
	uint16_t pc;
	uint8_t *memory;
	condition_bits_t cb;
	uint8_t int_enable;
} i8080_t;

extern void intel_8080_reset(i8080_t *);
extern int intel_8080_execute(register i8080_t *, register int);
extern void intel_8080_interrupt(register i8080_t *, register uint8_t);

#endif /* _INTEL_8080_H */