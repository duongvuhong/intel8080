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

typedef struct __attribute__((packed)) {
	uint8_t cy:1;
	uint8_t unused1:1;
	uint8_t p:1;
	uint8_t unused3:1;
	uint8_t ac:1;
	uint8_t unused5:1;
	uint8_t z:1;
	uint8_t s:1;
} condition_bits_t;

#define I8080_MEMORY_SIZE (64 * 1024)

typedef struct {
	pair_register_t regs[I8080_PAIR_REGISTERS];
	uint16_t sp;
	uint16_t pc;

	uint64_t cycles;
	bool halted;

	uint8_t (*memory_read_b)(uint16_t);
	void (*memory_write_b)(uint16_t, uint8_t);
	uint8_t (*port_in)(uint8_t);
	void (*port_out)(uint8_t, uint8_t);
} i8080_t;

extern void intel_8080_reset(i8080_t *);
extern void intel_8080_step(i8080_t *);

#endif /* _INTEL_8080_H */