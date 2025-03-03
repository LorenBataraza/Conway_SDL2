// Extracted from https://stackoverflow.com/questions/36449616/sdl2-how-to-draw-dotted-line
#include <SDL2/SDL.h>

void DrawDottedLine(SDL_Renderer* renderer, int x0, int y0, int x1, int y1) {
    int dx =  abs(x1-x0), sx = x0<x1 ? 1 : -1;
    int dy = -abs(y1-y0), sy = y0<y1 ? 1 : -1;
    int err = dx+dy, e2;
    int count = 0;
    while (1) {
        if (count < 10) {SDL_RenderDrawPoint(renderer,x0,y0);}
        if (x0==x1 && y0==y1) break;
        e2 = 2*err;
        if (e2 > dy) { err += dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
        count = (count + 1) % 20;
    }
}