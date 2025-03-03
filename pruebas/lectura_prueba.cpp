#include <iostream>
#include <fstream>
#include <string>



using namespace std;

int main() {
    const int rows = 20, cols = 20;

    // Crear la matriz dinámicamente
    bool** grid = new bool*[rows];
    for (int i = 0; i < rows; i++) {
        grid[i] = new bool[cols];
    }

    // Cargar la matriz desde el archivo
    ifstream file("lastSave.txt");
       if (!file.is_open()) {
        cerr << "Error: No se pudo abrir el archivo " << "lastSave.txt" << endl;
        return 0;
    }

    // Read first line 
    int file_rows, file_cols;
    file >> file_rows >> file_cols;
    
    int val;
    bool bol_val;
    for (int i = 0; i < cols; i++) {
        for (int j = 0; j < rows; j++) {
            if(file >> val);
            bol_val =  (bool) val;
            cout << bol_val << " ";
        }
        cout << endl;
    } 




    // Liberar memoria
    for (int i = 0; i < rows; i++) {
        delete[] grid[i];
    }
    delete[] grid;

    return 0;
}