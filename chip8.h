#ifndef CHIP8_H
#define CHIP8_H

// uint_8t, uint_16t
#include <stdbool.h>
#include <stdint.h>

// Configurable
extern bool USE_COSMAC_VIP_SHIFT;
extern bool USE_COSMAC_VIP_JUMP_WITH_OFFSET;
extern bool USE_COSMAC_VIP_ADD_TO_INDEX_OVERFLOW;
extern bool USE_COSMAC_VIP_INC_INDEX_ON_MEM_CP;
extern bool USE_COSMAC_VIP_VF_RESET_AND_OR_XOR;
extern int  INSTRUCTION_CYCLES_PER_FRAME;

/*
Specifications: 
4096 bytes RAM (12 bit memory addresses)
64x32 monochrome display

The first 512 bytes of memory were used for interpreter and not touched by
actual programs, so they can be used to store some data on how to draw fonts,
for example. 
*/

#define MEM_SIZE            (4 * 1024)
#define STACK_SIZE          64
#define FONT_MEMLOC         0x050
#define PROG_MEMLOC         0x200
#define DISPLAY_WIDTH       64
#define DISPLAY_HEIGHT      32
#define DISPLAY_WIDTH_MAX   64      // for later if implementing 128x64 superchip?
#define DISPLAY_HEIGHT_MAX  32

typedef struct {
    uint16_t PC;                    // Program counter (aka instruction pointer for program)
    uint16_t I;                     // Index register, used to point to memory locations
    uint16_t stack[STACK_SIZE];     // Stack of memory locations, used to call & return from subroutines 
    uint8_t  stack_len;             // Number of currently stored addresses on stack
    uint8_t  VAR[16];               // 16 8-bit CPU variable registers, labeled V0 - VF
    uint8_t  delay_timer;           // 8-bit delay timer, decremented at 60Hz until it reaches 0
    uint8_t  sound_timer;           // 8-but sound timer, same as delay timer, produces beep when nonzero
} cpu_t;

typedef struct {
    uint8_t  width;
    uint8_t  height;
    uint8_t  width_max;
    uint8_t  height_max;
    uint8_t *pixels;
} display_t;

bool load_program(char *filename);
bool do_instruction_cycle(void);
void timer_cycle(void);
void keypad_set(uint8_t key, bool state);
uint8_t get_sound_timer(void);
display_t *get_display(void);

#endif