## About

A CHIP-8 interpreter, based on [the guide by tobiasvl](https://tobiasvl.github.io/blog/write-a-chip-8-emulator/).

## Files

[chip8.c](chip8.c) and [chip8.h](chip8.h) are the emulator functionality and could be used with various implementations that handle the timings and drawing the graphics. 

[chip8_sdl.c](chip8_sdl.c) is one possible implementation in C using SDL2 library to create a window and draw a
representation of the chip's display memory. The functions declared at the bottom of the header file are what
any implementation can use to interact with the state of the "hardware."

There are a few test programs (ROMs) ending with the .ch8 extension

## Compiling and Using

SDL is needed: 
```bash
sudo apt install libsdl2-dev
```
Then:
```bash
gcc chip8.c chip8_sdl.c -lSDL2 -o chip8
```
Run a program with (for example):
```bash
./chip8 programs/space.ch8
```
