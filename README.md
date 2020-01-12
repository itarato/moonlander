```bash
clang -g `sdl2-config --cflags` `sdl2-config --libs` -lSDL2_image -lm -pthread game.c -o game && ./game

```
