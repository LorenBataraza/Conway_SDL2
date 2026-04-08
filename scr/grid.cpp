#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <cstring>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "grid.h"
#include "patterns.h"

using namespace std;

// Constructor/destructor
CellValue** construct_grid(int rows, int cols) {
    CellValue** grid = new CellValue*[rows];
    for (int i = 0; i < rows; i++) {
        grid[i] = new CellValue[cols];
        memset(grid[i], CELL_DEAD, cols * sizeof(CellValue));
    }
    return grid;
}

void destructor_grid(CellValue** grid, int rows, int cols) {
    if (!grid) return;
    for (int i = 0; i < rows; i++) {
        delete[] grid[i];
    }
    delete[] grid;
}

void clear_grid(CellValue** grid, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        memset(grid[i], CELL_DEAD, cols * sizeof(CellValue));
    }
}

// Viewport operations
VisibleRange get_visible_rows(const viewpoint* vp, int total_rows, int total_cols) {
    float visible_height = total_rows * vp->zoom;
    int start = static_cast<int>((vp->center_y - vp->zoom / 2) * total_rows);
    int end = static_cast<int>((vp->center_y + vp->zoom / 2) * total_rows);
    
    start = std::max(0, start);
    end = std::min(total_rows, end);
    
    return {start, end};
}

VisibleRange get_visible_columns(const viewpoint* vp, int total_rows, int total_cols) {
    float visible_width = total_cols * vp->zoom;
    int start = static_cast<int>((vp->center_x - vp->zoom / 2) * total_cols);
    int end = static_cast<int>((vp->center_x + vp->zoom / 2) * total_cols);
    
    start = std::max(0, start);
    end = std::min(total_cols, end);
    
    return {start, end};
}

void clamp_viewpoint(viewpoint* vp) {
    float half_zoom = vp->zoom / 2;
    vp->center_x = std::clamp(vp->center_x, half_zoom, 1.0f - half_zoom);
    vp->center_y = std::clamp(vp->center_y, half_zoom, 1.0f - half_zoom);
}

void zoomIn(viewpoint* vp, float x, float y, int rows, int cols) {
    float old_zoom = vp->zoom;
    vp->zoom = std::max(0.05f, vp->zoom * 0.9f);
    
    // Ajustar centro para zoom hacia el cursor
    float zoom_factor = vp->zoom / old_zoom;
    vp->center_x = x + (vp->center_x - x) * zoom_factor;
    vp->center_y = y + (vp->center_y - y) * zoom_factor;
    
    clamp_viewpoint(vp);
}

void zoomOut(viewpoint* vp, float x, float y, int rows, int cols) {
    float old_zoom = vp->zoom;
    vp->zoom = std::min(1.0f, vp->zoom * 1.1f);
    
    float zoom_factor = vp->zoom / old_zoom;
    vp->center_x = x + (vp->center_x - x) * zoom_factor;
    vp->center_y = y + (vp->center_y - y) * zoom_factor;
    
    clamp_viewpoint(vp);
}

// Grid operations
void turnCell_withMemory(SDL_Window* window, CellValue** grid, viewpoint vp, 
                         int x, int y, int rows, int cols, CellValue player_id) {
    static int last_row = -1;
    static int last_col = -1;

    VisibleRange cols_range = get_visible_columns(&vp, rows, cols);
    VisibleRange rows_range = get_visible_rows(&vp, rows, cols);

    int current_width, current_height;
    SDL_GetWindowSize(window, &current_width, &current_height);

    float grid_x = cols_range.start + (x * (cols_range.end - cols_range.start) / static_cast<float>(current_width));
    float grid_y = rows_range.start + (y * (rows_range.end - rows_range.start) / static_cast<float>(current_height));

    int col = static_cast<int>(grid_x);
    int row = static_cast<int>(grid_y);

    if (row == last_row && col == last_col) return;
    if (row < 0 || row >= rows || col < 0 || col >= cols) return;

    grid[row][col] = (grid[row][col] == CELL_DEAD) ? player_id : CELL_DEAD;
    last_row = row;
    last_col = col;
}

