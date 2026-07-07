#include "chip8.h"

#include <SDL2/SDL.h>

#include <stdbool.h>
#include <stdio.h>

#define MARGIN      5
#define BORDER      1
#define PIXEL_SIZE  10

#define CLR_MARGIN      SDL_MapRGB(screenSurface->format, 0x40, 0x40, 0x40)
#define CLR_BORDER      SDL_MapRGB(screenSurface->format, 0x90, 0xC0, 0xC0)
#define CLR_PIXEL_ON    SDL_MapRGB(screenSurface->format, 0xD0, 0xD0, 0xF0)
#define CLR_PIXEL_OFF   SDL_MapRGB(screenSurface->format, 0x10, 0x10, 0x10)

// kind of using the common convention 4x4 keypad on the left side of the keyboard
SDL_Scancode KEYMAP[16] = {
    SDL_SCANCODE_X,  // 0
    SDL_SCANCODE_1,  // 1
    SDL_SCANCODE_2,  // 2
    SDL_SCANCODE_3,  // 3
    SDL_SCANCODE_Q,  // 4
    SDL_SCANCODE_W,  // 5
    SDL_SCANCODE_E,  // 6
    SDL_SCANCODE_A,  // 7
    SDL_SCANCODE_S,  // 8
    SDL_SCANCODE_D,  // 9
    SDL_SCANCODE_Z,  // A
    SDL_SCANCODE_C,  // B
    SDL_SCANCODE_4,  // C
    SDL_SCANCODE_R,  // D
    SDL_SCANCODE_F,  // E
    SDL_SCANCODE_V,  // F
};

SDL_Window *window;
SDL_Surface *screenSurface;

void
window_draw()
{
    int width = 2 * MARGIN + PIXEL_SIZE * DISPLAY_WIDTH + BORDER * (DISPLAY_WIDTH + 1);
    int height = 2 * MARGIN + PIXEL_SIZE * DISPLAY_HEIGHT + BORDER * (DISPLAY_HEIGHT + 1);
    SDL_Rect inner = {MARGIN, MARGIN, width - 2 * MARGIN, height - 2 * MARGIN};

    SDL_FillRect(screenSurface, NULL, CLR_MARGIN);
    SDL_FillRect(screenSurface, &inner, CLR_BORDER);

    for (int x = 0; x < DISPLAY_WIDTH; x++) {
        for (int y = 0; y < DISPLAY_HEIGHT; y++) {
            SDL_Rect pixel = {
                MARGIN + x * PIXEL_SIZE + (x + 1) * BORDER,
                MARGIN + y * PIXEL_SIZE + (y + 1) * BORDER,
                PIXEL_SIZE,
                PIXEL_SIZE
            };
            SDL_FillRect(screenSurface, &pixel, display_get(x, y) ? CLR_PIXEL_ON : CLR_PIXEL_OFF);
        }
    }

    SDL_UpdateWindowSurface(window);
}


bool
window_init()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 0;
    } else {
        window = SDL_CreateWindow(
            "CHIP-8 Demo",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            2 * MARGIN + PIXEL_SIZE * DISPLAY_WIDTH + BORDER * (DISPLAY_WIDTH + 1),
            2 * MARGIN + PIXEL_SIZE * DISPLAY_HEIGHT + BORDER * (DISPLAY_HEIGHT + 1),
            SDL_WINDOW_SHOWN
        );
        if (window == NULL) {
            printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
            return 0;
        } else {
            screenSurface = SDL_GetWindowSurface(window);
            window_draw();
        }
    }
    return 1;
}

void
window_close()
{
    SDL_DestroyWindow(window);
    SDL_Quit();
}

int
main(int argc, char *args[])
{
    if (!boot_sequence(argc, args)) {
        printf("Error in CHIP-8 boot sequence\n");
    } else if (!window_init()) {
        printf("Failed to initialize SDL window\n");
    } else {
        bool quit = false;
        SDL_Event e;
        const double MS_PER_INSTRUCTION = 1000.0 / INSTRUCTION_CYCLES_PER_SEC;
        // timer decrement is supposed to be called at 60Hz, regardless of instruction speed
        const double MS_PER_TIMER_CYCLE = 1000.0 / 60.0;

        uint64_t last_timer = SDL_GetTicks64();
        uint64_t last_instruction = SDL_GetTicks64();
        uint64_t now;

        while (!quit) {
            while (SDL_PollEvent(&e) != 0) {
                if (e.type == SDL_QUIT) {
                    quit = true;
                } else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                    for (uint8_t key = 0; key <= 0x0F; key++) {
                        if (e.key.keysym.scancode == KEYMAP[key]) {
                            keypad_set(key, e.type == SDL_KEYDOWN);
                            break;
                        }
                    }
                }
            }
            
            now = SDL_GetTicks64();
            if (now - last_timer >= MS_PER_TIMER_CYCLE) {
                timer_cycle();
                last_timer = now;
            }
            if (now - last_instruction >= MS_PER_INSTRUCTION) {
                do_instruction_cycle();
                // TODO - maybe would be better to draw at 60Hz instead of every instruction...
                window_draw();
                last_instruction = now;
            }

            SDL_Delay(1); // don't burn out the CPU... :)
        }  
    }

    window_close();

    return 0;
}