import ctypes
import sys

import pygame
from pygame import Surface

MARGIN     = 10
BORDER     = 0
PIXEL_SIZE = 18

CLR_MARGIN     = "grey"
CLR_BORDER     = "cyan"
CLR_PIXEL_ON   = "green"
CLR_PIXEL_OFF  = "black"

FRAMES_PER_SECOND = 60;

KEYMAP = {
    pygame.KSCAN_1: 0x1, pygame.KSCAN_2: 0x2, pygame.KSCAN_3: 0x3, pygame.KSCAN_4: 0xC,
    pygame.KSCAN_Q: 0x4, pygame.KSCAN_W: 0x5, pygame.KSCAN_E: 0x6, pygame.KSCAN_R: 0xD,
    pygame.KSCAN_A: 0x7, pygame.KSCAN_S: 0x8, pygame.KSCAN_D: 0x9, pygame.KSCAN_F: 0xE,
    pygame.KSCAN_Z: 0xA, pygame.KSCAN_X: 0x0, pygame.KSCAN_C: 0xB, pygame.KSCAN_V: 0xF,
}

def load_chip8_lib() -> None:
    global CHIP8
    CHIP8 = ctypes.CDLL("./chip8.so")
    CHIP8.display_get.argtypes = [ctypes.c_uint8, ctypes.c_uint8]
    CHIP8.display_get.restype = ctypes.c_uint8
    CHIP8.do_instruction_cycle.argtypes = None
    CHIP8.do_instruction_cycle.restype = ctypes.c_bool
    CHIP8.load_program.argtypes = [ctypes.c_char_p]
    CHIP8.load_program.restype = ctypes.c_bool
    CHIP8.keypad_set.argtypes = [ctypes.c_uint8, ctypes.c_bool]
    CHIP8.keypad_set.restype = None
    CHIP8.timer_cycle.restype = None
    CHIP8.get_sound_timer.restype = ctypes.c_uint8


def get_window_dimensions() -> tuple[int, int]:
    display_w = ctypes.c_int.in_dll(CHIP8, "DISPLAY_WIDTH").value
    display_h = ctypes.c_int.in_dll(CHIP8, "DISPLAY_HEIGHT").value
    return (
        2 * MARGIN + PIXEL_SIZE * display_w + BORDER * (display_w + 1),
        2 * MARGIN + PIXEL_SIZE * display_h + BORDER * (display_h + 1)
    )


def window_draw(screen: Surface, win_w: int, win_h: int) -> None:
    pygame.draw.rect(screen, CLR_MARGIN, (0, 0, win_w, win_h))
    pygame.draw.rect(screen, CLR_BORDER, (MARGIN, MARGIN, win_w - 2 * MARGIN, win_h - 2 * MARGIN))

    for x in range(ctypes.c_int.in_dll(CHIP8, "DISPLAY_WIDTH").value):
        for y in range(ctypes.c_int.in_dll(CHIP8, "DISPLAY_HEIGHT").value):
            pygame.draw.rect(
                screen,
                CLR_PIXEL_ON if CHIP8.display_get(x, y) else CLR_PIXEL_OFF,
                (
                    MARGIN + x * PIXEL_SIZE + (x + 1) * BORDER,
                    MARGIN + y * PIXEL_SIZE + (y + 1) * BORDER,
                    PIXEL_SIZE,
                    PIXEL_SIZE
                )
            )
    

def main() -> None:
    if len(sys.argv) != 2:
        print(f"Usage: python {sys.argv[0]} <.ch8 program>")
    try:
        load_chip8_lib()
        CHIP8.load_program(sys.argv[1].encode())
        win_w, win_h = get_window_dimensions()
    except Exception as e:
        print(e)
        sys.exit(1)

    pygame.init()
    screen = pygame.display.set_mode((win_w, win_h))
    pygame.display.set_caption(f"CHIP-8 Emulator - {sys.argv[1]}")
    clock = pygame.Clock()
    while True:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                pygame.quit()
                raise SystemExit
            elif event.type in [pygame.KEYDOWN, pygame.KEYUP] and event.scancode in KEYMAP:
                CHIP8.keypad_set(KEYMAP[event.scancode], event.type == pygame.KEYDOWN)
            
        CHIP8.timer_cycle()
        for _ in range(ctypes.c_int.in_dll(CHIP8, "INSTRUCTION_CYCLES_PER_FRAME").value):
            CHIP8.do_instruction_cycle()
            
        window_draw(screen, win_w, win_h)
        pygame.display.flip()

        clock.tick(60)

if __name__ == "__main__":
    main()