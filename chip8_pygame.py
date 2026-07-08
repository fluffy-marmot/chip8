import ctypes
import pygame

lib = ctypes.CDLL("./chip8.so")
lib.display_get.argtypes = [ctypes.c_uint8, ctypes.c_uint8]
lib.display_get.restype = ctypes.c_uint8

lib.do_instruction_cycle.argtypes = [ctypes.c_bool]
lib.do_instruction_cycle.restypes = None


print(lib.add(2, 3))

pygame.init()

screen = pygame.display.set_mode((800, 600))

clock = pygame.Clock()

while True:
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            pygame.quit()
            raise SystemExit
        
    screen.fill("purple")
    pygame.display.flip()

    clock.tick(60)