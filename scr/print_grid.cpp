#include "dotted_line.cpp"

void drawGrid(SDL_Window* window, SDL_Renderer* renderer, bool ** grid, int rows, int cols){
    int current_width, current_height;
	SDL_GetWindowSize(window, &current_width, &current_height);
	
	// Agrego pad para que quede bien la grilla
	int rectangle_pad =1;
	int rectangle_width = (current_width/ cols);
	int rectangle_height = (current_height/ rows);
	int rectangle_width_padded = rectangle_width- rectangle_pad*2;
	int rectangle_height_padded = rectangle_height - rectangle_pad*2;
	
    SDL_SetRenderDrawColor( renderer, 255, 255, 255, 50 );
    // Grilla 
    // Dibujo lineas verticales
    for (size_t i = 0; i < rows-1; i++){
        int x = current_width * (i + 1) / rows;
        DrawDottedLine(renderer, x, 0, x, current_height);
    }
    // Dibujo líneas horizontales
    for (size_t i = 0; i < cols - 1; i++) {
        int y = current_height * (i + 1) / cols;
        DrawDottedLine(renderer, 0, y, current_width, y);
    }

    SDL_SetRenderDrawColor( renderer, 255, 255, 255, 200);
    // Recuados vivos
    for (size_t i = 0; i < cols; i++){
        int y = current_height *i  / rows;
            for (size_t j = 0; j < rows ; j++) {
                int x = current_width * j/ cols;
                SDL_Rect rect = {x+ rectangle_pad, y+ rectangle_pad, rectangle_width_padded, rectangle_height_padded};
                if(grid[i][j])SDL_RenderFillRect(renderer, &rect);
        }
    }
}