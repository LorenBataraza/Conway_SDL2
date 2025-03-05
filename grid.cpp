#include <fstream>
#include <iostream>
#include <SDL2/SDL_ttf.h>

#include "dotted_line.cpp"

using namespace std;

typedef struct {
    float center_x, center_y;
    float zoom;
} viewpoint;

bool show_numbers=false;

// Constructors
// Emplty Grid
bool** construct_grid(int rows,int col){
    bool** grid = new bool*[rows];
    for (int i = 0; i < rows; i++) {
		grid[i] = new bool[col];
		for (int j = 0; j < col; j++) {
			grid[i][j] = false;  
		}
	}
    return grid;
}


void fill_grid(bool** grid_to_fill, bool* given_grid, int rows,int cols){
    for (int i = 0; i < rows; i++) {
		grid_to_fill[i] = new bool[cols];
		for (int j = 0; j < cols; j++) {
			grid_to_fill[i][j] = given_grid[j+i*cols];  
		}
	}
}

/*
bool** construct_grid(bool* InitCond, int rows,int col){
    bool** contructed_grid = construct_grid(rows, col);
    fill_grid(contructed_grid, InitCond, rows, col );
    return contructed_grid;
}
*/

// Free memory
void destructor_grid(bool** grid, int rows,int col){
		for (int i = 0; i < rows; i++) {
			delete[] grid[i];
		}
        delete[] grid;
}

// Actions on Grid 
void turnCell_withMemory(SDL_Window* window, bool ** grid, viewpoint vp ,int x , int y, int rows, int cols){
    static int last_row = -1;
    static int last_col = -1;

    float x_min = max(0.0f,(vp.center_x - vp.zoom*0.5f)*cols);
    float y_min = max(0.0f,(vp.center_y - vp.zoom*0.5f)*rows);
    float x_max = min((float) cols, (vp.center_x + vp.zoom*0.5f)*cols);
    float y_max = min((float) rows, (vp.center_y + vp.zoom*0.5f)*rows);

    // cout << x_min << " " << y_min << endl;

    int current_width, current_height;
    SDL_GetWindowSize(window, &current_width, &current_height);

    int col = x_min + x*1.0*(x_max-x_min)/current_width;
    int row = y_min + y*1.0*(y_max-y_min)/current_height;
    
    // cout << "r: " << x*1.0*(x_max-x_min)/current_width << " c: "<<y*1.0*(y_max-y_min)/current_height << endl;

    if(row== last_row && col==last_col)return;

    grid[row][col] = !grid[row][col];
    last_row =row;
    last_col=col;
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

// Load Grid from TXT file
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


// Print
void print_grid(
    SDL_Window* window,
    SDL_Renderer* renderer,
    bool ** grid,
    viewpoint vp,
    int rows, 
    int cols,
    TTF_Font* font
    ){
        // Dimensions of current viewpoint
        // Handles points outside frame
        float x_min = max(0.0f,(vp.center_x - vp.zoom*0.5f)*cols);
        float y_min = max(0.0f,(vp.center_y - vp.zoom*0.5f)*rows);
        float x_max = min((float) cols, (vp.center_x + vp.zoom*0.5f)*cols);
        float y_max = min((float) rows, (vp.center_y + vp.zoom*0.5f)*rows);    
        
        // Determine squares to evaluate 
        int min_row = y_min;
        int max_row = y_max;
        int min_col = x_min;
        int max_col = x_max;
        
        int delta_rows = max_row-min_row;
        int delta_cols = max_col-min_col;

        // Debug 
        // cout << viewpoint.center_x << " "<< viewpoint.center_y << " " <<viewpoint.zoom << endl;
        // cout << x_min << " "<< x_max << " "<< y_min << " " << y_max << endl;
        //cout << min_row << " "<< max_row << " "<< min_col << " " << max_col << endl;
        
        int current_width, current_height;
        SDL_GetWindowSize(window, &current_width, &current_height);

        SDL_SetRenderDrawColor( renderer, 255, 255, 255, 50 );

        float x_offset = (x_min - (int)x_min) * (current_width / (max_col - min_col));
        float y_offset = (y_min - (int)y_min) * (current_height / (max_row - min_row));

        for (size_t i = 0; i < delta_rows; i++) {
            float x = (x_offset + (i * current_width / delta_rows));
            DrawDottedLine(renderer, x, 0, x, current_height);
        }
        
        for (size_t i = 0; i < delta_cols; i++) {
            float y = (y_offset + (i * current_height / delta_cols));
            DrawDottedLine(renderer, 0, y, current_width, y);
        }
        
        // Agrego pad para que quede bien la grilla
        int rectangle_pad =1;
        int rectangle_width_padded = (current_width/ (max_col-min_col))-2*rectangle_pad;
        int rectangle_height_padded = (current_height/ (max_row-min_row))-2*rectangle_pad;

        // A diferencia de las lineas para estos si tengo que imprimir los que están afuera de la pantalla
    
        SDL_SetRenderDrawColor( renderer, 255, 255, 255, 200);
        for (size_t i = 0; i < delta_cols; i++) {
            float y = (y_offset + (i * current_height / delta_cols));
            for (size_t j = 0; j < delta_rows; j++) {
                float x = (x_offset + (j * current_width / delta_rows));
                if (grid[i + min_row][j + min_col]) {
                    SDL_FRect rect = {x + rectangle_pad, y + rectangle_pad, (float)rectangle_width_padded, (float)rectangle_height_padded};
                    SDL_RenderFillRectF(renderer, &rect);
        
                    if (show_numbers) {
                        SDL_Surface* text = TTF_RenderText_Solid(font, ("(" + std::to_string(i + min_row) + ", " + std::to_string(j + min_col) + ")").c_str(), {0, 0, 0, 0});
                        SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer, text);
                        SDL_RenderCopyF(renderer, text_texture, NULL, &rect);
                    }
                }
            }
        }
        
}


void zoomOut(viewpoint* vp, float x , float y){
    // New center on the middle point between the 
    // previous center and the mouse 
    vp->center_x = vp->center_x - (x-0.5f)*vp->zoom;
    vp->center_y = vp->center_y - (x-0.5f)*vp->zoom;

    vp->zoom = min(vp->zoom*1.5f, 1.0f);
    
}

void zoomIn(viewpoint* vp, float x , float y){
    // Same as zoomOut
    vp->center_x = vp->center_x + (x-0.5f)*vp->zoom;
    vp->center_y = vp->center_y + (x-0.5f)*vp->zoom;

    vp->zoom = max(vp->zoom*0.5f, 0.1f);
}