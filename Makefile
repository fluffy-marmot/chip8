CC = gcc
LDFLAGS = -lSDL2 -lm

chip8: chip8.c chip8_sdl.c chip8.h
	$(CC) chip8.c chip8_sdl.c $(LDFLAGS) -o chip8

clean:
	rm -f chip8