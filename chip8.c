// bool
#include <stdbool.h>
// uint_8t, uint_16t
#include <stdint.h>
// fopen, fseek, ftell, rewind, fread, fclose, SEEK_END
#include <stdio.h>
// malloc, calloc
#include <stdlib.h>
// memcpy, memset
#include <string.h> 

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

uint8_t FONT_SPRITES[] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

typedef struct {
    uint16_t PC;                    // Program counter (aka instruction pointer for program)
    uint16_t I;                     // Index register, used to point to memory locations
    uint16_t stack[STACK_SIZE];     // Stack of memory locations, used to call & return from subroutines 
    uint8_t  stack_ptr;             // Current index of stack
    uint8_t  VAR[16];               // 16 8-bit CPU variable registers, labeled V0 - VF
    uint8_t  delay_timer;           // 8-bit delay timer, decremented at 60Hz until it reaches 0
    uint8_t  sound_timer;           // 8-but sound timer, same as delay timer, produces beep when nonzero
} cpu_t;

cpu_t *CPU;
uint8_t *MEM;
uint8_t *DISPLAY;

void
load_font_sprites()
{
    memcpy(MEM + FONT_MEMLOC, FONT_SPRITES, sizeof(FONT_SPRITES));
}

bool
boot_sequence()
{
    free(CPU);
    free(MEM);
    free(DISPLAY);

    CPU = calloc(1, sizeof(cpu_t));
    MEM = calloc(MEM_SIZE, sizeof(uint8_t));
    DISPLAY = calloc(DISPLAY_WIDTH * DISPLAY_HEIGHT, sizeof(uint8_t));
    if (!CPU || !MEM || !DISPLAY) return 0;

    CPU->PC = PROG_MEMLOC;
    load_font_sprites();
    return 1;
}

void
display_clear()
{
    memset(DISPLAY, 0, DISPLAY_WIDTH * DISPLAY_HEIGHT);
}

void
display_xor(uint8_t x, uint8_t y, uint8_t val)
{
    DISPLAY[y * DISPLAY_WIDTH + x] ^= val;
}

uint8_t
display_get(uint8_t x, uint8_t y)
{
    return DISPLAY[y * DISPLAY_WIDTH + x];
}

void
draw_sprite(uint8_t x, uint8_t y, uint8_t number_rows)
{
    x = x % DISPLAY_WIDTH;
    y = y % DISPLAY_HEIGHT;
    CPU->VAR[0X0F] = 0;

    for (uint8_t dy = 0; dy < number_rows && y + dy < DISPLAY_HEIGHT; dy++) {
        uint8_t sprite_row = MEM[CPU->I + dy];
        for (uint8_t dx = 0; dx < 8 && x + dx < DISPLAY_WIDTH; dx++) {
            uint8_t pixel_bit = sprite_row >> (7 - dx) & 0x01;
            CPU->VAR[0X0F] |= (display_get(x + dx, y + dy) & pixel_bit) > 0;
            display_xor(x + dx, y + dy, pixel_bit);
        }
    }
}

uint16_t
fetch_instruction()
{
    uint16_t instruction = (MEM[CPU->PC] << 8) | MEM[CPU->PC + 1];
    CPU->PC += 2;
    return instruction;
}

void
decode_instruction(uint16_t instruction)
{
    // breakup the two instruction bytes into four "nibbles" (four half-bytes)
    uint8_t op = (uint8_t) ((instruction & 0xF000) >> 12);  // first nibble - broad instruction category
    uint8_t x  = (uint8_t) ((instruction & 0x0F00) >>  8);  // mostly used to refer to a register
    uint8_t y  = (uint8_t) ((instruction & 0x00F0) >>  4);  // mostly used to refer to a register
    uint8_t n  = (uint8_t) ((instruction & 0x000F)      );  // a 4-bit number

    switch (op) {
        case 0x00: // clear display, among other things
            if (instruction == 0x00E0)
                display_clear();
            break;
        case 0x01: // jump instruction, using other 12 bits as memory address
            CPU->PC = instruction & 0x0FFF;
            break;
        case 0x06: // 6XNN - set register VX to NN
            CPU->VAR[x] =  (uint8_t) (instruction & 0x00FF);
            break;
        case 0x07: // 7XNN - add value to register VX
            CPU->VAR[x] += (uint8_t) (instruction & 0x00FF);
            break;
        case 0x0A: // ANNN - set index register
            CPU->I = instruction & 0x0FFF;
            break;
        case 0x0D: // DXYN - draw instruction ?
            draw_sprite(CPU->VAR[x], CPU->VAR[y], n);
            break;
    }
}

void
loop()
{
    uint16_t instruction = fetch_instruction();
    decode_instruction(instruction);
}

bool
load_program(char *filename)
{
    FILE *f = fopen(filename, "rb");
    fseek(f, 0, SEEK_END);
    long size = ftell(f);

    uint16_t prog_ram = MEM_SIZE - PROG_MEMLOC;
    if (size > prog_ram) {
        printf("Program of %ld bytes greater than available RAM of %d bytes\n", size, prog_ram);
        return 0;
    }

    rewind(f);
    fread(MEM + PROG_MEMLOC, 1, size, f);
    fclose(f);

    printf("Loaded program %s of %ld bytes\n", filename, size);
    return 1;
}

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s <program>\n", argv[0]);
        return 1;
    }
    if (!boot_sequence()) {
        printf("Error initializing memory during boot sequence\n");
        return 1;
    }
    if (!load_program(argv[1])) {
        printf("Error loading program %s\n", argv[1]);
        return 1;
    }

    while (true)
        loop();
}