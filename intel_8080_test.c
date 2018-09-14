#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define CC_PRINT /* for CC_* macro */
#include "common.h"
#include "intel_8080.h"

#define I8080_MEMORY_SIZE (64 * 1024) /* 64 KiB */

#define B_REG    (i8080.regs[BC_REG_INDEX].byte.l)
#define C_REG    (i8080.regs[BC_REG_INDEX].byte.h)
#define BC_REG   (i8080.regs[BC_REG_INDEX].word)
#define D_REG    (i8080.regs[DE_REG_INDEX].byte.l)
#define E_REG    (i8080.regs[DE_REG_INDEX].byte.h)
#define DE_REG   (i8080.regs[DE_REG_INDEX].word)
#define H_REG    (i8080.regs[HL_REG_INDEX].byte.l)
#define L_REG    (i8080.regs[HL_REG_INDEX].byte.h)
#define HL_REG   (i8080.regs[HL_REG_INDEX].word)
#define A_REG    (i8080.regs[PSW_REG_INDEX].byte.l)
#define FLAG_REG (i8080.regs[PSW_REG_INDEX].byte.h)
#define PSW_REG  (i8080.regs[PSW_REG_INDEX].word)

static uint8_t memory[I8080_MEMORY_SIZE];

static struct intel_8080 i8080;

static void load_memory(const char *name, size_t p)
{
	char buff[100];
	size_t total = 0, n;

	FILE *fp = fopen(name, "rb");
	if (!fp) {
		CC_ERROR("Failed to open file %s\n", name);
		exit(1);
	}

	while (!feof(fp) && (total + p) <= I8080_MEMORY_SIZE) {
		n = fread(buff, 1, 100, fp);
		memcpy(memory + p + total, buff, n);
		total += n;
	}

	fclose(fp);

	if ((total + p) > I8080_MEMORY_SIZE) {
		CC_ERROR("Not enough space for %s (>>%lu)\n", name, total);
		exit(1);
	}

	CC_INFO("%s loaded, size %lu byte(s)\n", name, total);
}

static void execute_test(const char *name, int check)
{
	uint16_t pc;
	int success;

	memset(memory, 0, I8080_MEMORY_SIZE);
	load_memory(name, 0x100);

	memory[5] = 0xC9; /* inject RET at 0x0005 to handle "CALL 5" */

	i8080_reset(&i8080);
	i8080.pc = 0x100 & 0xffff;

	for (;;) {
		pc = i8080.pc;
		if (memory[pc] == 0x76) {
			printf("HLT at %04X\n", pc);
			exit(1);
		}
		if (pc == 0x0005) {
			if (C_REG == 9) {
				for (int i = DE_REG; memory[i] != '$'; i++)
					putchar(memory[i]);
				success = 1;
			}

			if (C_REG == 2)
				putchar((char)E_REG);
		}

		i8080_execute(&i8080, 1);

		if (i8080.pc == 0) {
			printf("\nJump to 0000 from %04X\n", pc);
			if (check && !success)
				exit(1);
			return;
		}
	}
}

uint8_t i8080_memory_read(uint16_t addr)
{
	return memory[addr & 0xffff];
}

void i8080_memory_write(uint16_t addr, uint8_t val)
{
	memory[addr & 0xffff] = val;
}

uint8_t i8080_io_input(uint8_t port UNUSED)
{
	return 0;
}

void i8080_io_output(uint8_t val UNUSED, uint8_t port UNUSED)
{

}

int main(int argc, char **argv)
{
	if (argc != 2) {
		CC_INFO("Usage: %s dir\n", argv[0]);
		exit(1);
	}

	execute_test("test/CPUTEST.COM", 0);
	execute_test("test/TEST.COM", 0);
	execute_test("test/8080PRE.COM", 1);
	execute_test("test/8080EX1.COM", 0);

	return 0;
}
