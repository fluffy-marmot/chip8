## About

A CHIP-8 interpreter, based on [the guide by tobiasvl](https://tobiasvl.github.io/blog/write-a-chip-8-emulator/).

## Files

In [tests/](tests/) there are some test programs from [Timendus' CHIP-8 test
suite](https://github.com/Timendus/chip8-test-suite) that are helpful in making sure everything is behaving.

In [programs/](programs/) there are a few programs and simple game examples.

Note: [space.ch8](programs/space.ch8) game requires using `USE_COSMAC_VIP_SHIFT = false` in chip8.c even
though that's not the standard old school CHIP-8 implementation. This is just an example of how there is some
ambiguity between the exact behavior of some instructions among different implementations of CHIP-8.

## Compiling and Running

Files [chip8.c](chip8.c) and [chip8.h](chip8.h) contain the core implementation of the CHIP-8 interpreter; for
handling the graphics, sound, input, and timings there are two different versions:

### Pure C version

```bash
# this version needs to compile against SDL2
sudo apt install libsdl2-dev
# compile into chip8 executable
make chip8
# run by giving a .ch8 program as a command line arg
./chip8 tests/2-ibm-logo.ch8
```

### Using Pygame wrapper

```bash
# compile into a shared library that the Python code can use
make shared
# pygame-ce is the only pip package needed
pip install pygame-ce
# run by giving a .ch8 program as a command line arg
python chip8_pygame.py tests/2-ibm-logo.ch8
```

## Keypad

By the way, CHIP-8 systems used a 16 key keypad labeled 0-F (hex digits). Use the 4x4
grid of keys on the left of the keyboard: 1234 QWER ASDF ZXCV to interact with programs.