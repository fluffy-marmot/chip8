#include "chip8.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// if true, 8XY6, 8XYE copy VX to VY before shift operation
bool USE_COSMAC_VIP_SHIFT                 = true;
// if true, BNNN jump with offset instruction offsets by V0 instead of VX 
bool USE_COSMAC_VIP_JUMP_WITH_OFFSET      = true;
// if true, FX1E add VX to index instruction WILL NOT set VF to 1 on overflow (the original behavior)
bool USE_COSMAC_VIP_ADD_TO_INDEX_OVERFLOW = true;
// if true, FX55, Fx65 operations increment the index register
bool USE_COSMAC_VIP_INC_INDEX_ON_MEM_CP   = true;
// if true - 8XY1, 8XY2, 8XY3 set flag register VF to 0
bool USE_COSMAC_VIP_VF_RESET_AND_OR_XOR   = true;
int  INSTRUCTION_CYCLES_PER_FRAME         = 12;

typedef enum {
    INSTRUCTION_OK,
    INSTRUCTION_INVALID_CHIP8,
    INSTRUCTION_INVALID_SCHIP,
    INSTRUCTION_STACK_UNDERFLOW,
    INSTRUCTION_STACK_OVERFLOW,
} instruction_result_t;


cpu_t *CPU;
display_t *DISPLAY;
uint8_t *MEM;
uint8_t *KEYPAD;
uint8_t *KEYPAD_PREV;
uint8_t FLAG_REGISTERS[8];

void
load_font(const uint8_t font[], int memory_location)
{
    memcpy(MEM + memory_location, font, sizeof(font));
}

bool
init_memory(instruction_set_t chip)
{
    free(CPU);
    if (DISPLAY) {
        free(DISPLAY->pixels);
    }
    free(DISPLAY);
    free(MEM);
    free(KEYPAD);
    free(KEYPAD_PREV);

    CPU = calloc(1, sizeof(cpu_t));
    CPU->instruction_set = chip;
    DISPLAY = calloc(1, sizeof(display_t));
    if (!DISPLAY) {
        return 0;
    }
    DISPLAY->width = DISPLAY_WIDTH;
    DISPLAY->height = DISPLAY_HEIGHT;
    DISPLAY->width_max = (chip == INSTRUCTION_SET_CHIP8) ? DISPLAY_WIDTH : DISPLAY_WIDTH_EXT;
    DISPLAY->height_max = (chip == INSTRUCTION_SET_CHIP8) ? DISPLAY_HEIGHT : DISPLAY_HEIGHT_EXT;
    DISPLAY->pixels = calloc(DISPLAY->width_max * DISPLAY->height_max, sizeof(uint8_t));
    MEM = calloc(MEM_SIZE, sizeof(uint8_t));
    KEYPAD = calloc(16, sizeof(uint8_t));
    KEYPAD_PREV = calloc(16, sizeof(uint8_t));
    if (!CPU || !MEM || !DISPLAY->pixels || !KEYPAD || !KEYPAD_PREV) {
        return 0;
    }

    CPU->PC = PROG_MEMLOC;
    load_font(FONT_CHIP8, FONT_MEMLOC_CHIP8);
    load_font(FONT_SCHIP, FONT_MEMLOC_SCHIP);
    srand(time(NULL));
    return 1;
}

void
timer_cycle()
{
    if (CPU->delay_timer > 0) CPU->delay_timer--;
    if (CPU->sound_timer > 0) CPU->sound_timer--;
}

uint8_t
get_sound_timer()
{
    return CPU->sound_timer;
}

void
keypad_set(uint8_t key, bool state)
{
    KEYPAD[key & 0x0F] = state;
    //printf("Key event, set %d to %d\n", key, state);
}

void
display_clear()
{
    memset(DISPLAY->pixels, 0, DISPLAY->width_max * DISPLAY->height_max);
}

void
display_xor(uint8_t x, uint8_t y, uint8_t val)
{
    DISPLAY->pixels[y * DISPLAY->width_max + x] ^= val;
}

uint8_t
display_pixel(uint8_t x, uint8_t y)
{
    return DISPLAY->pixels[y * DISPLAY->width_max + x];
}

display_t *
get_display()
{
    return DISPLAY;
}