void load_pattern_into_grid(const std::string& pattern_name, SDL_Window* window, 
                            CellValue** grid, viewpoint vp, int x, int y, 
                            int rows, int cols, CellValue player_id,
                            bool mirror_h, bool mirror_v) {
    static int last_row = -1;
    static int last_col = -1;

    VisibleRange cols_range = get_visible_columns(&vp, rows, cols);
    VisibleRange rows_range = get_visible_rows(&vp, rows, cols);

    int current_width, current_height;
    SDL_GetWindowSize(window, &current_width, &current_height);

    float grid_x = cols_range.start + (x * (cols_range.end - cols_range.start) / static_cast<float>(current_width));
    float grid_y = rows_range.start + (y * (rows_range.end - rows_range.start) / static_cast<float>(current_height));

    int col = static_cast<int>(grid_x);
    int row = static_cast<int>(grid_y);

    if (row == last_row && col == last_col) return;
    if (row < 0 || row >= rows || col < 0 || col >= cols) return;

    if (pattern_exists(pattern_name)) {
        const auto& cells = get_pattern_cells(pattern_name);
        
        // Calcular bounding box para espejado
        int min_dx = 0, max_dx = 0, min_dy = 0, max_dy = 0;
        for (const auto& [dx, dy] : cells) {
            min_dx = std::min(min_dx, dx);
            max_dx = std::max(max_dx, dx);
            min_dy = std::min(min_dy, dy);
            max_dy = std::max(max_dy, dy);
        }
        
        for (const auto& [dx, dy] : cells) {
            int tx = dx;
            int ty = dy;
            
            // Aplicar espejado
            if (mirror_h) {
                tx = max_dx - (dx - min_dx);
            }
            if (mirror_v) {
                ty = max_dy - (dy - min_dy);
            }
            
            int px = col + tx - 1;
            int py = row + ty - 1;
            if (px >= 0 && px < cols && py >= 0 && py < rows) {
                grid[py][px] = player_id;
            }
        }
    }
    
    last_row = row;
    last_col = col;
}

void load_pattern_into_grid(const std::string& pattern_name, CellValue** grid, 
                            int row_pos, int col_pos, int rows, int cols, 
                            CellValue player_id,
                            bool mirror_h, bool mirror_v) {
    if (row_pos < 0 || row_pos >= rows || col_pos < 0 || col_pos >= cols) return;

    if (pattern_exists(pattern_name)) {
        const auto& cells = get_pattern_cells(pattern_name);
        
        // Calcular bounding box para espejado
        int min_dx = 0, max_dx = 0, min_dy = 0, max_dy = 0;
        for (const auto& [dx, dy] : cells) {
            min_dx = std::min(min_dx, dx);
            max_dx = std::max(max_dx, dx);
            min_dy = std::min(min_dy, dy);
            max_dy = std::max(max_dy, dy);
        }
        
        for (const auto& [dx, dy] : cells) {
            int tx = dx;
            int ty = dy;
            
            // Aplicar espejado
            if (mirror_h) {
                tx = max_dx - (dx - min_dx);
            }
            if (mirror_v) {
                ty = max_dy - (dy - min_dy);
            }
            
            int x = col_pos + tx;
            int y = row_pos + ty;
            if (x >= 0 && x < cols && y >= 0 && y < rows) {
                grid[y][x] = player_id;
            }
        }
    }
}

void update_grid(CellValue** grid, int rows, int cols) {
    // Matriz para contar vecinos
    int** neighbors = new int*[rows];
    for (int i = 0; i < rows; i++) {
        neighbors[i] = new int[cols]();
    }

    // Contar vecinos para cada celda
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            int count = 0;
            for (int di = -1; di <= 1; di++) {
                for (int dj = -1; dj <= 1; dj++) {
                    if (di == 0 && dj == 0) continue;
                    int ni = i + di;
                    int nj = j + dj;
                    if (ni >= 0 && ni < rows && nj >= 0 && nj < cols) {
                        if (grid[ni][nj] != CELL_DEAD) count++;
                    }
                }
            }
            neighbors[i][j] = count;
        }
    }

    // Nueva grilla
    CellValue** new_grid = new CellValue*[rows];
    for (int i = 0; i < rows; i++) {
        new_grid[i] = new CellValue[cols];
    }

    // Aplicar reglas de Conway
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (grid[i][j] != CELL_DEAD) {
                // Celda viva: sobrevive con 2 o 3 vecinos
                new_grid[i][j] = (neighbors[i][j] == 2 || neighbors[i][j] == 3) 
                                 ? grid[i][j] : CELL_DEAD;
            } else {
                // Celda muerta: nace con exactamente 3 vecinos
                if (neighbors[i][j] == 3) {
                    // Hereda el color del vecino más común
                    int player_count[NUM_PLAYER_COLORS + 1] = {0};
                    for (int di = -1; di <= 1; di++) {
                        for (int dj = -1; dj <= 1; dj++) {
                            if (di == 0 && dj == 0) continue;
                            int ni = i + di;
                            int nj = j + dj;
                            if (ni >= 0 && ni < rows && nj >= 0 && nj < cols) {
                                CellValue v = grid[ni][nj];
                                if (v > 0 && v <= NUM_PLAYER_COLORS) {
                                    player_count[v]++;
                                }
                            }
                        }
                    }
                    // Encontrar el jugador con más celdas vecinas
                    CellValue dominant = 1;
                    int max_count = 0;
                    for (int p = 1; p <= NUM_PLAYER_COLORS; p++) {
                        if (player_count[p] > max_count) {
                            max_count = player_count[p];
                            dominant = p;
                        }
                    }
                    new_grid[i][j] = dominant;
                } else {
                    new_grid[i][j] = CELL_DEAD;
                }
            }
        }
    }

    // Copiar resultado
    for (int i = 0; i < rows; i++) {
        memcpy(grid[i], new_grid[i], cols * sizeof(CellValue));
    }

    // Liberar memoria
    for (int i = 0; i < rows; i++) {
        delete[] neighbors[i];
        delete[] new_grid[i];
    }
    delete[] neighbors;
    delete[] new_grid;
}

