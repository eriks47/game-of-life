#!/bin/sh

set -xe

gcc -ggdb $(pkg-config --libs --cflags sdl2) \
-lSDL2_ttf src/main.c -o build/_SDL_TEST && \
./build/_SDL_TEST
