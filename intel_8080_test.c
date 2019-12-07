#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "common.h"
#define LOW_ENDIAN /* Intel family is low-endian */
#include "intel_8080.h"

static uint8_t *memory;

static i8080_t cpu;

static int test_finished = FALSE;

static uint8_t memory_read_b(uint16_t addr)
{
    return memory[addr];
}

static void memory_write_b(uint16_t addr, uint8_t val)
{
    memory[addr] = val;
}

static uint8_t port_in(uint8_t port)
{
    uint8_t operation = (cpu.regs[BC_REG_INDEX].byte.l);

    if (operation == 2) {
        printf("%c", cpu.regs[DE_REG_INDEX].byte.l);
    } else if (operation == 9) {
        uint16_t addr = (cpu.regs[DE_REG_INDEX].word);
        do {
            printf("%c", memory[addr++]);
        } while(memory[addr] != '$');
    }

    return 0xFF;
}

static void port_out(uint8_t port UNUSED, uint8_t value UNUSED)
{
    test_finished = TRUE;
}

static int load_rom(const char *rom, uint16_t addr)
{
	size_t total = 0;
	uint8_t buffer[100];
	FILE *fp;

	fp = fopen(rom, "rb");

	while (total + addr < I8080_MEMORY_SIZE) {
		size_t nread = fread(buffer, sizeof(uint8_t), 100, fp);
		if (nread == 0)
			break;

		memcpy(memory + addr + total, buffer, nread);
		total += nread;
	}

	fclose(fp);

	if (total >= I8080_MEMORY_SIZE)
		return -1;

	CC_INFO("Loaded %s (%lu bytes)\n", rom, total);

	return 0;
}

static void execute_test(const char* filename, unsigned long cyc_expected)
{
	unsigned long nins = 0;
    long long diff;

    intel_8080_reset(&cpu);
    cpu.memory_read_b = memory_read_b;
    cpu.memory_write_b = memory_write_b;
    cpu.port_in = port_in;
    cpu.port_out = port_out;

    memset(memory, 0, I8080_MEMORY_SIZE);

    if (load_rom(filename, 0x0100) != 0) {
        CC_ERROR("Failed to load rom %s\n", filename);
        return;
    }

    printf("*** TEST: %s\n", filename);

    cpu.pc = 0x0100;
    /* inject "out 1,a" at 0x0000 (signal to stop the test) */
    memory[0x0000] = 0xD3;
    memory[0x0001] = 0x00;
    /* inject "in a,0" at 0x0005 (signal to output some characters) */
    memory[0x0005] = 0xDB;
    memory[0x0006] = 0x00;
    memory[0x0007] = 0xC9;

    test_finished = FALSE;

    while (!test_finished) {
        nins++;

        intel_8080_step(&cpu);
    }

    diff = cyc_expected - cpu.cycles;

    printf("\n*** %lu instructions executed on %lu cycles (expected=%lu, diff=%lld)\n\n",
            nins, cpu.cycles, cyc_expected, diff);
}

int main(int argc, char **argv)
{
    memory = malloc(I8080_MEMORY_SIZE);

    execute_test("cpu_tests/TST8080.COM", 4924LU);
    execute_test("cpu_tests/CPUTEST.COM", 255653383LU);
    execute_test("cpu_tests/8080PRE.COM", 7817LU);
    execute_test("cpu_tests/8080EXM.COM", 23803381171LU);

    free(memory);

	return 0;
}
