#include <iostream>
#include <SDL2/SDL.h>
#include <ctime>

#define CLAY_IMPLEMENTATION
#include "./clay/clay.h"
#include "./clay/clay_renderer_SDL2.c"

#include "grid.cpp"
#include "dotted_line.cpp"

using namespace std;

bool init();
void kill();
bool loop();

// Pointers to our window and renderer
SDL_Window* window;
SDL_Renderer* renderer;

// Creo la grilla del mundo
int rows=20, cols=20;
bool** grid = construct_grid(rows, cols);
double frecuencia=1;

time_t start_time;




int main(int argc, char** args) {
	if ( !init() ) return 1;

	while ( loop() ) {
		// wait before processing the next frame
		SDL_Delay(10); 
	}

	kill();
	return 0;
}

bool loop() {
	static const unsigned char* keys = SDL_GetKeyboardState( NULL );
	SDL_Event e;
	SDL_Rect r;
	
	// For mouse rectangle (static to presist between function calls)
	static int mx0 = -1, my0 = -1, mx1 = -1, my1 = -1;
	int current_width, current_height;
	SDL_GetWindowSize(window, &current_width, &current_height);
	
	// Agrego pad para que quede bien la grilla
	int rectangle_pad =1;
	int rectangle_width = (current_width/ cols);
	int rectangle_height = (current_height/ rows);
	int rectangle_width_padded = rectangle_width- rectangle_pad*2;
	int rectangle_height_padded = rectangle_height - rectangle_pad*2;
	
	// Event loop
	while ( SDL_PollEvent( &e ) != 0 ) {
		switch ( e.type ) {
			case SDL_QUIT:
				return false;
			case SDL_MOUSEBUTTONDOWN:
				mx0 = e.button.x;
				my0 = e.button.y;
				turnCell_withMemory(grid,e.button.y/rectangle_height, e.button.x/rectangle_width);
				break;
			case SDL_MOUSEMOTION:
				mx1 = e.button.x;
				my1 = e.button.y;
				if(e.motion.state==SDL_PRESSED)turnCell_withMemory(grid,e.button.y/rectangle_height, e.button.x/rectangle_width);
				break;
			case SDL_MOUSEBUTTONUP:
				mx0 = my0 = mx1 = my1 = -1;
				break;
			case SDL_KEYDOWN:
				switch ( e.key.keysym.sym ) {
					case SDLK_ESCAPE:
						kill();
						break;
					case SDLK_a:
						update_grid(grid, rows,cols);
						break;
					case SDLK_e:
						save_grid_txt(grid, rows,cols, "lastSave.txt");
						break;	
					case SDLK_l:
						loadGridFromTXT(grid, rows, cols, "lastSave.txt");
						break;	
				}
		}
	}

	double elapsed_time = difftime(time(nullptr), start_time);
	if(elapsed_time>= 1/frecuencia){
		std::cout << "Acción ejecutada en t = " << elapsed_time << " segundos." << std::endl;
		update_grid(grid, rows, cols);
		start_time = time(nullptr);
	}
	
	// Clear the window to white
	SDL_SetRenderDrawColor( renderer, 24, 24, 24, 255 );
	SDL_RenderClear( renderer );
	SDL_SetRenderDrawColor( renderer, 255, 255, 255, 50 );
 
	
	// Grilla 
	for (size_t i = 0; i < rows-1; i++){
		int x = current_width * (i + 1) / rows;
		DrawDottedLine(renderer, x, 0, x, current_height);
	}
	for (size_t i = 0; i < cols - 1; i++) {
		int y = current_height * (i + 1) / cols;
		DrawDottedLine(renderer, 0, y, current_width, y);
	}


	// Recuados vivos
	SDL_SetRenderDrawColor( renderer, 255, 255, 255, 200);
	for (size_t i = 0; i < cols; i++){
		int y = current_height *i  / rows;
			for (size_t j = 0; j < rows ; j++) {
				int x = current_width * j/ cols;
				SDL_Rect rect = {x+ rectangle_pad, y+ rectangle_pad, rectangle_width_padded, rectangle_height_padded};
				if(grid[i][j])SDL_RenderFillRect(renderer, &rect);
		}
	}

	// Update window	
	SDL_RenderPresent( renderer );
	return true;
}

bool init() {
	start_time = time(nullptr);

	// See last example for comments
	if ( SDL_Init( SDL_INIT_EVERYTHING ) < 0 ) {
		cout << "Error initializing SDL: " << SDL_GetError() << endl;
		system("pause");
		return false;
	} 

	window = SDL_CreateWindow( "Conway Game of Life", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1500, 1500, 
		SDL_WINDOW_BORDERLESS|SDL_WINDOW_RESIZABLE|SDL_WINDOW_OPENGL );
	if ( !window ) {
		cout << "Error creating window: " << SDL_GetError()  << endl;
		system("pause");
		return false;
	}

	renderer = SDL_CreateRenderer( window, -1, SDL_RENDERER_ACCELERATED );
	if ( !renderer ) {
		cout << "Error creating renderer: " << SDL_GetError() << endl;
		return false;
	}

	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor( renderer, 255, 255, 255, 255 );	
	SDL_RenderClear( renderer );

	loadGridFromTXT(grid, rows, cols, "lastSave.txt");
	return true;
}

// Quit
void kill() {
	destructor_grid(grid, rows, cols);
	SDL_DestroyRenderer( renderer );
	SDL_DestroyWindow( window );
	SDL_Quit();
}


