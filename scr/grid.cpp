#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <unordered_map>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "dotted_line.h"
#include "grid.h"

#define SHOW_NUMBERS 0

using namespace std;

static const std::unordered_map<std::string, std::vector<std::pair<int, int>>> patterns = {
    // Still Life
    {"point", {{0,0}}},
    {"block",    {{0,0}, {1,0}, {0,1}, {1,1}}},
    {"beehive",  {{1,0}, {2,0}, {0,1}, {3,1}, {1,2}, {2,2}}},
    {"loaf",     {{1,0}, {2,0}, {0,1}, {3,1}, {1,2}, {3,2}, {2,3}}},
    {"boat",     {{0,0}, {1,0}, {0,1}, {2,1}, {1,2}}},
    {"flower",   {{1,0}, {0,1}, {2,1}, {1,2}}},
    // Oscilators
    {"blinker",  {{0,0}, {1,0}, {2,0}}},  // Línea horizontal (período 2)
    {"toad",     {{1,0}, {2,0}, {3,0}, {0,1}, {1,1}, {2,1}}},  // Sapo (período 2)
    {"beacon",   {{0,0}, {1,0}, {0,1}, {3,2}, {2,3}, {3,3}}},  // Baliza (período 2)
    {"pulsar",   // Pulsar (período 3)
        {{2,0}, {3,0}, {4,0}, {8,0}, {9,0}, {10,0},
         {0,2}, {5,2}, {7,2}, {12,2},
         {0,3}, {5,3}, {7,3}, {12,3},
         {0,4}, {5,4}, {7,4}, {12,4},
         {2,5}, {3,5}, {4,5}, {8,5}, {9,5}, {10,5},
         {2,7}, {3,7}, {4,7}, {8,7}, {9,7}, {10,7},
         {0,8}, {5,8}, {7,8}, {12,8},
         {0,9}, {5,9}, {7,9}, {12,9},
         {0,10}, {5,10}, {7,10}, {12,10},
         {2,12}, {3,12}, {4,12}, {8,12}, {9,12}, {10,12}}},
    // Spaceships
    {"glider",   {{0,1}, {1,2}, {2,0}, {2,1}, {2,2}}},
    {"lwss",     {{0,0}, {3,0}, {4,1}, {0,2}, {4,2}, {1,3}, {2,3}, {3,3}, {4,3}}},
    {"mwss",     // Nave media (Medium Spaceship)
        {{0,0}, {4,0}, {5,1}, {0,2}, {5,2}, {1,3}, {2,3}, {3,3}, {4,3}, {5,3}}},
    {"hwss",     // Nave pesada (Heavy Spaceship)
        {{0,0}, {5,0}, {6,1}, {0,2}, {6,2}, {1,3}, {2,3}, {3,3}, {4,3}, {5,3}, {6,3}}},
    
        {"glider_gun", {
            // Cuadrado izquierdo (generador)
            {0,4}, {0,5}, {1,4}, {1,5},
        
            // Cuadrado derecho (generador)
            {34,2}, {34,3}, {35,2}, {35,3},
        
            // Estructura central
            {10,4}, {10,5}, {10,6},
            {11,3}, {11,7},
            {12,2}, {12,8},
            {13,2}, {13,8},
            {14,5},
            {15,3}, {15,7},
            {16,4}, {16,5}, {16,6},
            {17,5},
        
            // Parte inferior
            {20,2}, {20,3}, {20,4},
            {21,2}, {21,3}, {21,4},
            {22,1}, {22,5},
            {24,0}, {24,1}, {24,5}, {24,6}
        }}

};

// Constructor/destructor
bool** construct_grid(int rows, int cols) {
    bool** grid = new bool*[rows];
    for (int i = 0; i < rows; i++) {
        grid[i] = new bool[cols]{false};
    }
    return grid;
}

void destructor_grid(bool** grid, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        delete[] grid[i];
    }
    delete[] grid;
}

// Grid operations
void turnCell_withMemory(SDL_Window* window, bool** grid, viewpoint vp, int x, int y, int rows, int cols) {
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

    grid[row][col] = !grid[row][col];
    last_row = row;
    last_col = col;
}

