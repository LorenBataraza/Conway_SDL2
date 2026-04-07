#include <iostream>
#include <ctime>
#include <stdio.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include "appState.h"
#include "grid.h"
#include "main_menu.cpp"

#include "client.cpp"

#if !SDL_VERSION_ATLEAST(2,0,17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

using namespace std;

// Defino variables globales 
#define FONT_PATH "../includes/Roboto-Regular.ttf"

#define DEBUG_LOOP 0
#define DEBUG_ZOOM 0
#define DEBUG_LOAD_ANIMATIONS 0

SDL_Window* window;
SDL_Renderer* renderer;
TTF_Font* font;

viewpoint vp = {
	.center_x = 0.50f,
	.center_y = 0.50f,
	.zoom = 0.2f
};

// En app_state está mi grid, necesito app_state.rows y COLUMNS
AppState app_state(ROWS, COLS);
static Uint32 last_update; 
Uint32 elapsed;

bool init();
bool loop();
void kill();

int main(int argc, char** args) {
	if ( !init() ) return 1;

	while ( loop() ) {
		// wait before processing the next frame
		SDL_Delay(10); 
	}
	kill();
	return 0;
}

// Funciones auxiliares
SDL_Texture* load_texture(const std::string& path) {
    SDL_Surface* surface = IMG_Load(path.c_str());
    if (!surface) {
        std::cerr << "Error cargando textura: " << IMG_GetError() << std::endl;
        return nullptr;
    }
    
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return tex;
}

Animation load_gif(const std::string& path, SDL_Renderer* renderer) {
    Animation anim;
    IMG_Animation* gif = IMG_LoadAnimation(path.c_str());
    
    if (!gif || !renderer) {
        std::cerr << "Error cargando GIF: " << IMG_GetError() << "\n";
        return anim;
    }

    // 1. Almacenar metadatos del GIF
    anim.width = gif->w;        // Ancho del GIF
    anim.height = gif->h;       // Alto del GIF
    anim.frame_count = gif->count;
    anim.total_duration = 0;    // Duración total de la animación

    // 2. Reservar memoria para frames y delays
    anim.frames = new SDL_Texture*[anim.frame_count];
    anim.delays = new Uint32[anim.frame_count];  // Array de delays individuales

    // 3. Cargar cada frame con su delay correspondiente
    for (int i = 0; i < anim.frame_count; ++i) {
        // 3a. Convertir surface a textura
        anim.frames[i] = SDL_CreateTextureFromSurface(renderer, gif->frames[i]);
        
		if (!gif->frames[i]) {
			std::cerr << "Error: Frame " << i << " es NULL en " << path << "\n";
			continue;
		}
		
		anim.frames[i] = SDL_CreateTextureFromSurface(renderer, gif->frames[i]);
		if (!anim.frames[i]) {
			std::cerr << "Error creando textura: " << SDL_GetError() << "\n";
		}

        // 3c. Almacenar delay específico del frame (en milisegundos)
        anim.delays[i] = gif->delays[i];  
        anim.total_duration += anim.delays[i];
    }

    // 4. Configurar valores iniciales
    anim.current_frame = 0;
    anim.last_update = SDL_GetTicks();
    anim.loop_count = 0;  // Contador de loops completados

    // 5. Liberar recursos de SDL
    IMG_FreeAnimation(gif);
    
    return anim;
}


ImGuiKey SDL_ScancodeToImGuiKey(SDL_Scancode scancode) {
    // Mapeo directo para teclas comunes (extiende según necesites)
    switch (scancode) {
        case SDL_SCANCODE_A: return ImGuiKey_A;
        case SDL_SCANCODE_E: return ImGuiKey_E;
        case SDL_SCANCODE_L: return ImGuiKey_L;
        case SDL_SCANCODE_ESCAPE: return ImGuiKey_Escape;
        case SDL_SCANCODE_LCTRL: return ImGuiKey_LeftCtrl;
        case SDL_SCANCODE_RCTRL: return ImGuiKey_RightCtrl;
        case SDL_SCANCODE_LSHIFT: return ImGuiKey_LeftShift;
        case SDL_SCANCODE_RSHIFT: return ImGuiKey_RightShift;
        case SDL_SCANCODE_LALT: return ImGuiKey_LeftAlt;
        case SDL_SCANCODE_RALT: return ImGuiKey_RightAlt;
        default: return ImGuiKey_None;
    }
}



