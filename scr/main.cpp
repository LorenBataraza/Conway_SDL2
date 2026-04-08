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
#include "patterns.h"
#include "main_menu.cpp"
#include "client.cpp"

#if !SDL_VERSION_ATLEAST(2,0,17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

using namespace std;

// Constantes
#define FONT_PATH "../includes/Roboto-Regular.ttf"

#define DEBUG_LOOP 0
#define DEBUG_ZOOM 0
#define DEBUG_LOAD_ANIMATIONS 0

// Variables globales
SDL_Window* window;
SDL_Renderer* renderer;
TTF_Font* font;

viewpoint vp = {
    .center_x = 0.50f,
    .center_y = 0.50f,
    .zoom = 1.0f
};

AppState app_state(GRID_ROWS, GRID_COLS);
static Uint32 last_sim_update;

// Declaraciones
bool init();
bool loop();
void kill();

int main(int argc, char** args) {
    if (!init()) return 1;

    while (loop()) {
        SDL_Delay(1);  // Mínimo delay para no saturar CPU
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

    anim.width = gif->w;
    anim.height = gif->h;
    anim.frame_count = gif->count;
    anim.total_duration = 0;

    anim.frames = new SDL_Texture*[anim.frame_count];
    anim.delays = new Uint32[anim.frame_count];

    for (int i = 0; i < anim.frame_count; ++i) {
        if (!gif->frames[i]) {
            std::cerr << "Error: Frame " << i << " es NULL en " << path << "\n";
            continue;
        }
        
        anim.frames[i] = SDL_CreateTextureFromSurface(renderer, gif->frames[i]);
        anim.delays[i] = gif->delays[i];
        anim.total_duration += anim.delays[i];
    }

    anim.current_frame = 0;
    anim.last_update = SDL_GetTicks();
    anim.loop_count = 0;

    IMG_FreeAnimation(gif);
    return anim;
}

void update_animation(Animation& anim) {
    Uint32 now = SDL_GetTicks();
    Uint32 elapsed = now - anim.last_update;

    if (elapsed >= anim.delays[anim.current_frame]) {
        anim.current_frame = (anim.current_frame + 1) % anim.frame_count;
        anim.last_update = now;
        if (anim.current_frame == 0) anim.loop_count++;
    }
}

ImGuiKey SDL_ScancodeToImGuiKey(SDL_Scancode scancode) {
    switch (scancode) {
        // Letras
        case SDL_SCANCODE_A: return ImGuiKey_A;
        case SDL_SCANCODE_B: return ImGuiKey_B;
        case SDL_SCANCODE_C: return ImGuiKey_C;
        case SDL_SCANCODE_D: return ImGuiKey_D;
        case SDL_SCANCODE_E: return ImGuiKey_E;
        case SDL_SCANCODE_F: return ImGuiKey_F;
        case SDL_SCANCODE_G: return ImGuiKey_G;
        case SDL_SCANCODE_H: return ImGuiKey_H;
        case SDL_SCANCODE_I: return ImGuiKey_I;
        case SDL_SCANCODE_J: return ImGuiKey_J;
        case SDL_SCANCODE_K: return ImGuiKey_K;
        case SDL_SCANCODE_L: return ImGuiKey_L;
        case SDL_SCANCODE_M: return ImGuiKey_M;
        case SDL_SCANCODE_N: return ImGuiKey_N;
        case SDL_SCANCODE_O: return ImGuiKey_O;
        case SDL_SCANCODE_P: return ImGuiKey_P;
        case SDL_SCANCODE_Q: return ImGuiKey_Q;
        case SDL_SCANCODE_R: return ImGuiKey_R;
        case SDL_SCANCODE_S: return ImGuiKey_S;
        case SDL_SCANCODE_T: return ImGuiKey_T;
        case SDL_SCANCODE_U: return ImGuiKey_U;
        case SDL_SCANCODE_V: return ImGuiKey_V;
        case SDL_SCANCODE_W: return ImGuiKey_W;
        case SDL_SCANCODE_X: return ImGuiKey_X;
        case SDL_SCANCODE_Y: return ImGuiKey_Y;
        case SDL_SCANCODE_Z: return ImGuiKey_Z;
        
        // Números
        case SDL_SCANCODE_0: return ImGuiKey_0;
        case SDL_SCANCODE_1: return ImGuiKey_1;
        case SDL_SCANCODE_2: return ImGuiKey_2;
        case SDL_SCANCODE_3: return ImGuiKey_3;
        case SDL_SCANCODE_4: return ImGuiKey_4;
        case SDL_SCANCODE_5: return ImGuiKey_5;
        case SDL_SCANCODE_6: return ImGuiKey_6;
        case SDL_SCANCODE_7: return ImGuiKey_7;
        case SDL_SCANCODE_8: return ImGuiKey_8;
        case SDL_SCANCODE_9: return ImGuiKey_9;
        
        // Teclas de edición
        case SDL_SCANCODE_BACKSPACE: return ImGuiKey_Backspace;
        case SDL_SCANCODE_DELETE: return ImGuiKey_Delete;
        case SDL_SCANCODE_RETURN: return ImGuiKey_Enter;
        case SDL_SCANCODE_KP_ENTER: return ImGuiKey_KeypadEnter;
        case SDL_SCANCODE_TAB: return ImGuiKey_Tab;
        case SDL_SCANCODE_SPACE: return ImGuiKey_Space;
        case SDL_SCANCODE_INSERT: return ImGuiKey_Insert;
        
        // Navegación
        case SDL_SCANCODE_LEFT: return ImGuiKey_LeftArrow;
        case SDL_SCANCODE_RIGHT: return ImGuiKey_RightArrow;
        case SDL_SCANCODE_UP: return ImGuiKey_UpArrow;
        case SDL_SCANCODE_DOWN: return ImGuiKey_DownArrow;
        case SDL_SCANCODE_HOME: return ImGuiKey_Home;
        case SDL_SCANCODE_END: return ImGuiKey_End;
        case SDL_SCANCODE_PAGEUP: return ImGuiKey_PageUp;
        case SDL_SCANCODE_PAGEDOWN: return ImGuiKey_PageDown;
        
        // Modificadores
        case SDL_SCANCODE_ESCAPE: return ImGuiKey_Escape;
        case SDL_SCANCODE_LCTRL: return ImGuiKey_LeftCtrl;
        case SDL_SCANCODE_RCTRL: return ImGuiKey_RightCtrl;
        case SDL_SCANCODE_LSHIFT: return ImGuiKey_LeftShift;
        case SDL_SCANCODE_RSHIFT: return ImGuiKey_RightShift;
        case SDL_SCANCODE_LALT: return ImGuiKey_LeftAlt;
        case SDL_SCANCODE_RALT: return ImGuiKey_RightAlt;
        
        // Teclas de función
        case SDL_SCANCODE_F1: return ImGuiKey_F1;
        case SDL_SCANCODE_F2: return ImGuiKey_F2;
        case SDL_SCANCODE_F3: return ImGuiKey_F3;
        case SDL_SCANCODE_F4: return ImGuiKey_F4;
        case SDL_SCANCODE_F5: return ImGuiKey_F5;
        case SDL_SCANCODE_F6: return ImGuiKey_F6;
        case SDL_SCANCODE_F7: return ImGuiKey_F7;
        case SDL_SCANCODE_F8: return ImGuiKey_F8;
        case SDL_SCANCODE_F9: return ImGuiKey_F9;
        case SDL_SCANCODE_F10: return ImGuiKey_F10;
        case SDL_SCANCODE_F11: return ImGuiKey_F11;
        case SDL_SCANCODE_F12: return ImGuiKey_F12;
        
        // Símbolos comunes
        case SDL_SCANCODE_MINUS: return ImGuiKey_Minus;
        case SDL_SCANCODE_EQUALS: return ImGuiKey_Equal;
        case SDL_SCANCODE_LEFTBRACKET: return ImGuiKey_LeftBracket;
        case SDL_SCANCODE_RIGHTBRACKET: return ImGuiKey_RightBracket;
        case SDL_SCANCODE_BACKSLASH: return ImGuiKey_Backslash;
        case SDL_SCANCODE_SEMICOLON: return ImGuiKey_Semicolon;
        case SDL_SCANCODE_APOSTROPHE: return ImGuiKey_Apostrophe;
        case SDL_SCANCODE_COMMA: return ImGuiKey_Comma;
        case SDL_SCANCODE_PERIOD: return ImGuiKey_Period;
        case SDL_SCANCODE_SLASH: return ImGuiKey_Slash;
        case SDL_SCANCODE_GRAVE: return ImGuiKey_GraveAccent;
        
        default: return ImGuiKey_None;
    }
}

bool init() {
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        cout << "Error initializing SDL: " << SDL_GetError() << endl;
        return false;
    }

    window = SDL_CreateWindow(
        "Conway's Game of Life - Multiplayer", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
        1200, 800,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL
    );
    
    if (!window) {
        cout << "Error creating window: " << SDL_GetError() << endl;
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        cout << "Error creating renderer: " << SDL_GetError() << endl;
        return false;
    }

    if (TTF_Init() < 0) {
        cout << "Error initializing SDL_ttf: " << TTF_GetError() << endl;
        return false;
    }
    
    font = TTF_OpenFont(FONT_PATH, 72);
    if (!font) {
        cout << "Error loading font: " << TTF_GetError() << endl;
        return false;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // ImGui setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    ImFont* imgui_font = io.Fonts->AddFontFromFileTTF(FONT_PATH, 18.0f);
    if (!imgui_font) {
        std::cerr << "Error cargando fuente para ImGui!" << std::endl;
    }

    // Cargar texturas
    app_state.patterns["beehive"] = load_texture("../includes/img/still_life/beehive.png");
    app_state.patterns["boat"] = load_texture("../includes/img/still_life/boat.png");
    app_state.patterns["block"] = load_texture("../includes/img/still_life/block.png");
    app_state.patterns["flower"] = load_texture("../includes/img/still_life/flower.png");
    app_state.patterns["loaf"] = load_texture("../includes/img/still_life/loaf.png");

    // Cargar animaciones
    app_state.animations["beacon"] = std::move(load_gif("../includes/img/oscillators/beacon.gif", renderer));
    app_state.animations["blinker"] = std::move(load_gif("../includes/img/oscillators/blinker.gif", renderer));
    app_state.animations["toad"] = std::move(load_gif("../includes/img/oscillators/toad.gif", renderer));
    app_state.animations["pulsar"] = std::move(load_gif("../includes/img/oscillators/pulsar.gif", renderer));
    app_state.animations["glider"] = std::move(load_gif("../includes/img/spaceships/glider.gif", renderer));
    app_state.animations["lwss"] = std::move(load_gif("../includes/img/spaceships/lwss.gif", renderer));
    app_state.animations["mwss"] = std::move(load_gif("../includes/img/spaceships/mwss.gif", renderer));
    app_state.animations["hwss"] = std::move(load_gif("../includes/img/spaceships/hwss.gif", renderer));
    app_state.animations["glider_gun"] = std::move(load_gif("../includes/img/guns/glider_gun.gif", renderer));

    last_sim_update = SDL_GetTicks();
    app_state.fps.reset();
    
    return true;
}

bool loop() {
    // Actualizar FPS
    app_state.fps.update();
    
    int window_width, window_height;
    SDL_GetWindowSize(window, &window_width, &window_height);
    
    static ImGuiIO& io = ImGui::GetIO();
    std::vector<SDL_Event> events;
    SDL_Event event;

    // Recolectar eventos
    while (SDL_PollEvent(&event)) {
        events.push_back(event);
    }

    // Actualizar ImGui con eventos
    for (auto& ev : events) {
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
            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                ImGuiKey key = SDL_ScancodeToImGuiKey(ev.key.keysym.scancode);
                if (key != ImGuiKey_None) {
                    io.AddKeyEvent(key, (ev.type == SDL_KEYDOWN));
                }
                io.AddKeyEvent(ImGuiKey_ModCtrl, (SDL_GetModState() & KMOD_CTRL) != 0);
                io.AddKeyEvent(ImGuiKey_ModShift, (SDL_GetModState() & KMOD_SHIFT) != 0);
                io.AddKeyEvent(ImGuiKey_ModAlt, (SDL_GetModState() & KMOD_ALT) != 0);
                break;
            }
            case SDL_TEXTINPUT:
                // Necesario para que ImGui reciba los caracteres escritos
                io.AddInputCharactersUTF8(ev.text.text);
                break;
        }
    }

    // Manejar eventos de la aplicación
    for (auto& ev : events) {
        bool skipEvent = false;
        
        switch (ev.type) {
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEMOTION:
            case SDL_MOUSEWHEEL:
                skipEvent = io.WantCaptureMouse;
                break;
            case SDL_KEYDOWN:
            case SDL_TEXTINPUT:
                skipEvent = io.WantCaptureKeyboard;
                break;
        }

        if (skipEvent) continue;

        switch (ev.type) {
            case SDL_QUIT:
                return false;

            case SDL_MOUSEBUTTONDOWN:
                if (!app_state.multiplayer) {
                    load_pattern_into_grid(app_state.current_pattern, window, 
                                          app_state.grid, vp, 
                                          ev.button.x, ev.button.y, 
                                          app_state.rows, app_state.cols,
                                          app_state.player_id,
                                          app_state.mirror_horizontal,
                                          app_state.mirror_vertical);
                } else {
                    send_pattern(&app_state, window, &vp, ev.button.x, ev.button.y);
                }
                break;

            case SDL_MOUSEWHEEL: {
                int mouseX, mouseY;
                SDL_GetMouseState(&mouseX, &mouseY);
                float relX = mouseX / static_cast<float>(window_width);
                float relY = mouseY / static_cast<float>(window_height);
                
                if (ev.wheel.y > 0) {
                    zoomIn(&vp, relX, relY, app_state.rows, app_state.cols);
                } else {
                    zoomOut(&vp, relX, relY, app_state.rows, app_state.cols);
                }
                break;
            }

            case SDL_KEYDOWN:
                switch (ev.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        return false;
                    case SDLK_a:
                        if (!app_state.multiplayer) {
                            update_grid(app_state.grid, app_state.rows, app_state.cols);
                        }
                        break;
                    case SDLK_e:
                        save_grid_txt(app_state.grid, app_state.rows, app_state.cols, "lastSave.txt");
                        break;
                    case SDLK_l:
                        loadGridFromTXT(app_state.grid, app_state.rows, app_state.cols, "lastSave.txt");
                        break;
                    case SDLK_m:
                        app_state.showMinimap = !app_state.showMinimap;
                        break;
                    case SDLK_r:
                        vp.zoom = 1.0f;
                        vp.center_x = 0.5f;
                        vp.center_y = 0.5f;
                        break;
                    case SDLK_c:
                        clear_grid(app_state.grid, app_state.rows, app_state.cols);
                        break;
                    case SDLK_h:
                        app_state.mirror_horizontal = !app_state.mirror_horizontal;
                        break;
                    case SDLK_v:
                        app_state.mirror_vertical = !app_state.mirror_vertical;
                        break;
                }
                break;
        }
    }

    // Simulación singleplayer
    if (app_state.run_sim && !app_state.multiplayer) {
        Uint32 current_time = SDL_GetTicks();
        Uint32 elapsed = current_time - last_sim_update;
        Uint32 interval = 1000 / app_state.frecuencia;
        
        if (elapsed >= interval) {
            update_grid(app_state.grid, app_state.rows, app_state.cols);
            last_sim_update = current_time;
        }
    }

    // Multiplayer: recibir actualizaciones
    if (app_state.multiplayer) {
        receive_update(&app_state);
    }

    // Actualizar animaciones
    for (auto& [name, anim] : app_state.animations) {
        update_animation(anim);
    }

    // ============== RENDERIZADO UI ==============
    ImGui_ImplSDL2_NewFrame();
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui::NewFrame();

    // Menú principal
    ShowExampleAppMainMenuBar(&app_state);

    // Ventana de parámetros
    if (app_state.showParameters) {
        ImGui::Begin("Parámetros", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        
        ImGui::Text("FPS: %.1f", app_state.fps.current_fps);
        ImGui::Separator();
        
        if (!app_state.multiplayer) {
            ImGui::Checkbox("Simulación", &app_state.run_sim);
            ImGui::SliderInt("Hz", &app_state.frecuencia, 1, 60);
        }
        
        ImGui::Text("Patrón: %s", app_state.current_pattern.c_str());
        ImGui::Text("Zoom: %.0f%%", vp.zoom * 100);
        
        // Selector de color de jugador (solo singleplayer)
        if (!app_state.multiplayer) {
            ImGui::Separator();
            ImGui::Text("Tu color:");
            for (int i = 0; i < NUM_PLAYER_COLORS; i++) {
                ImGui::PushID(i);
                SDL_Color c = PLAYER_COLORS[i];
                ImVec4 color(c.r/255.0f, c.g/255.0f, c.b/255.0f, 1.0f);
                
                if (ImGui::ColorButton("##color", color, 0, ImVec2(20, 20))) {
                    app_state.player_id = i + 1;
                }
                
                if (app_state.player_id == i + 1) {
                    ImGui::SameLine();
                    ImGui::Text("<-");
                }
                
                if (i < NUM_PLAYER_COLORS - 1 && (i + 1) % 4 != 0) {
                    ImGui::SameLine();
                }
                ImGui::PopID();
            }
        }
        
        ImGui::Separator();
        ImGui::Checkbox("Minimapa", &app_state.showMinimap);
        
        ImGui::End();
    }

    // Ventana de configuración multiplayer
    if (app_state.showMultiplayerConf) {
        ImGui::Begin("Multiplayer", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        
        ImGui::Text("Estado: %s", app_state.multiplayer ? "Conectado" : "Desconectado");
        
        if (!app_state.multiplayer) {
            static char ip_buf[64] = "127.0.0.1";
            static int port = 6969;
            
            ImGui::InputText("IP", ip_buf, sizeof(ip_buf));
            ImGui::InputInt("Puerto", &port);
            
            if (ImGui::Button("Conectar", ImVec2(100, 25))) {
                app_state.server_ip = ip_buf;
                app_state.server_port = port;
                init_connection(&app_state);
                if (app_state.multiplayer) {
                    request_config(&app_state);
                }
            }
        } else {
            ImGui::Text("Servidor: %s:%d", 
                       app_state.server_ip.c_str(), 
                       app_state.server_port);
            ImGui::Text("Tu ID: %d", app_state.player_id);
            ImGui::Text("Clientes: %d", app_state.num_clients);
            
            // Mostrar tu color
            SDL_Color c = get_player_color(app_state.player_id);
            ImVec4 color(c.r/255.0f, c.g/255.0f, c.b/255.0f, 1.0f);
            ImGui::ColorButton("Tu color", color, 0, ImVec2(50, 20));
            
            ImGui::Separator();
            
            // Modo de juego
            const char* mode_str = (app_state.game_mode == AppState::GameMode::COMPETITION) 
                                   ? "COMPETITION" : "NORMAL";
            ImGui::Text("Modo: %s", mode_str);
            
            if (app_state.game_mode == AppState::GameMode::COMPETITION) {
                ImGui::TextColored(ImVec4(0.3f, 0.7f, 0.9f, 1.0f), 
                                  "Consumo: %d", app_state.my_consumption());
                ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), 
                                  "Victoria: %d/%d", 
                                  app_state.my_victory(), 
                                  app_state.victory_goal);
                ImGui::Text("Celdas: %d", app_state.my_cells());
            }
            
            ImGui::Separator();
            
            // Botones de control
            if (ImGui::Button(app_state.run_sim ? "Pausar" : "Reanudar", ImVec2(80, 25))) {
                send_command(&app_state, app_state.run_sim ? "SET_RUN 0" : "SET_RUN 1");
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Limpiar", ImVec2(80, 25))) {
                send_command(&app_state, "CLEAR");
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Reset", ImVec2(80, 25))) {
                send_command(&app_state, "RESET");
            }
            
            ImGui::Separator();
            ImGui::Text("Estadísticas:");
            ImGui::Text("  TX: %lu paquetes", app_state.net_stats.packets_sent);
            ImGui::Text("  RX: %lu paquetes", app_state.net_stats.packets_received);
            
            if (ImGui::Button("Desconectar", ImVec2(100, 25))) {
                disconnect(&app_state);
            }
        }
        
        ImGui::End();
    }

    // Ventana de estructuras (botones de patrones)
    if (app_state.showStructures) {
        ImGui::Begin("Estructuras", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        
        const ImVec2 button_size(64, 64);
        
        // Helper para obtener costo y verificar si se puede comprar
        auto get_pattern_info = [&](const char* name) -> std::pair<int, bool> {
            int cost = get_pattern_cost(name);
            bool can_buy = app_state.can_afford(cost);
            return {cost, can_buy};
        };
        
        auto pattern_button = [&](const char* name, const char* tooltip) {
            ImGui::PushID(name);
            bool selected = (name == app_state.current_pattern);
            auto [cost, can_buy] = get_pattern_info(name);
            
            // Tinte rojo si no se puede comprar (modo competición)
            ImVec4 tint = (app_state.game_mode == AppState::GameMode::COMPETITION && !can_buy) 
                          ? ImVec4(0.3f, 0.3f, 0.3f, 0.5f) 
                          : (selected ? ImVec4(1,1,1,1) : ImVec4(0.7f, 0.7f, 0.7f, 0.8f));
            
            // Guardar posición antes del botón
            ImVec2 btn_pos = ImGui::GetCursorScreenPos();
            
            if (ImGui::ImageButton(
                name,
                (ImTextureID)app_state.patterns[name],
                button_size,
                ImVec2(0,0), ImVec2(1,1),
                ImGui::GetStyle().Colors[ImGuiCol_Button],
                tint
            )) {
                if (can_buy || app_state.game_mode == AppState::GameMode::NORMAL) {
                    app_state.current_pattern = selected ? "point" : name;
                }
            }
            
            // Dibujar costo en esquina inferior derecha
            if (app_state.game_mode == AppState::GameMode::COMPETITION) {
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                char cost_str[16];
                snprintf(cost_str, sizeof(cost_str), "%d", cost);
                ImVec2 text_size = ImGui::CalcTextSize(cost_str);
                
                // Posición: esquina inferior derecha del botón
                float padding = ImGui::GetStyle().FramePadding.x;
                ImVec2 text_pos(
                    btn_pos.x + button_size.x + padding * 2 - text_size.x - 4,
                    btn_pos.y + button_size.y + padding * 2 - text_size.y - 2
                );
                
                // Fondo semitransparente
                draw_list->AddRectFilled(
                    ImVec2(text_pos.x - 2, text_pos.y - 1),
                    ImVec2(text_pos.x + text_size.x + 2, text_pos.y + text_size.y + 1),
                    IM_COL32(0, 0, 0, 180),
                    2.0f
                );
                
                // Texto del costo
                ImU32 cost_col = can_buy 
                    ? IM_COL32(100, 255, 100, 255)
                    : IM_COL32(255, 100, 100, 255);
                draw_list->AddText(text_pos, cost_col, cost_str);
            }
            
            // Tooltip
            if (ImGui::IsItemHovered()) {
                if (app_state.game_mode == AppState::GameMode::COMPETITION) {
                    ImGui::SetTooltip("%s\nCosto: %d pts\n%s", 
                                     tooltip, cost,
                                     can_buy ? "✓ Disponible" : "✗ Sin puntos");
                } else {
                    ImGui::SetTooltip("%s", tooltip);
                }
            }
            
            ImGui::PopID();
        };

        auto anim_button = [&](const char* name, const char* tooltip, ImVec2 size = ImVec2(64, 64)) {
            ImGui::PushID(name);
            
            auto it = app_state.animations.find(name);
            if (it == app_state.animations.end()) {
                ImGui::TextColored(ImVec4(1,0,0,1), "N/A");
                ImGui::PopID();
                return;
            }
            
            Animation& anim = it->second;
            if (!anim.frames || anim.current_frame >= anim.frame_count) {
                ImGui::PopID();
                return;
            }
            
            bool selected = (name == app_state.current_pattern);
            auto [cost, can_buy] = get_pattern_info(name);
            
            // Tinte si no se puede comprar
            ImVec4 tint = (app_state.game_mode == AppState::GameMode::COMPETITION && !can_buy) 
                          ? ImVec4(0.3f, 0.3f, 0.3f, 0.5f) 
                          : (selected ? ImVec4(1,1,1,1) : ImVec4(0.7f, 0.7f, 0.7f, 0.8f));
            
            // Guardar posición antes del botón
            ImVec2 btn_pos = ImGui::GetCursorScreenPos();
            
            if (ImGui::ImageButton(
                name,
                (ImTextureID)anim.frames[anim.current_frame],
                size,
                ImVec2(0,0), ImVec2(1,1),
                ImGui::GetStyle().Colors[ImGuiCol_Button],
                tint
            )) {
                if (can_buy || app_state.game_mode == AppState::GameMode::NORMAL) {
                    app_state.current_pattern = selected ? "point" : name;
                }
            }
            
            // Dibujar costo en esquina inferior derecha
            if (app_state.game_mode == AppState::GameMode::COMPETITION) {
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                char cost_str[16];
                snprintf(cost_str, sizeof(cost_str), "%d", cost);
                ImVec2 text_size = ImGui::CalcTextSize(cost_str);
                
                float padding = ImGui::GetStyle().FramePadding.x;
                ImVec2 text_pos(
                    btn_pos.x + size.x + padding * 2 - text_size.x - 4,
                    btn_pos.y + size.y + padding * 2 - text_size.y - 2
                );
                
                // Fondo semitransparente
                draw_list->AddRectFilled(
                    ImVec2(text_pos.x - 2, text_pos.y - 1),
                    ImVec2(text_pos.x + text_size.x + 2, text_pos.y + text_size.y + 1),
                    IM_COL32(0, 0, 0, 180),
                    2.0f
                );
                
                // Texto del costo
                ImU32 cost_col = can_buy 
                    ? IM_COL32(100, 255, 100, 255)
                    : IM_COL32(255, 100, 100, 255);
                draw_list->AddText(text_pos, cost_col, cost_str);
            }
            
            // Tooltip
            if (ImGui::IsItemHovered()) {
                if (app_state.game_mode == AppState::GameMode::COMPETITION) {
                    ImGui::SetTooltip("%s\nCosto: %d pts\n%s", 
                                     tooltip, cost,
                                     can_buy ? "✓ Disponible" : "✗ Sin puntos");
                } else {
                    ImGui::SetTooltip("%s", tooltip);
                }
            }
            
            ImGui::PopID();
        };

        // === CONTROLES DE ESPEJADO ===
        ImGui::Text("Espejado:");
        ImGui::SameLine();
        
        // Botón espejado horizontal
        bool was_mirror_h = app_state.mirror_horizontal;
        if (was_mirror_h) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
        }
        if (ImGui::Button("H##mirror_h", ImVec2(30, 25))) {
            app_state.mirror_horizontal = !app_state.mirror_horizontal;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Espejo Horizontal (izq/der)");
        }
        if (was_mirror_h) {
            ImGui::PopStyleColor();
        }
        
        ImGui::SameLine();
        
        // Botón espejado vertical
        bool was_mirror_v = app_state.mirror_vertical;
        if (was_mirror_v) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
        }
        if (ImGui::Button("V##mirror_v", ImVec2(30, 25))) {
            app_state.mirror_vertical = !app_state.mirror_vertical;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Espejo Vertical (arriba/abajo)");
        }
        if (was_mirror_v) {
            ImGui::PopStyleColor();
        }
        
        ImGui::SameLine();
        
        // Botón reset espejado
        if (ImGui::Button("Reset##mirror_reset", ImVec2(50, 25))) {
            app_state.mirror_horizontal = false;
            app_state.mirror_vertical = false;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Quitar espejado");
        }
        
        ImGui::Separator();

        if (ImGui::TreeNode("Still Life")) {
            pattern_button("block", "Block (2x2)");
            ImGui::SameLine();
            pattern_button("beehive", "Beehive");
            ImGui::SameLine();
            pattern_button("boat", "Boat");
            ImGui::SameLine();
            pattern_button("loaf", "Loaf");
            ImGui::SameLine();
            pattern_button("flower", "Flower");
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Oscillators")) {
            anim_button("blinker", "Blinker");
            ImGui::SameLine();
            anim_button("beacon", "Beacon");
            ImGui::SameLine();
            anim_button("toad", "Toad");
            ImGui::SameLine();
            anim_button("pulsar", "Pulsar");
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Spaceships")) {
            anim_button("glider", "Glider");
            ImGui::SameLine();
            anim_button("lwss", "Light Spaceship");
            ImGui::SameLine();
            anim_button("mwss", "Medium Spaceship");
            ImGui::SameLine();
            anim_button("hwss", "Heavy Spaceship");
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Guns")) {
            anim_button("glider_gun", "Gosper Glider Gun", ImVec2(128, 64));
            ImGui::TreePop();
        }

        ImGui::End();
    }

    // ============== HUD SUPERIOR (info del jugador) ==============
    if (app_state.showPlayerHUD && app_state.multiplayer && 
        app_state.game_mode == AppState::GameMode::COMPETITION) {
        
        ImGuiWindowFlags hud_flags = 
            ImGuiWindowFlags_NoTitleBar | 
            ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_AlwaysAutoResize;
        
        ImGui::SetNextWindowPos(ImVec2(10, 30));
        ImGui::SetNextWindowBgAlpha(0.85f);
        
        ImGui::Begin("##PlayerHUD", nullptr, hud_flags);
        
        SDL_Color my_color = get_player_color(app_state.player_id);
        ImVec4 color_vec(my_color.r/255.0f, my_color.g/255.0f, my_color.b/255.0f, 1.0f);
        
        // Header con color
        ImGui::TextColored(color_vec, "■ Jugador %d", app_state.player_id);
        ImGui::Separator();
        
        // Victoria
        ImGui::Text("Victoria:");
        ImGui::SameLine(100);
        ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.2f, 1.0f), "%d / %d", 
                          app_state.my_victory(), app_state.victory_goal);
        
        // Consumo
        ImGui::Text("Consumo:");
        ImGui::SameLine(100);
        ImGui::TextColored(ImVec4(0.3f, 0.7f, 0.9f, 1.0f), "%d", app_state.my_consumption());
        
        // Celdas
        ImGui::Text("Celdas:");
        ImGui::SameLine(100);
        ImGui::TextColored(color_vec, "%d", app_state.my_cells());
        
        // Patrón actual
        ImGui::Separator();
        int cost = get_pattern_cost(app_state.current_pattern);
        bool can_buy = app_state.can_afford(cost);
        
        ImGui::Text("Patrón:");
        ImGui::SameLine(100);
        ImGui::Text("%s", app_state.current_pattern.c_str());
        
        ImGui::Text("Costo:");
        ImGui::SameLine(100);
        if (can_buy) {
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "%d", cost);
        } else {
            ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%d (sin puntos)", cost);
        }
        
        // Indicadores de espejado
        if (app_state.mirror_horizontal || app_state.mirror_vertical) {
            ImGui::Text("Espejo:");
            ImGui::SameLine(100);
            if (app_state.mirror_horizontal) {
                ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "[H]");
                ImGui::SameLine();
            }
            if (app_state.mirror_vertical) {
                ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "[V]");
            }
        }
        
        ImGui::End();
    }

    // ============== BARRA DE VICTORIA DE TODOS LOS JUGADORES ==============
    if (app_state.showScoreBar && app_state.multiplayer && 
        app_state.game_mode == AppState::GameMode::COMPETITION) {
        
        ImGuiWindowFlags bar_flags = 
            ImGuiWindowFlags_NoTitleBar | 
            ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings;
        
        // Contar jugadores activos
        int active_count = 0;
        for (int i = 1; i <= AppState::MAX_PLAYERS; i++) {
            if (app_state.player_scores[i].active) active_count++;
        }
        
        // Altura de la barra según jugadores
        float bar_per_player = 22.0f;
        float bar_height = 10.0f + active_count * bar_per_player;
        
        ImGui::SetNextWindowPos(ImVec2(0, window_height - bar_height));
        ImGui::SetNextWindowSize(ImVec2(window_width, bar_height));
        ImGui::SetNextWindowBgAlpha(0.9f);
        
        ImGui::Begin("##ScoreBar", nullptr, bar_flags);
        
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        float bar_width = window_width - 20.0f;
        float y_offset = 5.0f;
        
        // Dibujar barra para cada jugador activo
        for (int i = 1; i <= AppState::MAX_PLAYERS; i++) {
            if (!app_state.player_scores[i].active) continue;
            
            SDL_Color p_color = get_player_color(i);
            float victory_ratio = static_cast<float>(app_state.player_scores[i].victory_points) 
                                 / app_state.victory_goal;
            
            ImVec2 bar_pos(10, window_height - bar_height + y_offset);
            
            // Fondo de la barra
            draw_list->AddRectFilled(
                bar_pos, 
                ImVec2(bar_pos.x + bar_width, bar_pos.y + 16),
                IM_COL32(30, 30, 30, 200),
                3.0f
            );
            
            // Barra de progreso del color del jugador
            float progress_width = bar_width * std::min(1.0f, victory_ratio);
            if (progress_width > 0) {
                // Barra más brillante para el jugador local
                Uint8 alpha = (i == app_state.player_id) ? 255 : 180;
                draw_list->AddRectFilled(
                    bar_pos, 
                    ImVec2(bar_pos.x + progress_width, bar_pos.y + 16),
                    IM_COL32(p_color.r, p_color.g, p_color.b, alpha),
                    3.0f
                );
            }
            
            // Borde (más grueso para jugador local)
            if (i == app_state.player_id) {
                draw_list->AddRect(
                    bar_pos, 
                    ImVec2(bar_pos.x + bar_width, bar_pos.y + 16),
                    IM_COL32(255, 255, 255, 200),
                    3.0f, 0, 2.0f
                );
            } else {
                draw_list->AddRect(
                    bar_pos, 
                    ImVec2(bar_pos.x + bar_width, bar_pos.y + 16),
                    IM_COL32(80, 80, 80, 150),
                    3.0f
                );
            }
            
            // Texto: "P1: 1234" a la izquierda
            char label[32];
            snprintf(label, sizeof(label), "P%d: %d", i, app_state.player_scores[i].victory_points);
            draw_list->AddText(
                ImVec2(bar_pos.x + 5, bar_pos.y + 1),
                IM_COL32(255, 255, 255, 255),
                label
            );
            
            y_offset += bar_per_player;
        }
        
        ImGui::End();
    }

    // ============== RENDERIZADO ==============
    SDL_SetRenderDrawColor(renderer, 20, 20, 25, 255);
    SDL_RenderClear(renderer);

    // Renderizar grilla (con zonas si está en modo competición multiplayer)
    if (app_state.multiplayer && app_state.game_mode == AppState::GameMode::COMPETITION) {
        // Contar jugadores activos
        int active_players = 0;
        for (int i = 1; i <= AppState::MAX_PLAYERS; i++) {
            if (app_state.player_scores[i].active) active_players++;
        }
        
        print_grid_with_zones(window, renderer, app_state.grid, vp, 
                             app_state.rows, app_state.cols,
                             app_state.player_id, active_players, true);
    } else {
        print_grid(window, renderer, app_state.grid, vp, app_state.rows, app_state.cols);
    }

    // Renderizar minimapa (solo si hay zoom)
    if (app_state.showMinimap && is_zoomed(vp)) {
        int margin = 10;
        render_minimap(renderer, app_state.grid, 
                      app_state.rows, app_state.cols, vp,
                      window_width - app_state.minimap_size - margin,
                      margin,
                      app_state.minimap_size, app_state.minimap_size);
    }

    // Renderizar ImGui
    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
    
    SDL_RenderPresent(renderer);

    return true;
}

void kill() {
    if (app_state.multiplayer) {
        disconnect(&app_state);
    }
    
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();
}