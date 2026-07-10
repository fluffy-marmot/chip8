#include "chip8.h"

#include <SDL2/SDL.h>

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define MARGIN      10
#define BORDER      0
#define PIXEL_SIZE  20

#define WIN_WIDTH  (2 * MARGIN + PIXEL_SIZE * DISPLAY->width + BORDER * (DISPLAY->width + 1))
#define WIN_HEIGHT (2 * MARGIN + PIXEL_SIZE * DISPLAY->height + BORDER * (DISPLAY->height + 1))

#define CLR_MARGIN      SDL_MapRGB(screenSurface->format, 0x40, 0x40, 0x40)
#define CLR_BORDER      SDL_MapRGB(screenSurface->format, 0x90, 0xC0, 0xC0)
#define CLR_PIXEL_ON    SDL_MapRGB(screenSurface->format, 0xD0, 0xD0, 0xF0)
#define CLR_PIXEL_OFF   SDL_MapRGB(screenSurface->format, 0x10, 0x10, 0x10)

#define AUDIO_SAMPLE_RATE   44100
#define AUDIO_AMPLITUDE     28000
#define AUDIO_FREQUENCY     440.0
#define PI                  3.14159265358979323846

const int FRAMES_PER_SECOND = 60;

// kind of using the common convention:
// The keys used to activate CHIP-8's 4x4, 16 key, keypad: 1234 QWER ASDF ZXCV   
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
SDL_AudioDeviceID audio_device;
static display_t *DISPLAY;

bool audio_on = false;
static double audio_phase = 0.0;

void
window_draw()
{
    SDL_Rect inner = {MARGIN, MARGIN, WIN_WIDTH - 2 * MARGIN, WIN_HEIGHT - 2 * MARGIN};

    SDL_FillRect(screenSurface, NULL, CLR_MARGIN);
    SDL_FillRect(screenSurface, &inner, CLR_BORDER);

    for (int x = 0; x < DISPLAY->width; x++) {
        for (int y = 0; y < DISPLAY->height; y++) {
            SDL_Rect pixel = {
                MARGIN + x * PIXEL_SIZE + (x + 1) * BORDER,
                MARGIN + y * PIXEL_SIZE + (y + 1) * BORDER,
                PIXEL_SIZE,
                PIXEL_SIZE
            };
            uint8_t pixel_on = DISPLAY->pixels[y * DISPLAY->width_max + x];
            SDL_FillRect(screenSurface, &pixel, pixel_on ? CLR_PIXEL_ON : CLR_PIXEL_OFF);
        }
    }

    SDL_UpdateWindowSurface(window);
}


bool
window_init(char *rom_name)
{
    DISPLAY = get_display();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 0;
    } else {
        char *window_title = malloc(19 + strlen(rom_name));
        strcpy(window_title, "CHIP-8 Emulator - ");
        strcat(window_title, rom_name);
        window = SDL_CreateWindow(
            window_title,
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            WIN_WIDTH,
            WIN_HEIGHT,
            SDL_WINDOW_SHOWN
        );
        free(window_title);
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
    SDL_CloseAudioDevice(audio_device); // does this throw an error if null?
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void
audio_callback(void *userdata, uint8_t *stream, int len)
{
    int16_t *buf = (int16_t *)stream;
    int samples = len / sizeof(int16_t);

    if (!audio_on) {
        memset(stream, 0, len);
        audio_phase = 0.0;
        return;
    }

    for (int i = 0; i < samples; i++) {
        buf[i] = (int16_t)(AUDIO_AMPLITUDE * sin(audio_phase));
        audio_phase += 2.0 * PI * AUDIO_FREQUENCY / AUDIO_SAMPLE_RATE;
        if (audio_phase > 2.0 * PI)
            audio_phase -= 2.0 * PI;
    }
}

bool audio_init()
{
    SDL_AudioSpec want = {0};
    want.freq = AUDIO_SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    want.callback = audio_callback;

    audio_device = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    if (!audio_device) {
        return 0;
    }

    SDL_PauseAudioDevice(audio_device, 0); // unpause, start playing
    return 1;
}

int
main(int argc, char *args[])
{
    if (argc < 2) {
        printf("Usage: %s <.ch8 program>\n", args[0]);
        return 0;
    }
    if (!load_program(args[1])) {
        printf("Error in CHIP-8 boot sequence\n");
    } else if (!window_init(args[1])) {
        printf("Failed to initialize SDL window\n");
    } else {
        if (!audio_init()) {
            printf("Failed to initialize audio device. Continuing with no sound\n");
        }
        bool quit = false;
        SDL_Event e;
        const double MS_PER_FRAME = 1000.0 / FRAMES_PER_SECOND;

        uint64_t last_frame = SDL_GetTicks64();
        uint64_t now;

        while (!quit) {
            now = SDL_GetTicks64();
            if (now - last_frame >= MS_PER_FRAME) {
                /* 
                each frame: check key events, decrement delay/sound timers, 
                check audio state, perform N instruction cycles, draw the graphics to screen
                */
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
                timer_cycle();
                audio_on = (get_sound_timer() > 0);
                for (int cycle = 0; cycle < INSTRUCTION_CYCLES_PER_FRAME; cycle++) {
                    do_instruction_cycle();
                }
                window_draw();
                last_frame = now;
            }
            SDL_Delay(1); // don't burn out the CPU... :)
        }  
    }

    window_close();

    return 0;
}