bool init() {

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

	// CARGO TEXTURAS
	// IMAGENES 
	app_state.patterns["beehive"] = load_texture("../includes/img/still_life/beehive.png");
	app_state.patterns["boat"] = load_texture("../includes/img/still_life/boat.png");
	app_state.patterns["block"] = load_texture("../includes/img/still_life/block.png");
	app_state.patterns["flower"] = load_texture("../includes/img/still_life/flower.png");
	app_state.patterns["loaf"] = load_texture("../includes/img/still_life/loaf.png");

	// Oscillators 
	app_state.animations["beacon"] = std::move(load_gif("../includes/img/oscillators/beacon.gif", renderer));
	app_state.animations["blinker"] = std::move(load_gif("../includes/img/oscillators/blinker.gif", renderer));
	app_state.animations["toad"] = std::move(load_gif("../includes/img/oscillators/toad.gif", renderer));
	app_state.animations["pulsar"] = std::move(load_gif("../includes/img/oscillators/pulsar.gif", renderer));

	// Spaceships
	app_state.animations["glider"] = std::move(load_gif("../includes/img/spaceships/glider.gif", renderer));
	app_state.animations["lwss"] = std::move(load_gif("../includes/img/spaceships/lwss.gif", renderer));
	app_state.animations["mwss"] = std::move(load_gif("../includes/img/spaceships/mwss.gif", renderer));
	app_state.animations["hwss"] = std::move(load_gif("../includes/img/spaceships/hwss.gif", renderer));

	// Guns
	app_state.animations["glider_gun"] = std::move(load_gif("../includes/img/guns/glider_gun.gif", renderer));

	if(DEBUG_LOAD_ANIMATIONS){
		for (const auto& [name, anim] : app_state.animations) {
			std::cout << "Cargado GIF: " << name 
			<< "\nFrames: " << anim.frame_count
			<< "\n Current Frame "<<anim.current_frame <<" \n";

			for(int i=0; i<anim.frame_count; i++){
					std::cout << i <<" frame ptr: "<< anim.frames[i]<< "\n";
				};
		}
	}

	last_update= SDL_GetTicks();
	return true;
}

void update_animation(Animation& anim) {
    Uint32 now = SDL_GetTicks();
    Uint32 elapsed = now - anim.last_update;

    // Avanzar frame solo si ha pasado el delay requerido
    if (elapsed >= anim.delays[anim.current_frame]) {
        anim.current_frame = (anim.current_frame + 1) % anim.frame_count;
        anim.last_update = now;
        
        // Contar loops completados
        if (anim.current_frame == 0) anim.loop_count++;
    }
}