// Rendering con zonas de spawn
void print_grid_with_zones(SDL_Window* window, SDL_Renderer* renderer, CellValue** grid, 
                           viewpoint vp, int rows, int cols,
                           int local_player_id, int num_players, bool competition_mode) {
    int window_width, window_height;
    SDL_GetWindowSize(window, &window_width, &window_height);

    VisibleRange row_range = get_visible_rows(&vp, rows, cols);
    VisibleRange col_range = get_visible_columns(&vp, rows, cols);

    int visible_rows = row_range.end - row_range.start;
    int visible_cols = col_range.end - col_range.start;

    if (visible_rows <= 0 || visible_cols <= 0) return;

    float cell_width = static_cast<float>(window_width) / visible_cols;
    float cell_height = static_cast<float>(window_height) / visible_rows;

    // Dibujar zonas de spawn con tinte (solo en modo competición con jugadores válidos)
    if (competition_mode && is_valid_player_count(num_players)) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        
        for (int player = 1; player <= num_players; player++) {
            SpawnZone zone = get_spawn_zone(player, num_players, rows, cols);
            SDL_Color player_color = get_player_color(player);
            
            // Calcular intersección de la zona con el área visible
            int zone_start_row = std::max(zone.start_row, row_range.start);
            int zone_end_row = std::min(zone.end_row, row_range.end);
            int zone_start_col = std::max(zone.start_col, col_range.start);
            int zone_end_col = std::min(zone.end_col, col_range.end);
            
            if (zone_start_row < zone_end_row && zone_start_col < zone_end_col) {
                // Alpha más alto para la zona del jugador local
                Uint8 alpha = (player == local_player_id) ? 35 : 20;
                SDL_SetRenderDrawColor(renderer, player_color.r, player_color.g, player_color.b, alpha);
                
                SDL_FRect zone_rect;
                zone_rect.x = (zone_start_col - col_range.start) * cell_width;
                zone_rect.y = (zone_start_row - row_range.start) * cell_height;
                zone_rect.w = (zone_end_col - zone_start_col) * cell_width;
                zone_rect.h = (zone_end_row - zone_start_row) * cell_height;
                
                SDL_RenderFillRectF(renderer, &zone_rect);
            }
        }
        
        // Dibujar bordes de zonas
        SDL_SetRenderDrawColor(renderer, 80, 80, 80, 100);
        for (int player = 1; player <= num_players; player++) {
            SpawnZone zone = get_spawn_zone(player, num_players, rows, cols);
            
            // Calcular posición en pantalla
            float x1 = (zone.start_col - col_range.start) * cell_width;
            float y1 = (zone.start_row - row_range.start) * cell_height;
            float x2 = (zone.end_col - col_range.start) * cell_width;
            float y2 = (zone.end_row - row_range.start) * cell_height;
            
            // Solo dibujar si está visible
            if (x2 > 0 && x1 < window_width && y2 > 0 && y1 < window_height) {
                SDL_RenderDrawLineF(renderer, x1, y1, x2, y1);  // Top
                SDL_RenderDrawLineF(renderer, x1, y2, x2, y2);  // Bottom
                SDL_RenderDrawLineF(renderer, x1, y1, x1, y2);  // Left
                SDL_RenderDrawLineF(renderer, x2, y1, x2, y2);  // Right
            }
        }
    }

    // Dibujar celdas vivas
    for (int i = row_range.start; i < row_range.end; i++) {
        for (int j = col_range.start; j < col_range.end; j++) {
            if (grid[i][j] != CELL_DEAD) {
                SDL_Color color = get_player_color(grid[i][j]);
                SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

                SDL_FRect rect;
                rect.x = (j - col_range.start) * cell_width;
                rect.y = (i - row_range.start) * cell_height;
                rect.w = cell_width;
                rect.h = cell_height;

                SDL_RenderFillRectF(renderer, &rect);
            }
        }
    }

    // Dibujar líneas de grilla (solo si las celdas son suficientemente grandes)
    if (cell_width > 4 && cell_height > 4) {
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
        
        for (int i = 0; i <= visible_rows; i++) {
            int y = static_cast<int>(i * cell_height);
            SDL_RenderDrawLine(renderer, 0, y, window_width, y);
        }
        
        for (int j = 0; j <= visible_cols; j++) {
            int x = static_cast<int>(j * cell_width);
            SDL_RenderDrawLine(renderer, x, 0, x, window_height);
        }
    }
}