void
draw_sprite(uint8_t x, uint8_t y, uint8_t number_rows, uint8_t width)
{
    x = x % DISPLAY->width;
    y = y % DISPLAY->height;
    CPU->VAR[0X0F] = 0;
    uint16_t sprite_row;

    for (uint8_t dy = 0; dy < number_rows && y + dy < DISPLAY->height; dy++) {
        if (width == 8) {
            sprite_row = MEM[CPU->I + dy];
        } else if (width == 16) {
            sprite_row = (MEM[CPU->I + 2 * dy] << 8) | MEM[CPU->I + 2 * dy + 1];
        }
        for (uint8_t dx = 0; dx < width && x + dx < DISPLAY->width; dx++) {
            uint8_t pixel_bit = sprite_row >> (width - 1 - dx) & 0x01;
            CPU->VAR[0X0F] |= (display_pixel(x + dx, y + dy) & pixel_bit) > 0;
            display_xor(x + dx, y + dy, pixel_bit);
        } 
    }
}

instruction_result_t
decode_instruction_chip8(uint16_t instruction)
{
    // breakup the two instruction bytes into four "nibbles" (four half-bytes)
    uint8_t op = (uint8_t) ((instruction & 0xF000) >> 12);  // first nibble - broad instruction category
    uint8_t x  = (uint8_t) ((instruction & 0x0F00) >>  8);  // mostly used to refer to a register
    uint8_t y  = (uint8_t) ((instruction & 0x00F0) >>  4);  // mostly used to refer to a register
    uint8_t n  = (uint8_t) ((instruction & 0x000F)      );  // a 4-bit number

    uint8_t nn = (uint8_t) ((instruction & 0x00FF)      );  // the whole second byte
    uint8_t flag;

    switch (op) {
    case 0x00: 
        if (instruction == 0x00E0) {            // 00E0 clear display instruction
            display_clear();
        } else if (instruction == 0x00EE) {     // 00EE instruction to return from subroutine
            if (CPU->stack_len == 0) {
                return INSTRUCTION_STACK_UNDERFLOW;
            }
            CPU->PC = CPU->stack[--CPU->stack_len];
        } else {
            return INSTRUCTION_INVALID_CHIP8;
        }
        break;
    case 0x01: // 1NNN jump instruction, using other 12 bits as memory address
        CPU->PC = instruction & 0x0FFF;
        break;
    case 0x02: // 2NNN subroutine instruction, similar to 1NNN but push current address on stack
        if (CPU->stack_len == STACK_SIZE) {
            return INSTRUCTION_STACK_OVERFLOW;
        }
        CPU->stack[CPU->stack_len++] = CPU->PC;
        CPU->PC = instruction & 0x0FFF;
        break;
    case 0x03: // 3XNN - skip one instruction if VX equals NN
        if (CPU->VAR[x] == (uint8_t) (instruction & 0x00FF)) {
            CPU->PC += 2;
        }
        break;
    case 0x04: // 4XNN - skip one instruction if VX doesn't equal NN
        if (CPU->VAR[x] != nn) {
            CPU->PC += 2;
        }
        break;
    case 0x05: // 5XY0 - skip one instruction if VX equals VY
        if (n == 0x00) {
            if (CPU->VAR[x] == CPU->VAR[y]) {
                CPU->PC += 2;
            }
        } else {
            return INSTRUCTION_INVALID_CHIP8;
        }
        break;
    case 0x06: // 6XNN - set register VX to NN
        CPU->VAR[x] = nn;
        break;
    case 0x07: // 7XNN - add value to register VX
        CPU->VAR[x] += nn;
        break;
    case 0x08: // 8XY# - various arithmetic and bitwise operations

        switch (n) {
        case 0x00: // 8XY0 - set VX to value of VY
            CPU->VAR[x] = CPU->VAR[y];
            break;
        case 0x01: // 8XY1 - set VX to VX OR VY
            CPU->VAR[x] |= CPU->VAR[y];
            if (USE_COSMAC_VIP_VF_RESET_AND_OR_XOR) {
                CPU->VAR[0x0F] = 0;
            }
            break;
        case 0x02: // 8XY2 - set VX to VX AND VY
            CPU->VAR[x] &= CPU->VAR[y];
            if (USE_COSMAC_VIP_VF_RESET_AND_OR_XOR) {
                CPU->VAR[0x0F] = 0;
            }
            break;
        case 0x03: // 8XY3 - set VX to VX XOR VY
            CPU->VAR[x] ^= CPU->VAR[y];
            if (USE_COSMAC_VIP_VF_RESET_AND_OR_XOR) {
                CPU->VAR[0x0F] = 0;
            }
            break;
        case 0x04: // 8XY4 - VX = VX + VY, and set VF to 1 if VX + VY > 255 (overflows 8 bits)
            flag = ((uint16_t) CPU->VAR[x] + (uint16_t) CPU->VAR[y] > 0xFF);
            CPU->VAR[x] += CPU->VAR[y];
            CPU->VAR[0X0F] = flag;
            break;
        case 0x05: // 8XY5 - VX = VX - VY, and set VF to 1 if VX > VY (confusingly)
            flag = (CPU->VAR[x] >= CPU->VAR[y]);
            CPU->VAR[x] -= CPU->VAR[y];
            CPU->VAR[0X0F] = flag;
            break;
        case 0x06: // 8XY6 - right bit shift, two implementation standards exist
            if (USE_COSMAC_VIP_SHIFT) {
                CPU->VAR[x] = CPU->VAR[y]; // copy value in VY to VX if using this setting
            }
            flag = CPU->VAR[x] & 0x01; // set VF register to bit shifted out
            CPU->VAR[x] >>= 1;
            CPU->VAR[0X0F] = flag;
            break;
        case 0x07: // 8XY7 - VX = VY - VX, and set VF to 1 if VY > VX (confusingly)
            flag = (CPU->VAR[y] >= CPU->VAR[x]);
            CPU->VAR[x] = CPU->VAR[y] - CPU->VAR[x];
            CPU->VAR[0X0F] = flag;
            break;
        case 0x0E: // 8XYE - left bit shift, two implementation standards exist
            if (USE_COSMAC_VIP_SHIFT) {
                CPU->VAR[x] = CPU->VAR[y]; // copy value in VY to VX if using this setting
            }
            flag = (CPU->VAR[x] & 0x80) >> 7; // set VF register to bit shifted out
            CPU->VAR[x] <<= 1;
            CPU->VAR[0X0F] = flag;
            break;
        default:
            return INSTRUCTION_INVALID_CHIP8;
        }

        break;
    case 0x09: // 9XY0 - skip one instruction if VX doesn't equal VY
        if (n == 0x00) {
            if (CPU->VAR[x] != CPU->VAR[y]) {
                CPU->PC += 2;
            }
        } else {
            return INSTRUCTION_INVALID_CHIP8;
        }
        break;
    case 0x0A: // ANNN - set index register
        CPU->I = instruction & 0x0FFF;
        break;
    case 0x0B: // BNNN - jump with offset, a second (buggy?) implementation standard exists
        if (USE_COSMAC_VIP_JUMP_WITH_OFFSET) {
            CPU->PC = (instruction & 0x0FFF) + CPU->VAR[0x00]; // add V0 register to NNN
        } else {
            CPU->PC = (instruction & 0x0FFF) + CPU->VAR[x];    // add VX register to NNN
        }
        break;
    case 0x0C: // CXNN - generate random number, AND it with NN and put result in VX
        CPU->VAR[x] = (uint8_t) (rand() % 256) & nn;
        break;
    case 0x0D: // DXYN - draw instruction ?
        draw_sprite(CPU->VAR[x], CPU->VAR[y], n, 8);
        break;
    case 0x0E: // Skip if key pressed (or not pressed) instructions
        if (nn == 0x9E) { // EX9E - skip instruction if key VX is pressed
            if (KEYPAD[CPU->VAR[x] & 0x0F]) {
                CPU->PC += 2;
            }
        } else if (nn == 0xA1) { // EXA1 - skip instruction if key VX is -NOT- pressed
            if (!(KEYPAD[CPU->VAR[x] & 0x0F])) {
                CPU->PC += 2;
            }
        } else {
            return INSTRUCTION_INVALID_CHIP8;
        }
        break;
    case 0x0F:
        switch (nn) {
        case 0x07: // FX07 - sets VX to current value of delay timer
            CPU->VAR[x] = CPU->delay_timer;
            break;
        case 0x15: // FX15 - sets delay timer to current value of VX
            CPU->delay_timer = CPU->VAR[x];
            break;
        case 0x18: // FX18 - sets sound timer to current value of VX
            CPU->sound_timer = CPU->VAR[x];
            break;
        case 0x1E: // FX1E - add value of VX to index register
            flag = CPU->I + CPU->VAR[x] > 0x0FFF; // check for overflow above addressable 12 bits
            CPU->I = (CPU->I + CPU->VAR[x]) & 0x0FFF;
            if (!USE_COSMAC_VIP_ADD_TO_INDEX_OVERFLOW) {
                CPU->VAR[0x0F] = flag; // only set flag for certain implementation
            }
            break;
        case 0x0A: // FX0A - block execution until key input (my decrementing instruction pointer)
            bool key_press = false;
            for (uint8_t key = 0x00; key <= 0x0F; key++) {
                /* 
                following the convention: "On the original COSMAC VIP, the key was only registered 
                when it was pressed and then released." Therefore checking key pressed last cycle but not now
                */
                if (KEYPAD_PREV[key] && !KEYPAD[key]) {
                    key_press = true;
                    CPU->VAR[x] = key;
                    break;
                }
            }
            if (!key_press) {
                CPU->PC -= 2;
            }
            break;
        case 0x29: // FX29 - set index register to "font address" of hex character VX's value (0 <= VX <= 0xF)
            CPU->I = FONT_MEMLOC_CHIP8 + ((CPU->VAR[x] & 0x0F) * 5);
            break;
        case 0x33: // FX33 - binary-coded decimal conversion
            /* takes the number in VX (which is one byte, so it can be any number from 0 to 255) and converts it
            to three decimal digits, storing these digits in memory at the address in the index register I*/
            MEM[CPU->I]     =  CPU->VAR[x] / 100;
            MEM[CPU->I + 1] = (CPU->VAR[x] % 100) / 10;
            MEM[CPU->I + 2] =  CPU->VAR[x] % 10;
            break;
        case 0x55: // FX55 - store VX number of registers to memory
            for (uint8_t i = 0; i <= x; i++) {
                MEM[CPU->I + i] = CPU->VAR[i];
            }
            if (USE_COSMAC_VIP_INC_INDEX_ON_MEM_CP) {
                CPU->I += x + 1; // implementation option, I is incremented on each byte copied
            }
            break;
        case 0x65: // FX65 - load memory into VX number of registers
            for (uint8_t i = 0; i <= x; i++) {
                CPU->VAR[i] = MEM[CPU->I + i];
            }
            if (USE_COSMAC_VIP_INC_INDEX_ON_MEM_CP) {
                CPU->I += x + 1; // implementation option, I is incremented on each byte copied
            }
            break;
        default:
            return INSTRUCTION_INVALID_CHIP8;
        }
        break;
    }

    return INSTRUCTION_OK;
}

