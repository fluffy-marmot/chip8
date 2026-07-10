import array
import ctypes
import math
import pathlib
import sys

import pygame

MARGIN     = 10
BORDER     = 0
PIXEL_SIZE = 18

CLR_MARGIN     = "grey"
CLR_BORDER     = "cyan"
CLR_PIXEL_ON   = "green"
CLR_PIXEL_OFF  = "black"

# Convention: the keys used to activate CHIP-8's 4x4, 16 key, keypad: 1234 QWER ASDF ZXCV   
KEYMAP = {
    pygame.KSCAN_1: 0x1, pygame.KSCAN_2: 0x2, pygame.KSCAN_3: 0x3, pygame.KSCAN_4: 0xC,
    pygame.KSCAN_Q: 0x4, pygame.KSCAN_W: 0x5, pygame.KSCAN_E: 0x6, pygame.KSCAN_R: 0xD,
    pygame.KSCAN_A: 0x7, pygame.KSCAN_S: 0x8, pygame.KSCAN_D: 0x9, pygame.KSCAN_F: 0xE,
    pygame.KSCAN_Z: 0xA, pygame.KSCAN_X: 0x0, pygame.KSCAN_C: 0xB, pygame.KSCAN_V: 0xF,
}


def make_beep(freq:int=440, duration:float=1.0, sample_rate:int=44100, volume:float=0.3):
    n_samples = int(sample_rate * duration)
    amplitude = int(volume * 28000)
    samples = array.array("h")
    for i in range(n_samples):
        t = i / sample_rate
        value = int(amplitude * math.sin(2 * math.pi * freq * t))
        samples.append(value)
        samples.append(value)
    return pygame.mixer.Sound(buffer=samples)


def get_window_dimensions() -> tuple[int, int]:
    display_w = ctypes.c_int.in_dll(CHIP8, "DISPLAY_WIDTH").value
    display_h = ctypes.c_int.in_dll(CHIP8, "DISPLAY_HEIGHT").value
    return (
        2 * MARGIN + PIXEL_SIZE * display_w + BORDER * (display_w + 1),
        2 * MARGIN + PIXEL_SIZE * display_h + BORDER * (display_h + 1)
    )


def window_draw(screen: pygame.Surface, win_w: int, win_h: int) -> None:
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
    global CHIP8
    if len(sys.argv) != 2:
        sys.exit(f"Usage: python {sys.argv[0]} <.ch8 program>")
    elif not pathlib.Path(sys.argv[1]).exists():
        sys.exit(f"Cannot find file {sys.argv[1]}")
    try:
        CHIP8 = ctypes.CDLL("./chip8.so")
        CHIP8.load_program(sys.argv[1].encode())
        win_w, win_h = get_window_dimensions()
    except Exception as e:
        print(e)
        sys.exit(1)

    pygame.mixer.pre_init(frequency=44100, size=-16, channels=2)
    pygame.init()
    screen = pygame.display.set_mode((win_w, win_h))
    beep = make_beep()
    beep.set_volume(0.0)
    beep.play(loops=-1)

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
        
        beep.set_volume(CHIP8.get_sound_timer() > 0)

        window_draw(screen, win_w, win_h)
        pygame.display.flip()

        clock.tick(60)

if __name__ == "__main__":
    main()