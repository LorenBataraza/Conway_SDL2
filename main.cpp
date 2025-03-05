#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_opengl.h>

#include <ctime>

#include "grid.cpp"

using namespace std;


bool init();
void kill();
bool loop();


// Main pointers
SDL_Window* window;
SDL_Renderer* renderer;
TTF_Font* font;

viewpoint vp = {
	.center_x = 0.50f,
	.center_y = 0.50f,
	.zoom = 0.2f
};
bool run_sim = true;
int rows=100, cols=100;

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
			turnCell_withMemory(window, grid, vp, e.button.x, e.button.y, rows, cols);
			break;
			case SDL_MOUSEMOTION:
				if(e.motion.state==SDL_PRESSED)turnCell_withMemory(window, grid, vp, e.button.x, e.button.y, rows, cols);
				break;
			case SDL_MOUSEBUTTONUP:
				mx0 = my0 = mx1 = my1 = -1;
				break;
			case SDL_MOUSEWHEEL:
				(e.wheel.y==1)? zoomIn(&vp, (float) e.wheel.mouseX/current_width, (float) e.wheel.mouseY/current_height):zoomOut(&vp, (float) e.wheel.mouseX/current_width, (float) e.wheel.mouseY/current_height );
				// cout<< vp.zoom << "\t" <<vp.center_x << "\t" <<vp.center_y << endl;
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

	if(run_sim){
		double elapsed_time = difftime(time(nullptr), start_time);
		if(elapsed_time>= 1/frecuencia){
			std::cout << "Acción ejecutada en t = " << elapsed_time << " segundos." << std::endl;
			update_grid(grid, rows, cols);
			start_time = time(nullptr);
		}
	}
	
	SDL_SetRenderDrawColor( renderer, 24, 24, 24, 255 );
	SDL_RenderClear( renderer );
 
	print_grid(window, renderer, grid, vp, rows, cols, font);

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
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	renderer = SDL_CreateRenderer( window, -1, SDL_RENDERER_ACCELERATED );
	if ( !renderer ) {
		cout << "Error creating renderer: " << SDL_GetError() << endl;
		return false;
	}

	if ( TTF_Init() < 0 ) {
		cout << "Error intializing SDL_ttf: " << TTF_GetError() << endl;
		return false;
	}

	// Load font
	font = TTF_OpenFont("Roboto-Regular.ttf", 72);
	if ( !font ) {
		cout << "Error loading font: " << TTF_GetError() << endl;
		return false;
	}
	

	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor( renderer, 255, 255, 255, 255 );	
	SDL_RenderClear( renderer );

	// loadGridFromTXT(grid, rows, cols, "lastSave.txt");
	return true;
}

// Quit
void kill() {
	destructor_grid(grid, rows, cols);
	SDL_DestroyRenderer( renderer );
	SDL_DestroyWindow( window );
	SDL_Quit();
}



