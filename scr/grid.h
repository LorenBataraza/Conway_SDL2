#pragma once
#include <SDL2/SDL_ttf.h>
#include <string>
#include <fstream>
using namespace std;


typedef struct {
    float center_x, center_y;
    float zoom;
} viewpoint;

bool** construct_grid(int rows,int col);
void fill_grid(bool** grid_to_fill, bool* given_grid, int rows,int cols);
void destructor_grid(bool** grid, int rows,int col);
void turnCell_withMemory(SDL_Window* window, bool ** grid, viewpoint vp ,int x , int y, int rows, int cols);
void update_grid(bool** grid, int rows, int col) ;
void save_grid_txt(bool** grid, int rows, int cols, const string& filename);
void loadGridFromTXT(bool** grid, int rows, int cols, const string& filename) ;

void print_grid(
    SDL_Window* window,
    SDL_Renderer* renderer,
    bool ** grid,
    viewpoint vp,
    int rows, 
    int cols,
    TTF_Font* font
    );


void zoomOut(viewpoint* vp, float x , float y);
void zoomIn(viewpoint* vp, float x , float y);
