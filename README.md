```bash
cc -Wall game.c -o game -lm `sdl2-config --cflags` `sdl2-config --libs` -lSDL2_image -pthread && ./game
```
