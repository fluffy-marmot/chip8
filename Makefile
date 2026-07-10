CC = gcc
SDL_FLAGS = -lSDL2 -lm
PYGAME_FLAGS = -shared -fPIC

chip8: chip8.c chip8_sdl.c chip8.h
	$(CC) chip8.c chip8_sdl.c $(SDL_FLAGS) -o chip8

chip8.so: chip8.c chip8.h
	$(CC) chip8.c $(PYGAME_FLAGS) -o chip8.so

# aliases
shared: chip8.so
so: chip8.so

clean:
	rm -f chip8 chip8.so