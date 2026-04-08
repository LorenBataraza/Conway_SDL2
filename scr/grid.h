#pragma once

#include <SDL2/SDL.h>
#include <cstdint>
#include <string>

// Constantes de la grilla
inline constexpr int GRID_ROWS = 200;
inline constexpr int GRID_COLS = 400;

// ==================== ZONAS DE SPAWN ====================

struct SpawnZone {
    int start_row, end_row;
    int start_col, end_col;
    
    bool contains(int row, int col) const {
        return row >= start_row && row < end_row &&
               col >= start_col && col < end_col;
    }
};

// Calcula la zona de spawn para un jugador dado el número total de jugadores
// Retorna zona vacía si el número de jugadores no es válido (debe ser 2,4,6,8)
inline SpawnZone get_spawn_zone(int player_id, int num_players, int rows, int cols) {
    SpawnZone zone = {0, 0, 0, 0};
    
    // Solo permitir 2, 4, 6, 8 jugadores
    if (num_players != 2 && num_players != 4 && num_players != 6 && num_players != 8) {
        // Modo libre: toda la grilla
        zone.start_row = 0;
        zone.end_row = rows;
        zone.start_col = 0;
        zone.end_col = cols;
        return zone;
    }
    
    // Ajustar player_id a índice 0-based
    int idx = player_id - 1;
    if (idx < 0 || idx >= num_players) {
        return zone;  // Zona inválida
    }
    
    switch (num_players) {
        case 2: {
            // Mitad-Mitad (división vertical)
            // Jugador 1: izquierda, Jugador 2: derecha
            int half_col = cols / 2;
            zone.start_row = 0;
            zone.end_row = rows;
            if (idx == 0) {
                zone.start_col = 0;
                zone.end_col = half_col;
            } else {
                zone.start_col = half_col;
                zone.end_col = cols;
            }
            break;
        }
        case 4: {
            // Cuadrilla 2x2
            // [0][1]
            // [2][3]
            int half_row = rows / 2;
            int half_col = cols / 2;
            int grid_row = idx / 2;
            int grid_col = idx % 2;
            zone.start_row = grid_row * half_row;
            zone.end_row = (grid_row + 1) * half_row;
            zone.start_col = grid_col * half_col;
            zone.end_col = (grid_col + 1) * half_col;
            break;
        }
        case 6: {
            // Cuadrilla 3x2 (3 columnas, 2 filas)
            // [0][1][2]
            // [3][4][5]
            int half_row = rows / 2;
            int third_col = cols / 3;
            int grid_row = idx / 3;
            int grid_col = idx % 3;
            zone.start_row = grid_row * half_row;
            zone.end_row = (grid_row + 1) * half_row;
            zone.start_col = grid_col * third_col;
            zone.end_col = (grid_col + 1) * third_col;
            break;
        }
        case 8: {
            // Cuadrilla 4x2 (4 columnas, 2 filas)
            // [0][1][2][3]
            // [4][5][6][7]
            int half_row = rows / 2;
            int quarter_col = cols / 4;
            int grid_row = idx / 4;
            int grid_col = idx % 4;
            zone.start_row = grid_row * half_row;
            zone.end_row = (grid_row + 1) * half_row;
            zone.start_col = grid_col * quarter_col;
            zone.end_col = (grid_col + 1) * quarter_col;
            break;
        }
    }
    
    return zone;
}

// Verifica si el número de jugadores es válido para modo competición con zonas
inline bool is_valid_player_count(int num_players) {
    return num_players == 2 || num_players == 4 || num_players == 6 || num_players == 8;
}

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
    CellValue player_id = 1,
    bool mirror_h = false,
    bool mirror_v = false
);

void load_pattern_into_grid(
    const std::string& pattern_name, 
    CellValue** grid, 
    int row_pos, int col_pos, 
    int rows, int cols,
    CellValue player_id = 1,
    bool mirror_h = false,
    bool mirror_v = false
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

// Rendering con zonas de spawn (para modo competición)
void print_grid_with_zones(SDL_Window* window, SDL_Renderer* renderer, CellValue** grid, 
                           viewpoint vp, int rows, int cols,
                           int local_player_id, int num_players, bool competition_mode);

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