instruction_result_t
decode_instruction_schip(uint16_t instruction)
{
    // TODO - repetition - refactor later?
    // breakup the two instruction bytes into four "nibbles" (four half-bytes)
    uint8_t op = (uint8_t) ((instruction & 0xF000) >> 12);  // first nibble - broad instruction category
    uint8_t x  = (uint8_t) ((instruction & 0x0F00) >>  8);  // mostly used to refer to a register
    uint8_t y  = (uint8_t) ((instruction & 0x00F0) >>  4);  // mostly used to refer to a register
    uint8_t n  = (uint8_t) ((instruction & 0x000F)      );  // a 4-bit number

    uint8_t nn = (uint8_t) ((instruction & 0x00FF)      );  // the whole second byte
    uint8_t flag;

    if        (instruction == 0x00FB) { // 00FB - scroll right by 4
        memmove(DISPLAY->pixels + 4, DISPLAY->pixels, DISPLAY->height_max * DISPLAY->width_max - 4);
        for (uint8_t r = 0; r < DISPLAY->height; r++) {
            DISPLAY->pixels[r * DISPLAY->width_max + 0] = 
            DISPLAY->pixels[r * DISPLAY->width_max + 1] =
            DISPLAY->pixels[r * DISPLAY->width_max + 2] = 
            DISPLAY->pixels[r * DISPLAY->width_max + 3] = 0;
        }
    } else if (instruction == 0x00FC) { // 00FC - scroll left  by 4
        memmove(DISPLAY->pixels, DISPLAY->pixels + 4, DISPLAY->height_max * DISPLAY->width_max - 4);
        for (uint8_t r = 1; r <= DISPLAY->height; r++) {
            DISPLAY->pixels[r * DISPLAY->width_max - 1] = 
            DISPLAY->pixels[r * DISPLAY->width_max - 2] =
            DISPLAY->pixels[r * DISPLAY->width_max - 3] = 
            DISPLAY->pixels[r * DISPLAY->width_max - 4] = 0;
        }
    } else if (instruction == 0x00FD) { // 00FD - exit
        // TODO - guess I'd need a program in front 512B of mem to do something fun?
    } else if (instruction == 0x00FE) { // 00FE - disable hi-res
        DISPLAY->extended_mode = 0;
        display_clear();
    } else if (instruction == 0x00FF) { // 00FF - enable hi-res
        DISPLAY->extended_mode = 1;
        display_clear();
    } else if ((instruction & 0xFFF0) == 0x00C0) { // 00CN - scroll down N (as in move pixel data down)
        memmove(
            DISPLAY->pixels + DISPLAY->width_max * n, 
            DISPLAY->pixels, 
            DISPLAY->width_max * (DISPLAY->height_max - n)
        );
        memset(DISPLAY->pixels, 0, DISPLAY->width_max * n);
    } else if ((instruction & 0xF00F) == 0xD000) { // DXY0 - draw 16x16 sprite (or quirk TODO: maaaybe 8x16...)
        draw_sprite(CPU->VAR[x], CPU->VAR[y], 16, 16);
    } else if ((instruction & 0xF0FF) == 0xF030) { // FX30 - set I to address of large font X character
        CPU->I = FONT_MEMLOC_SCHIP + ((CPU->VAR[x] & 0x0F) * 10);
    } else if ((instruction & 0xF0FF) == 0xF075) { // FX75 - save V0 - VX to flag registers
        if (x > 0x07) { // only enough flag registers for 8 bytes
            return INSTRUCTION_INVALID_SCHIP;
        }
        memcpy(FLAG_REGISTERS, CPU->VAR, x + 1);
    } else if ((instruction & 0xF0FF) == 0xF085) { // FX85 - restore V0 - VX from flag registers
        if (x > 0x07) { // only enough flag registers for 8 bytes
            return INSTRUCTION_INVALID_SCHIP;
        }
        memcpy(CPU->VAR, FLAG_REGISTERS, x + 1);
    } else {
        return INSTRUCTION_INVALID_SCHIP;
    }

    return INSTRUCTION_OK;
}

