#pragma once

#include <SDL2/SDL.h>
int const ROWS= 100;
int const COLS= 100;

typedef struct {
    float center_x, center_y;
    float zoom;
} viewpoint;

typedef struct {
    int start;
    int end;
} VisibleRange;

// Constructor/destructor
bool** construct_grid(int rows, int cols);
void destructor_grid(bool** grid, int rows, int cols);

// Grid operations
void turnCell_withMemory(SDL_Window* window, bool** grid, viewpoint vp, int x, int y, int rows, int cols);
void update_grid(bool** grid, int rows, int cols);

void load_pattern_into_grid(
    const std::string& pattern_name, 
    SDL_Window* window, 
    bool** grid, 
    viewpoint vp, 
    int x, 
    int y, 
    int rows, 
    int cols
);

void load_pattern_into_grid(
    const std::string& pattern_name, 
    bool** grid, 
    int row_pos, 
    int col_pos, 
    int rows, 
    int cols
);

void save_grid_txt(bool** grid, int rows, int cols, const std::string& filename);
void loadGridFromTXT(bool** grid, int rows, int cols, const std::string& filename);
void clear_grid(bool** grid, int rows, int cols);

// Viewport operations
VisibleRange get_visible_rows(const viewpoint* vp, int total_rows, int total_cols);
VisibleRange get_visible_columns(const viewpoint* vp, int total_rows, int total_cols);
void clamp_viewpoint(viewpoint* vp);
void zoomIn(viewpoint* vp, float x, float y, int rows, int cols);
void zoomOut(viewpoint* vp, float x, float y, int rows, int cols);

// Rendering
void print_grid(SDL_Window* window, SDL_Renderer* renderer, bool** grid, viewpoint vp, 
               int rows, int cols);