void load_pattern_into_grid(const std::string& pattern_name, SDL_Window* window, bool** grid, viewpoint vp, int x, int y, int rows, int cols) {

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

    if (patterns.find(pattern_name) != patterns.end()) {
        for (auto [dx, dy] : patterns.at(pattern_name)) {
            int x = col + dx - 1;  // Ajuste para centrar
            int y = row + dy - 1;
            if (x >= 0 && x < cols && y >= 0 && y < rows) {
                grid[y][x] = true;
            }
        }
    }
}

void load_pattern_into_grid(const std::string& pattern_name, bool** grid, int row_pos, int col_pos, int rows, int cols){
    static int last_row = -1;
    static int last_col = -1;

    if (row_pos == last_row && col_pos == last_col) return;
    if (row_pos < 0 || row_pos >= rows || col_pos < 0 || col_pos >= cols) return;

    if (patterns.find(pattern_name) != patterns.end()) {
        for (auto [dx, dy] : patterns.at(pattern_name)) {
            int x = col_pos + dx - 1;  // Ajuste para centrar
            int y = row_pos + dy - 1;
            if (x >= 0 && x < cols && y >= 0 && y < rows) {
                grid[y][x] = true;
            }
        }
    }
}


void update_grid(bool** grid, int rows, int col) {
    // Crear matriz para almacenar los valores de los vecinos
    int** valores = new int*[rows];
    for (int i = 0; i < rows; i++) {
        valores[i] = new int[col](); // Inicializa a 0
    }

    // Contar vecinos para cada celda
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < col; j++) {
            int contador_local = 0;

            // Contar los 8 vecinos
            for (int x = -1; x <= 1; x++) {
                for (int y = -1; y <= 1; y++) {
                    if (x == 0 && y == 0) continue; // Ignorar la celda actual
                    if (i + x >= 0 && i + x < rows && j + y >= 0 && j + y < col) {
                        if (grid[i + x][j + y]) contador_local++;
                    }
                }
            }

            valores[i][j] = contador_local;
        }
    }

    // Crear una nueva grid para almacenar el siguiente estado
    bool** nueva_grid = new bool*[rows];
    for (int i = 0; i < rows; i++) {
        nueva_grid[i] = new bool[col];
    }

    // Calcular el nuevo estado
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < col; j++) {
            if (grid[i][j]) {
                // Celda viva
                nueva_grid[i][j] = (valores[i][j] == 2 || valores[i][j] == 3);
            } else {
                // Celda muerta
                nueva_grid[i][j] = (valores[i][j] == 3);
            }
        }
    }

    // Copiar nueva_grid a grid
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < col; j++) {
            grid[i][j] = nueva_grid[i][j];
        }
    }

    // Liberar memoria
    for (int i = 0; i < rows; i++) {
        delete[] valores[i];
        delete[] nueva_grid[i];
    }
    delete[] valores;
    delete[] nueva_grid;
}

void save_grid_txt(bool** grid, int rows, int cols, const string& filename){
    ofstream archivo(filename);
    if (!archivo.is_open()) {
        std::cerr << "Error al abrir el archivo." << std::endl;
        return;
    }
    
    archivo << rows << " " << cols << endl;

    // Escribir la matriz en el archivo
    for (int i = 0; i < cols; i++) {
        for (int j = 0; j < rows; j++) {
            archivo << grid[i][j]<< " ";
        }
        archivo << std::endl;
    }

    archivo.close();
    std::cout << "Exported Grid Sucessfully" << std::endl;
} 

void loadGridFromTXT(bool** grid, int rows, int cols, const string& filename) {
    ifstream file(filename);
       if (!file.is_open()) {
        cerr << "Error: No se pudo abrir el archivo " << filename << endl;
        return;
    }

    // Read first line 
    int file_rows, file_cols;
    file >> file_rows >> file_cols;
    
    // Verificar que las dimensiones coincidan
    if (file_rows != rows || file_cols != cols) {
        cerr << "Error: Las dimensiones del archivo no coinciden con la matriz." << endl;
        return;
    }

    int val;
    for (int i = 0; i < cols; i++) {
        for (int j = 0; j < rows; j++) {
            file >> val;
            grid[i][j] = (bool) val; 
        }
    }
}