bool
do_instruction_cycle()
{   
    // fetch current instruction two bytes and increment pointer by 2
    uint16_t instruction = (MEM[CPU->PC] << 8) | MEM[CPU->PC + 1];
    CPU->PC += 2;

    // decode and execute instruction
    instruction_result_t result = decode_instruction_chip8(instruction);
    if (result != INSTRUCTION_OK && CPU->instruction_set > INSTRUCTION_SET_CHIP8) {
        result = decode_instruction_schip(instruction);
    }

    memcpy(KEYPAD_PREV, KEYPAD, 16); // copy current keypad state to KEYPAD_PREV

    switch (result) {
    case INSTRUCTION_INVALID_CHIP8:
    case INSTRUCTION_INVALID_SCHIP:
        printf(
            "Unknown instruction %04X. Current PC: %04X. Instruction set: %d\n", 
            instruction, 
            CPU->PC, 
            CPU->instruction_set
        );
        return 0;
    case INSTRUCTION_STACK_UNDERFLOW:
        printf("Got subroutine return instruction when stack empty\n");
        return 0;
    case INSTRUCTION_STACK_OVERFLOW:
        printf("Subroutine stack overflow\n");
        return 0;
    }
    return 1;
}

bool
load_program(char *filename, instruction_set_t chip)
{   
    if (!init_memory(chip)) {
        printf("Error initializing memory during boot sequence\n");
        return 0;
    }
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