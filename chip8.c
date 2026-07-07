#include "chip8.h"

// fopen, fseek, ftell, rewind, fread, fclose, SEEK_END
#include <stdio.h>
// malloc, calloc
#include <stdlib.h>
// memcpy, memset
#include <string.h> 


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

cpu_t *CPU;
uint8_t *MEM;
uint8_t *DISPLAY;

void
load_font_sprites()
{
    memcpy(MEM + FONT_MEMLOC, FONT_SPRITES, sizeof(FONT_SPRITES));
}

bool
init_memory()
{
    free(CPU);
    free(MEM);
    free(DISPLAY);

    CPU = calloc(1, sizeof(cpu_t));
    MEM = calloc(MEM_SIZE, sizeof(uint8_t));
    DISPLAY = calloc(DISPLAY_WIDTH * DISPLAY_HEIGHT, sizeof(uint8_t));
    if (!CPU || !MEM || !DISPLAY) {
        return 0;
    }

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
            if (instruction == 0x00E0) {
                display_clear();
            }
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
do_instruction_cycle()
{
    uint16_t instruction = fetch_instruction();
    decode_instruction(instruction);
}

bool
load_program(char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f) {
        printf("Could not open %s", filename);
        return 0;
    }

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

bool
boot_sequence(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s <program>\n", argv[0]);
        return 0;
    }
    if (!init_memory()) {
        printf("Error initializing memory during boot sequence\n");
        return 0;
    }
    if (!load_program(argv[1])) {
        printf("Error loading program %s\n", argv[1]);
        return 0;
    }

    return 1;
}