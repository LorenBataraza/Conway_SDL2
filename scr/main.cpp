#include <iostream>
#include <ctime>
#include <stdio.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include "grid.h"
// #include "ui.cpp"

#if !SDL_VERSION_ATLEAST(2,0,17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

using namespace std;

#define FONT_PATH "../includes/Roboto-Regular.ttf"
#define DEBUG_LOOP 0
#define DEBUG_ZOOM 1

bool init();
bool loop();
void kill();

SDL_Window* window;
SDL_Renderer* renderer;
TTF_Font* font;

viewpoint vp = {
	.center_x = 0.50f,
	.center_y = 0.50f,
	.zoom = 0.2f
};

int rows=100, cols=100;
bool run_sim = true;

bool** grid = construct_grid(rows, cols);
int frecuencia=1;

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
	int current_width, current_height;
	SDL_GetWindowSize(window, &current_width, &current_height);
	static const unsigned char* keys = SDL_GetKeyboardState( NULL );

	// --- UI ---
    ImGuiIO& io = ImGui::GetIO();

	SDL_Event event;

	// Agrego pad para que quede bien la grilla 
	int rectangle_pad =1;
	int rectangle_width = (current_width/ cols);
	int rectangle_height = (current_height/ rows);
	int rectangle_width_padded = rectangle_width- rectangle_pad*2;
	int rectangle_height_padded = rectangle_height - rectangle_pad*2;
	
	// Event loop
	while ( SDL_PollEvent( &event ) != 0 ) {
		switch ( event.type ) {
			case SDL_QUIT:
				return false;
			case SDL_MOUSEBUTTONDOWN:
				turnCell_withMemory(window, grid, vp, event.button.x, event.button.y, rows, cols);
				break;
			case SDL_MOUSEMOTION:
				if(event.motion.state==SDL_PRESSED)turnCell_withMemory(window, grid, vp, event.button.x, event.button.y, rows, cols);
				break;
			case SDL_MOUSEWHEEL:
				(event.wheel.y==1)? zoomIn(&vp, (float) event.wheel.mouseX/current_width, (float) event.wheel.mouseY/current_height):zoomOut(&vp, (float) event.wheel.mouseX/current_width, (float) event.wheel.mouseY/current_height );
				cout<< vp.zoom << "\t" <<vp.center_x << "\t" <<vp.center_y << endl;
				break;
			case SDL_KEYDOWN:
				switch ( event.key.keysym.sym ) {
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

	// Necesito este código para interacción de ImgUI
	int wheel = 0;
	int mouseX, mouseY;
	const int buttons = SDL_GetMouseState(&mouseX, &mouseY);
	// Setup low-level inputs (e.g. on Win32, GetKeyboardState(), or write to those fields from your Windows message loop handlers, etc.)
	io.DeltaTime = 1.0f / 60.0f;
	io.MousePos = ImVec2(static_cast<float>(mouseX), static_cast<float>(mouseY));
	io.MouseDown[0] = buttons & SDL_BUTTON(SDL_BUTTON_LEFT);
	io.MouseDown[1] = buttons & SDL_BUTTON(SDL_BUTTON_RIGHT);
	io.MouseWheel = static_cast<float>(wheel);
	
	

	ImGui_ImplSDL2_NewFrame();    
	ImGui_ImplSDLRenderer2_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("Parámetros");
	ImGui::Checkbox("Run", &run_sim);
	ImGui::DragInt("Hz", &frecuencia, 1, 1, 10, "%d", ImGuiSliderFlags_AlwaysClamp);
	ImGui::End();

	

	if(run_sim){
		double elapsed_time = difftime(time(nullptr), start_time);
		if(elapsed_time>= 1/frecuencia){
			if(DEBUG_LOOP)std::cout << "Acción ejecutada en t = " << elapsed_time << " segundos." << std::endl;
			update_grid(grid, rows, cols);
			start_time = time(nullptr);
		}
	}
	
	// Renderizado
	SDL_SetRenderDrawColor( renderer, 24, 24, 24, 255 ); 	 
	SDL_RenderClear( renderer );						 		// Limpia la pantalla
	SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
	ImGui::Render();											// Renderiza Imgui
	print_grid(window, renderer, grid, vp, rows, cols, font);	// Imprime Grilla 
	ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
	SDL_RenderPresent( renderer );								//Imprime todo por SDL2

	return true;
}

bool init() {
	start_time = time(nullptr);

	// Inicializar SDL
	if ( SDL_Init( SDL_INIT_EVERYTHING ) < 0 ) {
		cout << "Error initializing SDL: " << SDL_GetError() << endl;
		system("pause");
		return false;
	} 

	// Crea Ventana
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

	// Carga Fuente
	if ( TTF_Init() < 0 ) {
		cout << "Error intializing SDL_ttf: " << TTF_GetError() << endl;
		return false;
	}
	font = TTF_OpenFont(FONT_PATH, 72);
	if ( !font ) {
		cout << "Error loading font: " << TTF_GetError() << endl;
		return false;
	}

	// Agrega alfas
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor( renderer, 255, 255, 255, 255 );	
	SDL_RenderClear( renderer );

	//Seteo Imgui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
	ImGui::StyleColorsDark();
	
	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer2_Init(renderer);

	// Cargo Fuentes para ImGui
	ImFont* imgui_font = io.Fonts->AddFontFromFileTTF(FONT_PATH, 18.0f);
	if (!imgui_font) {
		std::cerr << "Error cargando la fuente para ImGui!" << std::endl;
		return false;
	}
	

	return true;
}

// Quit
void kill() {
	destructor_grid(grid, rows, cols);

	ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

	SDL_DestroyRenderer( renderer );
	SDL_DestroyWindow( window );
	SDL_Quit();

}