bool loop() {
	int current_width, current_height;
	SDL_GetWindowSize(window, &current_width, &current_height);
	static const unsigned char* keys = SDL_GetKeyboardState( NULL );

	///////// RENDERIZADO 
    static ImGuiIO& io = ImGui::GetIO();
    std::vector<SDL_Event> events;
    SDL_Event event;

    // 1. Recolectar todos los eventos
    while (SDL_PollEvent(&event)) {
        events.push_back(event);
    }

    // 2. Actualizar estado de ImGui con los eventos
    for (auto& ev : events) {
        // Mouse
        switch (ev.type) {
            case SDL_MOUSEMOTION:
                io.MousePos = ImVec2(ev.motion.x, ev.motion.y);
                break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: {
                int button = -1;
                if (ev.button.button == SDL_BUTTON_LEFT) button = 0;
                else if (ev.button.button == SDL_BUTTON_RIGHT) button = 1;
                if (button != -1) {
                    io.MouseDown[button] = (ev.type == SDL_MOUSEBUTTONDOWN);
                }
                break;
            }
            case SDL_MOUSEWHEEL:
                io.MouseWheel = ev.wheel.y;
                break;
            
				
            // Teclado
			case SDL_KEYDOWN:
			case SDL_KEYUP: {
				SDL_Scancode scancode = ev.key.keysym.scancode;
				ImGuiKey key = SDL_ScancodeToImGuiKey(scancode);
				if (key != ImGuiKey_None) {
					io.AddKeyEvent(key, (ev.type == SDL_KEYDOWN));
				}
	
				// Actualizar modificadores
				io.AddKeyEvent(ImGuiKey_ModCtrl, (SDL_GetModState() & KMOD_CTRL) != 0);
				io.AddKeyEvent(ImGuiKey_ModShift, (SDL_GetModState() & KMOD_SHIFT) != 0);
				io.AddKeyEvent(ImGuiKey_ModAlt, (SDL_GetModState() & KMOD_ALT) != 0);
				break;
			}
        }
    }


    // 4. Manejar eventos de la aplicación (solo si ImGui no los está usando)
    for (auto& ev : events) {
        bool skipEvent = false;
        
        // Determinar si el evento debe ser manejado por la aplicación
        switch (ev.type) {
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEMOTION:
            case SDL_MOUSEWHEEL:
                skipEvent = io.WantCaptureMouse;
                break;
            
            case SDL_KEYDOWN:
                skipEvent = io.WantCaptureKeyboard;
                break;
        }

        if (skipEvent) continue;

        // Manejar eventos específicos de la aplicación
        switch (ev.type) {
            case SDL_QUIT:
                return false;

            case SDL_MOUSEBUTTONDOWN:
				if(app_state.multiplayer==false)load_pattern_into_grid(app_state.current_pattern, window, app_state.grid, vp, ev.button.x, ev.button.y, app_state.rows, app_state.cols);
				if(app_state.multiplayer==true){
					int mouseX, mouseY;
					SDL_GetMouseState(&mouseX, &mouseY);
					send_pattern(&app_state, window, &vp, mouseX, mouseY);
				}
                break;
			/*
            case SDL_MOUSEMOTION:
                if(ev.motion.state == SDL_PRESSED) {
                    load_pattern_into_grid(app_state.current_pattern, window, grid, vp, ev.button.x, ev.button.y, app_state.rows, app_state.cols);
                }
                break;
			*/
			case SDL_MOUSEWHEEL: {
				int mouseX, mouseY;
				SDL_GetMouseState(&mouseX, &mouseY);
				float relX = mouseX / static_cast<float>(current_width);
				float relY = mouseY / static_cast<float>(current_height);
				
				if (event.wheel.y > 0) {
					zoomIn(&vp, relX, relY, app_state.rows, app_state.cols);
				} else {
					zoomOut(&vp, relX, relY, app_state.rows, app_state.cols);
				}
				break;
			}

            case SDL_KEYDOWN:
                switch (ev.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        kill();
                        break;
                    case SDLK_a:
						if(app_state.multiplayer==false)update_grid(app_state.grid, app_state.rows, app_state.cols);
                        break;
                    case SDLK_e:
                        save_grid_txt(app_state.grid, app_state.rows, app_state.cols, "lastSave.txt");
                        break;    
                    case SDLK_l:
                        loadGridFromTXT(app_state.grid, app_state.rows, app_state.cols, "lastSave.txt");
                        break;
                }
                break;
        }
    }
	
	if(app_state.run_sim && !app_state.multiplayer) {
		Uint32 current_ticks = SDL_GetTicks();  // Milisegundos desde que inició SDL
		elapsed = current_ticks - last_update;
		
		if(elapsed >= (1000 / app_state.frecuencia)) {  // Conversión a milisegundos
			if(DEBUG_LOOP) {
				std::cout << "Update en " << elapsed << " ms\t" << "T= " << (1000 / app_state.frecuencia) << std::endl  ;
			}
			update_grid(app_state.grid, app_state.rows, app_state.cols);
			last_update = current_ticks;  // Resetear contador
		}
	}

	if(app_state.multiplayer) {
		receive_update(&app_state); // Recibir actualizaciones
	}

	// Actualiza anim.current_frame para correr el gif
	for (auto& [name, anim] : app_state.animations) {
		update_animation(anim);
	}

	// RENDERIZADO UI
	ImGui_ImplSDL2_NewFrame();    
	ImGui_ImplSDLRenderer2_NewFrame();
	ImGui::NewFrame();

	// Menu 
	ShowExampleAppMainMenuBar(&app_state);
	
	// Ventana Simulación 
	if(app_state.showParameters && app_state.multiplayer==false){
	ImGui::Begin("Parámetros",nullptr , ImGuiWindowFlags_NoMove|| ImGuiWindowFlags_NoCollapse || ImGuiWindowFlags_NoResize);
	ImGui::Checkbox("Run", &app_state.run_sim);
	ImGui::DragInt("Hz", &app_state.frecuencia, 1, 1, 60, "%d", ImGuiSliderFlags_AlwaysClamp);
    ImGui::Spacing();
	ImGui::Text("Patron selecionado: %s", app_state.current_pattern.c_str());
	ImGui::Text("fps: %d", 1000/elapsed);
	ImGui::End();
	}

	
	const ImVec2 button_size(64, 64);  // Ajustar según necesidad
	
	if(app_state.showStructures){
	ImGui::Begin("Estructuras", nullptr, ImGuiWindowFlags_NoMove || ImGuiWindowFlags_AlwaysAutoResize);
	if (ImGui::TreeNode("Still Life")){
	// Tamaño común para los botones
	// Función helper para crear botones
	auto pattern_button = [&](const char* name, const char* tooltip, const ImVec2 button_size) {
		ImGui::PushID(name);
		
		// Usar un ID único basado en el nombre para str_id
		if (ImGui::ImageButton(
			name,                            // str_id único (1er parámetro)
			(ImTextureID)app_state.patterns[name],        // ImTextureID (2do parámetro)
			button_size, 
			ImVec2(0,0), 
			ImVec2(1,1),
			ImGui::GetStyle().Colors[ImGuiCol_Button], // Bg color
			(name==app_state.current_pattern)? ImVec4(1,1,1,1):ImVec4(0.5, 0.5, 0.5, 0.5)                 // Tint color (blanco)
		)) {
			(name==app_state.current_pattern)? app_state.current_pattern="point":app_state.current_pattern=name;
		}
		
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(tooltip);
			ImGui::EndTooltip();
		}
		
		ImGui::PopID();
	};
	// Organización en fila
	pattern_button("block", "Block (Cuadrado 2x2)", button_size);
	ImGui::SameLine();
	pattern_button("beehive", "Beehive (Colmena)", button_size);
	ImGui::SameLine();
	pattern_button("boat", "Boat (Bote)", button_size);
	ImGui::SameLine();
	pattern_button("loaf", "Loaf (Pan)", button_size);
	ImGui::SameLine();
	pattern_button("flower", "Flower (Flor)", button_size);
	ImGui::TreePop();
	}

	auto oscillator_button = [&](const char* name, const char* tooltip, const ImVec2 button_size) {
			
		ImGui::PushID(name);
		
		// 1. Verificar existencia
		auto it = app_state.animations.find(name);
		if (it == app_state.animations.end()) {
			ImGui::TextColored(ImVec4(1,0,0,1), "Animación %s no cargada!", name);
			ImGui::PopID();
			return;
		}
		
		// 2. Obtener referencia
		Animation& anim = it->second;
		
		// 3. Verificar frames
		if (!anim.frames || anim.current_frame < 0 || anim.current_frame >= anim.frame_count) {
			ImGui::TextColored(ImVec4(1,0,0,1), "Animación %s corrupta!", name);
			ImGui::PopID();
			return;
		}
		
		// 4. Usar la referencia con seguridad
		SDL_Texture* current_frame = anim.frames[anim.current_frame];
		Uint32 current_delay = anim.delays[anim.current_frame];
		
		if (ImGui::ImageButton(
			name,
			(ImTextureID)current_frame,
			button_size,
			ImVec2(0,0),
			ImVec2(1,1),
			ImGui::GetStyle().Colors[ImGuiCol_Button],
			(name==app_state.current_pattern)? ImVec4(1,1,1,1):ImVec4(0.5, 0.5, 0.5, 0.5)                // Tint color (blanco)
		)) {
			(name==app_state.current_pattern)? app_state.current_pattern="point":app_state.current_pattern=name;
		}
		
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(tooltip);
			ImGui::EndTooltip();
		}

		ImGui::PopID();
	};

	if (ImGui::TreeNode("Oscilators")){
	
		oscillator_button("blinker", "Titilador", button_size);
		ImGui::SameLine();
		oscillator_button("beacon", "Farol", button_size);
		ImGui::SameLine();
		oscillator_button("toad", "Sapo", button_size);
		ImGui::SameLine();
		oscillator_button("pulsar", "Pulsar", button_size);
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Spaceships")){
		oscillator_button("glider", "Planeador", button_size);
		ImGui::SameLine();
		oscillator_button("lwss", "Nave ligera", button_size);
		ImGui::SameLine();
		oscillator_button("mwss", "Nave mediana", button_size);
		ImGui::SameLine();
		oscillator_button("hwss", "Nave pesada", button_size);
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Guns")){
		oscillator_button("glider_gun", "Glider Gun", ImVec2(128,64));
		ImGui::TreePop();
	}
	// 3. Cerrar ventana
	ImGui::End();
	}

	if(app_state.showMultiplayerConf){
		ImGui::Begin("Configuración Multiplayeer",nullptr , ImGuiWindowFlags_NoCollapse || ImGuiWindowFlags_NoResize);
		ImGui::Text("Estado de Conexión: %s", (app_state.multiplayer)?"Conectado":"Desconectado");
		ImGui::Text("Puerto actual selecionado: %i", app_state.server_port);
		ImGui::Text("IP actual: %i", app_state.server_ip);
		
		if(app_state.multiplayer==false){
		if(ImGui::Button("Conectar", ImVec2(90.0,20.0))) {
			init_conection(&app_state);
		}
		}

		ImGui::End();
	}

	// Renderizado
	SDL_SetRenderDrawColor( renderer, 24, 24, 24, 255 ); 	 
	SDL_RenderClear( renderer );						 		// Limpia la pantalla
	SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
	ImGui::Render();											// Renderiza Imgui
	print_grid(window, renderer, app_state.grid, vp, app_state.rows, app_state.cols);	// Imprime Grilla 
	ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
	SDL_RenderPresent( renderer );								//Imprime todo por SDL2

	return true;
}

// Quit
void kill() {
	ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

	SDL_DestroyRenderer( renderer );
	SDL_DestroyWindow( window );
	SDL_Quit();

}

