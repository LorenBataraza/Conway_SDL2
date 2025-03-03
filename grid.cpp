#include <fstream>
#include <iostream>

using namespace std;

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

bool** construct_grid(bool* InitCond, int rows,int col){
    bool** contructed_grid = construct_grid(rows, col);
    fill_grid(contructed_grid, InitCond, rows, col );
    return contructed_grid;
}


// Free memory
void destructor_grid(bool** grid, int rows,int col){
		for (int i = 0; i < rows; i++) {
			delete[] grid[i];
		}
        delete[] grid;
}

// Actions on Grid 
void turnCell_withMemory(bool ** grid, int row, int col ){
    static int last_row = -1;
    static int last_col = -1;

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