void clear_grid(bool** grid, int rows, int cols) {
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            grid[i][j] = false;
        }
    }
}

// Viewport calculations
VisibleRange get_visible_rows(const viewpoint* vp, int total_rows, int total_cols) {
    VisibleRange range;
    float y_min = (vp->center_y - vp->zoom * 0.5f) * total_rows;
    float y_max = (vp->center_y + vp->zoom * 0.5f) * total_rows;
    
    range.start = max(0, static_cast<int>(floor(y_min)));
    range.end = min(total_rows - 1, static_cast<int>(ceil(y_max)));
    
    return range;
}

VisibleRange get_visible_columns(const viewpoint* vp, int total_rows, int total_cols) {
    VisibleRange range;
    float x_min = (vp->center_x - vp->zoom * 0.5f) * total_cols;
    float x_max = (vp->center_x + vp->zoom * 0.5f) * total_cols;
    
    range.start = max(0, static_cast<int>(floor(x_min)));
    range.end = min(total_cols - 1, static_cast<int>(ceil(x_max)));
    
    return range;
}

void clamp_viewpoint(viewpoint* vp) {
    vp->zoom = std::clamp(vp->zoom, 0.1f, 1.0f);
    
    const float half_zoom = vp->zoom * 0.5f;
    vp->center_x = std::clamp(vp->center_x, half_zoom, 1.0f - half_zoom);
    vp->center_y = std::clamp(vp->center_y, half_zoom, 1.0f - half_zoom);
}

void zoomIn(viewpoint* vp, float x, float y, int rows, int cols) {
    vp->center_x += (x - 0.5f) * vp->zoom;
    vp->center_y += (y - 0.5f) * vp->zoom;
    vp->zoom = std::max(vp->zoom * 0.5f, 0.1f);
    clamp_viewpoint(vp);
}

void zoomOut(viewpoint* vp, float x, float y, int rows, int cols) {
    vp->center_x += (x - 0.5f) * vp->zoom;
    vp->center_y += (y - 0.5f) * vp->zoom;
    vp->zoom = std::min(vp->zoom * 1.5f, 1.0f);
    clamp_viewpoint(vp);
}

// Rendering
void print_grid(SDL_Window* window, SDL_Renderer* renderer, bool** grid, viewpoint vp,
               int rows, int cols) {
    
    VisibleRange rows_range = get_visible_rows(&vp, rows, cols);
    VisibleRange cols_range = get_visible_columns(&vp, rows, cols);

    int current_width, current_height;
    SDL_GetWindowSize(window, &current_width, &current_height);

    // Dibujar líneas
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 50);
    
    // Cálculo de paso para líneas
    float x_step = current_width / static_cast<float>(cols_range.end - cols_range.start);
    float y_step = current_height / static_cast<float>(rows_range.end - rows_range.start);

    // Dibujar líneas verticales
    for (int i = cols_range.start; i <= cols_range.end; ++i) {
        float x = (i - cols_range.start) * x_step;
        DrawDottedLine(renderer, x, 0, x, current_height);
    }

    // Dibujar líneas horizontales
    for (int i = rows_range.start; i <= rows_range.end; ++i) {
        float y = (i - rows_range.start) * y_step;
        DrawDottedLine(renderer, 0, y, current_width, y);
    }

    // Dibujar celdas vivas
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
    const int padding = 1;
    
    for (int row = rows_range.start; row <= rows_range.end; ++row) {
        for (int col = cols_range.start; col <= cols_range.end; ++col) {
            if (grid[row][col]) {
                float x = (col - cols_range.start) * x_step + padding;
                float y = (row - rows_range.start) * y_step + padding;
                float w = x_step - 2 * padding;
                float h = y_step - 2 * padding;
                
                SDL_FRect rect = {x, y, w, h};
                SDL_RenderFillRectF(renderer, &rect);
            }
        }
    }
}


