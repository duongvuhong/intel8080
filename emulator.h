#ifndef _EMULATOR_H
#define _EMULATOR_H

#include <stdint.h>

extern uint8_t i8080_memory_read(uint16_t);
extern void i8080_memory_write(uint16_t, uint8_t);
extern uint8_t i8080_io_input(uint8_t);
extern void i8080_io_output(uint8_t, uint8_t);

#endif