// Rendering (versión simple sin zonas)
void print_grid(SDL_Window* window, SDL_Renderer* renderer, CellValue** grid, 
                viewpoint vp, int rows, int cols) {
    print_grid_with_zones(window, renderer, grid, vp, rows, cols, 0, 0, false);
}

// Minimapa
void render_minimap(SDL_Renderer* renderer, CellValue** grid, 
                    int rows, int cols, viewpoint vp,
                    int minimap_x, int minimap_y, 
                    int minimap_width, int minimap_height) {
    // Fondo del minimapa
    SDL_Rect bg = {minimap_x - 2, minimap_y - 2, minimap_width + 4, minimap_height + 4};
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 200);
    SDL_RenderFillRect(renderer, &bg);
    
    // Borde
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderDrawRect(renderer, &bg);

    // Escala de celdas en el minimapa
    float scale_x = static_cast<float>(minimap_width) / cols;
    float scale_y = static_cast<float>(minimap_height) / rows;

    // Dibujar celdas (agrupadas para rendimiento)
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (grid[i][j] != CELL_DEAD) {
                SDL_Color color = get_player_color(grid[i][j]);
                SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 200);
                
                SDL_Rect cell;
                cell.x = minimap_x + static_cast<int>(j * scale_x);
                cell.y = minimap_y + static_cast<int>(i * scale_y);
                cell.w = std::max(1, static_cast<int>(scale_x));
                cell.h = std::max(1, static_cast<int>(scale_y));
                
                SDL_RenderFillRect(renderer, &cell);
            }
        }
    }

    // Dibujar viewport actual (rectángulo del área visible)
    VisibleRange row_range = get_visible_rows(&vp, rows, cols);
    VisibleRange col_range = get_visible_columns(&vp, rows, cols);
    
    SDL_Rect viewport_rect;
    viewport_rect.x = minimap_x + static_cast<int>(col_range.start * scale_x);
    viewport_rect.y = minimap_y + static_cast<int>(row_range.start * scale_y);
    viewport_rect.w = static_cast<int>((col_range.end - col_range.start) * scale_x);
    viewport_rect.h = static_cast<int>((row_range.end - row_range.start) * scale_y);
    
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 180);
    SDL_RenderDrawRect(renderer, &viewport_rect);
}

// I/O
void save_grid_txt(CellValue** grid, int rows, int cols, const string& filename) {
    ofstream file(filename);
    if (!file.is_open()) {
        cerr << "Error al abrir archivo para escritura: " << filename << endl;
        return;
    }
    
    file << rows << " " << cols << endl;
    
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            file << static_cast<int>(grid[i][j]) << " ";
        }
        file << endl;
    }
    
    file.close();
    cout << "Grilla guardada en: " << filename << endl;
}

void loadGridFromTXT(CellValue** grid, int rows, int cols, const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error al abrir archivo: " << filename << endl;
        return;
    }

    int file_rows, file_cols;
    file >> file_rows >> file_cols;
    
    if (file_rows != rows || file_cols != cols) {
        cerr << "Dimensiones no coinciden" << endl;
        file.close();
        return;
    }

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            int value;
            file >> value;
            grid[i][j] = static_cast<CellValue>(value);
        }
    }

    file.close();
    cout << "Grilla cargada desde: " << filename << endl;
}
