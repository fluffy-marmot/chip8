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
        while (true) {
            do_instruction_cycle();
            window_draw();
        }

        //Hacky way to get window to stay up
        SDL_Event e; bool quit = false; while( quit == false ){ while( SDL_PollEvent( &e ) ){ if( e.type == SDL_QUIT ) quit = true; } }
    }

    window_close();

    return 0;
}