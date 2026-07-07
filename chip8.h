#ifndef CHIP8_H
#define CHIP8_H

// uint_8t, uint_16t
#include <stdbool.h>
#include <stdint.h>

/*
Specifications: 
4096 bytes RAM (12 bit memory addresses)
64x32 monochrome display

The first 512 bytes of memory were used for interpreter and not touched by
actual programs, so they can be used to store some data on how to draw fonts,
for example. 
*/

#define MEM_SIZE        (4 * 1024)
#define DISPLAY_WIDTH   64
#define DISPLAY_HEIGHT  32
#define STACK_SIZE      64
#define FONT_MEMLOC     0x050
#define PROG_MEMLOC     0x200

typedef struct {
    uint16_t PC;                    // Program counter (aka instruction pointer for program)
    uint16_t I;                     // Index register, used to point to memory locations
    uint16_t stack[STACK_SIZE];     // Stack of memory locations, used to call & return from subroutines 
    uint8_t  stack_ptr;             // Current index of stack
    uint8_t  VAR[16];               // 16 8-bit CPU variable registers, labeled V0 - VF
    uint8_t  delay_timer;           // 8-bit delay timer, decremented at 60Hz until it reaches 0
    uint8_t  sound_timer;           // 8-but sound timer, same as delay timer, produces beep when nonzero
} cpu_t;

uint8_t display_get(uint8_t x, uint8_t y);
void do_instruction_cycle(void);
bool boot_sequence(int argc, char *argv[]);

#endif