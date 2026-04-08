#pragma once

#include <SDL2/SDL.h>
#include <cstdint>
#include <string>

// Constantes de la grilla
inline constexpr int GRID_ROWS = 200;
inline constexpr int GRID_COLS = 400;

// Valor de celda: 0 = muerta, 1-255 = viva (ID del jugador que la creó)
using CellValue = int8_t;
constexpr CellValue CELL_DEAD = 0;

// Colores para jugadores (hasta 8 jugadores distintos)
inline const SDL_Color PLAYER_COLORS[] = {
    {100, 255, 100, 255},  // Jugador 1: Verde claro (local por defecto)
    {100, 100, 255, 255},  // Jugador 2: Azul
    {255, 100, 100, 255},  // Jugador 3: Rojo
    {255, 255, 100, 255},  // Jugador 4: Amarillo
    {255, 100, 255, 255},  // Jugador 5: Magenta
    {100, 255, 255, 255},  // Jugador 6: Cyan
    {255, 165, 0, 255},    // Jugador 7: Naranja
    {200, 200, 200, 255},  // Jugador 8: Gris claro
};
inline constexpr int NUM_PLAYER_COLORS = sizeof(PLAYER_COLORS) / sizeof(PLAYER_COLORS[0]);

// Viewport
typedef struct {
    float center_x, center_y;
    float zoom;
} viewpoint;

typedef struct {
    int start;
    int end;
} VisibleRange;

// Constructor/destructor de grilla
CellValue** construct_grid(int rows, int cols);
void destructor_grid(CellValue** grid, int rows, int cols);

// Operaciones de grilla
void turnCell_withMemory(SDL_Window* window, CellValue** grid, viewpoint vp, 
                         int x, int y, int rows, int cols, CellValue player_id = 1);

void update_grid(CellValue** grid, int rows, int cols);

void load_pattern_into_grid(
    const std::string& pattern_name, 
    SDL_Window* window, 
    CellValue** grid, 
    viewpoint vp, 
    int x, int y, 
    int rows, int cols,
    CellValue player_id = 1
);

void load_pattern_into_grid(
    const std::string& pattern_name, 
    CellValue** grid, 
    int row_pos, int col_pos, 
    int rows, int cols,
    CellValue player_id = 1
);

// I/O
void save_grid_txt(CellValue** grid, int rows, int cols, const std::string& filename);
void loadGridFromTXT(CellValue** grid, int rows, int cols, const std::string& filename);
void clear_grid(CellValue** grid, int rows, int cols);

// Viewport operations
VisibleRange get_visible_rows(const viewpoint* vp, int total_rows, int total_cols);
VisibleRange get_visible_columns(const viewpoint* vp, int total_rows, int total_cols);
void clamp_viewpoint(viewpoint* vp);
void zoomIn(viewpoint* vp, float x, float y, int rows, int cols);
void zoomOut(viewpoint* vp, float x, float y, int rows, int cols);

// Rendering
void print_grid(SDL_Window* window, SDL_Renderer* renderer, CellValue** grid, 
                viewpoint vp, int rows, int cols);

// Minimapa
void render_minimap(SDL_Renderer* renderer, CellValue** grid, 
                    int rows, int cols, viewpoint vp,
                    int minimap_x, int minimap_y, 
                    int minimap_width, int minimap_height);

// Utilidades
inline SDL_Color get_player_color(CellValue player_id) {
    if (player_id <= 0) return {0, 0, 0, 255};
    int index = (player_id - 1) % NUM_PLAYER_COLORS;
    return PLAYER_COLORS[index];
}

inline bool is_zoomed(const viewpoint& vp) {
    return vp.zoom < 0.95f;  // Consideramos zoom si está por debajo del 95%
}
