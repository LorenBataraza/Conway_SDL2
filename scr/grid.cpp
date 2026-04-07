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
                            int rows, int cols, CellValue player_id) {
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
        for (const auto& [dx, dy] : cells) {
            int px = col + dx - 1;
            int py = row + dy - 1;
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
                            CellValue player_id) {
    if (row_pos < 0 || row_pos >= rows || col_pos < 0 || col_pos >= cols) return;

    if (pattern_exists(pattern_name)) {
        const auto& cells = get_pattern_cells(pattern_name);
        for (const auto& [dx, dy] : cells) {
            int x = col_pos + dx;
            int y = row_pos + dy;
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

// Rendering
void print_grid(SDL_Window* window, SDL_Renderer* renderer, CellValue** grid, 
                viewpoint vp, int rows, int cols) {
    int window_width, window_height;
    SDL_GetWindowSize(window, &window_width, &window_height);

    VisibleRange row_range = get_visible_rows(&vp, rows, cols);
    VisibleRange col_range = get_visible_columns(&vp, rows, cols);

    int visible_rows = row_range.end - row_range.start;
    int visible_cols = col_range.end - col_range.start;

    if (visible_rows <= 0 || visible_cols <= 0) return;

    float cell_width = static_cast<float>(window_width) / visible_cols;
    float cell_height = static_cast<float>(window_height) / visible_rows;

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
