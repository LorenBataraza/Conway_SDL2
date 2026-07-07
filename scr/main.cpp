#include "net_compat.h"   // Winsock antes que SDL/windows.h (orden importante en Windows)

#include <iostream>
#include <ctime>
#include <algorithm>
#include <stdio.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include "appState.h"
#include "grid.h"
#include "patterns.h"
#include "pattern_preview.h"
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

// Previews nativos de patrones (reemplazan los GIF/PNG): render con el propio
// autómata. Clave = nombre del patrón.
std::unordered_map<std::string, std::unique_ptr<PatternPreview>> g_previews;

// Crea (una vez) el preview de un patrón. `animate` para osciladores/naves/guns.
static PatternPreview* get_preview(const std::string& name, bool animate, int size = 64) {
    auto it = g_previews.find(name);
    if (it != g_previews.end()) return it->second.get();
    auto pv = std::make_unique<PatternPreview>(renderer, name, size, animate);
    PatternPreview* raw = pv.get();
    g_previews[name] = std::move(pv);
    return raw;
}

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
// (Los previews de patrones ahora se renderizan con el propio autómata en
// PatternPreview; ya no se cargan texturas PNG ni animaciones GIF externas.)

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
    // Inicializar la pila de red (Winsock en Windows; no-op en POSIX).
    if (net_init() != 0) {
        std::cerr << "Error inicializando la red (Winsock)" << std::endl;
        return false;
    }

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

    // Crear previews NATIVOS: el propio autómata renderiza cada patrón, en
    // lugar de cargar GIF/PNG externos. Still lifes estáticos; el resto animado.
    for (const char* n : {"block", "beehive", "boat", "loaf", "flower",
                          "tub", "pond", "ship"})
        get_preview(n, /*animate=*/false);
    for (const char* n : {"blinker", "beacon", "toad", "pulsar", "pentadecathlon",
                          "glider", "lwss", "mwss", "hwss", "glider_gun",
                          "r_pentomino", "acorn", "diehard"})
        get_preview(n, /*animate=*/true);

    // Cargar historial de conexiones multiplayer.
    app_state.connection_history = load_connection_history();

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
                    // Edición externa de la grilla: reevaluar todo el próximo paso.
                    app_state.automaton.mark_all_dirty();
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
                            app_state.automaton.step();
                        }
                        break;
                    case SDLK_e:
                        save_grid_txt(app_state.grid, app_state.rows, app_state.cols, "lastSave.txt");
                        break;
                    case SDLK_l:
                        loadGridFromTXT(app_state.grid, app_state.rows, app_state.cols, "lastSave.txt");
                        app_state.automaton.mark_all_dirty();
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
                        app_state.automaton.clear();
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
            app_state.automaton.step();
            last_sim_update = current_time;
        }
    }

    // Multiplayer: recibir actualizaciones
    if (app_state.multiplayer) {
        receive_update(&app_state);
    }

    // (Los previews nativos se animan a sí mismos al pedir su textura.)

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
                    request_full_grid(&app_state);  // sync inicial (lockstep)
                    remember_connection(app_state.connection_history,
                                        app_state.server_ip, app_state.server_port);
                }
            }

            // Lista de conexiones previas (click para rellenar IP/puerto).
            if (!app_state.connection_history.empty()) {
                ImGui::Separator();
                ImGui::TextDisabled("Conexiones previas:");
                for (size_t i = 0; i < app_state.connection_history.size(); ++i) {
                    const auto& e = app_state.connection_history[i];
                    char label[96];
                    snprintf(label, sizeof(label), "%s:%d##conn%zu",
                             e.ip.c_str(), e.port, i);
                    if (ImGui::Selectable(label)) {
                        snprintf(ip_buf, sizeof(ip_buf), "%s", e.ip.c_str());
                        port = e.port;
                    }
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

        // Helper para obtener costo y verificar si se puede comprar
        auto get_pattern_info = [&](const char* name) -> std::pair<int, bool> {
            int cost = get_pattern_cost(name);
            bool can_buy = app_state.can_afford(cost);
            return {cost, can_buy};
        };

        // Botón de patrón con PREVIEW NATIVO (render del propio autómata en vez
        // de un GIF/PNG). `animate` para osciladores / naves / cañones.
        const Uint32 now_ticks = SDL_GetTicks();
        auto preview_button = [&](const char* name, const char* tooltip, bool animate) {
            ImGui::PushID(name);
            PatternPreview* pv = get_preview(name, animate);
            SDL_Texture* tex = pv ? pv->texture(now_ticks) : nullptr;
            if (!tex) {
                ImGui::TextColored(ImVec4(1,0,0,1), "N/A");
                ImGui::PopID();
                return;
            }
            const ImVec2 img_size(static_cast<float>(pv->width()),
                                  static_cast<float>(pv->height()));

            bool selected = (name == app_state.current_pattern);
            auto [cost, can_buy] = get_pattern_info(name);

            ImVec4 tint = (app_state.game_mode == AppState::GameMode::COMPETITION && !can_buy)
                          ? ImVec4(0.3f, 0.3f, 0.3f, 0.5f)
                          : (selected ? ImVec4(1,1,1,1) : ImVec4(0.7f, 0.7f, 0.7f, 0.8f));

            ImVec2 btn_pos = ImGui::GetCursorScreenPos();

            if (ImGui::ImageButton(
                    name, (ImTextureID)tex, img_size,
                    ImVec2(0,0), ImVec2(1,1),
                    ImGui::GetStyle().Colors[ImGuiCol_Button], tint)) {
                if (can_buy || app_state.game_mode == AppState::GameMode::NORMAL) {
                    app_state.current_pattern = selected ? "point" : name;
                }
            }

            // Costo en esquina inferior derecha (modo competición)
            if (app_state.game_mode == AppState::GameMode::COMPETITION) {
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                char cost_str[16];
                snprintf(cost_str, sizeof(cost_str), "%d", cost);
                ImVec2 text_size = ImGui::CalcTextSize(cost_str);
                float padding = ImGui::GetStyle().FramePadding.x;
                ImVec2 text_pos(
                    btn_pos.x + img_size.x + padding * 2 - text_size.x - 4,
                    btn_pos.y + img_size.y + padding * 2 - text_size.y - 2);
                draw_list->AddRectFilled(
                    ImVec2(text_pos.x - 2, text_pos.y - 1),
                    ImVec2(text_pos.x + text_size.x + 2, text_pos.y + text_size.y + 1),
                    IM_COL32(0, 0, 0, 180), 2.0f);
                ImU32 cost_col = can_buy ? IM_COL32(100,255,100,255)
                                         : IM_COL32(255,100,100,255);
                draw_list->AddText(text_pos, cost_col, cost_str);
            }

            if (ImGui::IsItemHovered()) {
                if (app_state.game_mode == AppState::GameMode::COMPETITION) {
                    ImGui::SetTooltip("%s\nCosto: %d pts\n%s", tooltip, cost,
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
            preview_button("block", "Block (2x2)", false);
            ImGui::SameLine();
            preview_button("beehive", "Beehive", false);
            ImGui::SameLine();
            preview_button("boat", "Boat", false);
            ImGui::SameLine();
            preview_button("loaf", "Loaf", false);
            ImGui::SameLine();
            preview_button("flower", "Flower", false);
            ImGui::SameLine();
            preview_button("tub", "Tub", false);
            ImGui::SameLine();
            preview_button("pond", "Pond", false);
            ImGui::SameLine();
            preview_button("ship", "Ship", false);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Oscillators")) {
            preview_button("blinker", "Blinker", true);
            ImGui::SameLine();
            preview_button("beacon", "Beacon", true);
            ImGui::SameLine();
            preview_button("toad", "Toad", true);
            ImGui::SameLine();
            preview_button("pulsar", "Pulsar", true);
            ImGui::SameLine();
            preview_button("pentadecathlon", "Pentadecathlon (período 15)", true);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Spaceships")) {
            preview_button("glider", "Glider", true);
            ImGui::SameLine();
            preview_button("lwss", "Light Spaceship", true);
            ImGui::SameLine();
            preview_button("mwss", "Medium Spaceship", true);
            ImGui::SameLine();
            preview_button("hwss", "Heavy Spaceship", true);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Guns")) {
            preview_button("glider_gun", "Gosper Glider Gun", true);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Methuselahs")) {
            preview_button("r_pentomino", "R-pentomino (~1103 gen)", true);
            ImGui::SameLine();
            preview_button("acorn", "Acorn (~5206 gen)", true);
            ImGui::SameLine();
            preview_button("diehard", "Diehard (desaparece en 130 gen)", true);
            ImGui::TreePop();
        }

        ImGui::End();
    }

    // ============== AYUDA (?) ==============
    if (app_state.showHelp) {
        ImGui::SetNextWindowSize(ImVec2(440, 400), ImGuiCond_FirstUseEver);
        ImGui::Begin("Ayuda (?)", &app_state.showHelp);

        ImGui::TextWrapped("Juego de la Vida de Conway — guía rápida.");
        ImGui::Separator();

        if (ImGui::CollapsingHeader("Controles básicos", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::BulletText("Click izquierdo: colocar el patrón seleccionado.");
            ImGui::BulletText("Rueda del mouse: zoom hacia el cursor.");
            ImGui::BulletText("Tecla A: avanzar un paso (útil en pausa).");
            ImGui::BulletText("Tecla C: limpiar la grilla.");
            ImGui::BulletText("Tecla R: resetear la vista (zoom/centro).");
            ImGui::BulletText("Tecla M: mostrar/ocultar el minimapa.");
            ImGui::BulletText("Teclas H / V: espejar el patrón horizontal / vertical.");
            ImGui::BulletText("Teclas E / L: guardar / cargar la grilla (lastSave.txt).");
        }
        if (ImGui::CollapsingHeader("Estructuras")) {
            ImGui::TextWrapped("En el panel 'Estructuras' elegís un patrón (still life, "
                "oscilador, nave, cañón). Cada preview se renderiza con el propio "
                "autómata del juego, no con imágenes externas.");
        }
        if (ImGui::CollapsingHeader("Parámetros")) {
            ImGui::TextWrapped("Ajustá la frecuencia de simulación, pausá/reanudá y "
                "elegí tu color de jugador desde el panel 'Parámetros'.");
        }
        if (ImGui::CollapsingHeader("Multiplayer")) {
            ImGui::TextWrapped("Abrí 'Multiplayer', ingresá IP y puerto del servidor y "
                "conectá; se guarda un historial de conexiones previas. En modo "
                "competición gastás puntos de consumo al colocar patrones dentro de tu "
                "zona de spawn.");
        }
        ImGui::Separator();
        ImGui::TextDisabled("(Contenido preliminar — se ampliará.)");
        ImGui::End();
    }

    // ============== WIKI ==============
    if (app_state.showWiki) {
        ImGui::SetNextWindowSize(ImVec2(660, 470), ImGuiCond_FirstUseEver);
        ImGui::Begin("Wiki", &app_state.showWiki);

        static int wiki_section = 0;
        const char* sections[] = {
            "Vecindarios", "Reglas y conceptos", "Entropía e información", "Catálogo"
        };

        ImGui::BeginChild("wiki_nav", ImVec2(150, 0), true);
        for (int i = 0; i < IM_ARRAYSIZE(sections); ++i) {
            if (ImGui::Selectable(sections[i], wiki_section == i)) wiki_section = i;
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("wiki_content", ImVec2(0, 0), true);

        const Uint32 wnow = SDL_GetTicks();

        if (wiki_section == 0) {
            ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "Introducción a los Vecindarios");
            ImGui::Separator();
            ImGui::TextWrapped("El vecindario define qué celdas se consideran 'vecinas' de "
                "una celda al aplicar las reglas. Los dos más comunes:");

            // Diagrama de vecindario (3x3) dibujado con el draw list.
            auto draw_neighborhood = [&](bool moore) {
                const float cell = 24.0f;
                ImVec2 p = ImGui::GetCursorScreenPos();
                ImDrawList* dl = ImGui::GetWindowDrawList();
                for (int r = 0; r < 3; ++r) {
                    for (int c = 0; c < 3; ++c) {
                        ImVec2 a(p.x + c * cell, p.y + r * cell);
                        ImVec2 b(a.x + cell - 3, a.y + cell - 3);
                        bool center = (r == 1 && c == 1);
                        bool neigh = moore ? !center : ((r == 1) != (c == 1));
                        ImU32 col = center ? IM_COL32(100, 255, 100, 255)
                                   : (neigh ? IM_COL32(90, 120, 230, 255)
                                            : IM_COL32(50, 50, 58, 255));
                        dl->AddRectFilled(a, b, col, 2.0f);
                    }
                }
                ImGui::Dummy(ImVec2(3 * cell, 3 * cell));
            };

            ImGui::Spacing();
            ImGui::BeginGroup();
            ImGui::Text("Moore (8 vecinos)");
            draw_neighborhood(true);
            ImGui::EndGroup();
            ImGui::SameLine(0.0f, 40.0f);
            ImGui::BeginGroup();
            ImGui::Text("von Neumann (4 vecinos)");
            draw_neighborhood(false);
            ImGui::EndGroup();

            ImGui::Spacing();
            ImGui::TextWrapped("Moore incluye las 8 celdas que rodean a la central (es el "
                "que usa el Juego de la Vida). von Neumann sólo incluye las 4 celdas "
                "ortogonales. La elección del vecindario cambia drásticamente la dinámica.");
        }
        else if (wiki_section == 1) {
            ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "Reglas y conceptos");
            ImGui::Separator();
            ImGui::TextWrapped("Notación B/S: un autómata se describe por los conteos de "
                "vecinos vivos que provocan Nacimiento (B) y Supervivencia (S). El Juego "
                "de la Vida clásico es B3/S23: una celda muerta nace con exactamente 3 "
                "vecinos; una viva sobrevive con 2 o 3, y en otro caso muere.");
            ImGui::Spacing();
            ImGui::BulletText("Modo NORMAL: B3/S23, heredando el color del jugador dominante.");
            ImGui::TextWrapped("Modo COMPETITION: las celdas vivas cuentan aliados (+1) y "
                "enemigos (-1); sobreviven con vecindario efectivo entre 2 y 3. Así, celdas "
                "enemigas se debilitan mutuamente.");
            ImGui::Spacing();
            ImGui::TextDisabled("(El motor permite registrar reglas nuevas como Rulesets.)");
        }
        else if (wiki_section == 2) {
            ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "Entropía e información");
            ImGui::Separator();
            ImGui::TextWrapped("Algunas reglas tienden a homogeneizar el tablero: la "
                "actividad decae hacia un estado uniforme, borrando la información de la "
                "configuración inicial (alta 'difusión', baja entropía estructural).");
            ImGui::Spacing();
            ImGui::TextWrapped("Otras reglas —como B3/S23— mantienen estructuras complejas y "
                "persistentes: coexisten regiones estables, oscilantes y caóticas que "
                "conservan y transportan información local (naves, cañones, cables).");
            ImGui::Spacing();
            ImGui::TextWrapped("Medir cómo evoluciona la 'población' y la diversidad de "
                "patrones da una intuición de cuánta información retiene una regla frente "
                "a cuánto tiende al equilibrio homogéneo.");
            ImGui::Spacing();
            ImGui::TextDisabled("(Se podrán añadir demos comparando reglas homogeneizantes.)");
        }
        else {
            ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "Catálogo de patrones");
            ImGui::Separator();

            // `pattern`: patrón a mostrar/cargar (nullptr = sólo texto).
            // `big`: ejemplo grande -> sin preview chico; se muestra cargándolo
            //        en la grilla (guarda la grilla previa) para verlo a tamaño real.
            struct WikiEntry { const char* title; const char* desc; const char* pattern; bool big; };
            static const WikiEntry catalog[] = {
                {"Still lifes",     "Patrones estables que no cambian entre generaciones.", "beehive", false},
                {"Oscillators",     "Vuelven a su estado inicial tras un período fijo.", "pulsar", false},
                {"Spaceships",      "Se trasladan por la grilla conservando su forma.", "glider", false},
                {"Guns",            "Emiten naves periódicamente (p. ej. el cañón de Gosper).", "glider_gun", true},
                {"Methuselahs",     "Patrones pequeños que evolucionan miles de generaciones antes de estabilizarse (Acorn).", "acorn", true},
                {"Puffers",         "Naves que dejan un rastro de restos a su paso (puffer train).", "puffer", true},
                {"Sawtooths",       "Poblaciones que crecen y colapsan sin cota superior. No entran en ninguna grilla finita (necesitan naves que viajan al infinito y vuelven).", nullptr, false},
                {"Agars",           "Texturas periódicas que llenan el plano; aquí un spacefiller que va generando el agar.", "agar", true},
                {"Wicks",           "Estructuras que 'arden' a lo largo de una línea (mecha diagonal).", "wick", true},
                {"Conduits",        "Canales que transportan y redirigen naves; aquí un 'racetrack' que hace circular un glider en bucle.", "conduit", true},
                {"Gardens of Eden", "Configuraciones sin predecesor posible. Su rasgo (no tener pasado) no se observa en la simulación, por eso va sólo como texto.", nullptr, false},
            };

            // Carga un ejemplo en la grilla actual (sólo un jugador): guarda la
            // grilla previa, la limpia, coloca el patrón centrado y avisa.
            auto load_example_into_grid = [&](const char* name) {
                Board& b = app_state.automaton.board();
                app_state.grid_backup.assign(b.data(), b.data() + b.size());
                app_state.has_backup = true;

                app_state.automaton.clear();

                // Centrar la bounding box del patrón (dx -> col, dy -> fila).
                int mindx = 0, maxdx = 0, mindy = 0, maxdy = 0; bool first = true;
                for (const auto& [dx, dy] : get_pattern_cells(name)) {
                    if (first) { mindx = maxdx = dx; mindy = maxdy = dy; first = false; }
                    else {
                        mindx = std::min(mindx, dx); maxdx = std::max(maxdx, dx);
                        mindy = std::min(mindy, dy); maxdy = std::max(maxdy, dy);
                    }
                }
                const int pw = maxdx - mindx + 1, ph = maxdy - mindy + 1;
                const int row = app_state.rows / 2 - ph / 2 - mindy;
                const int col = app_state.cols / 2 - pw / 2 - mindx;
                app_state.automaton.add_pattern(name, row, col, app_state.player_id);
                app_state.grid = app_state.automaton.grid();

                // Ver el ejemplo completo: resetear la vista.
                vp.zoom = 1.0f; vp.center_x = 0.5f; vp.center_y = 0.5f;

                app_state.notify(std::string("Grilla previa guardada — ejemplo cargado: ") + name);
            };

            for (const auto& e : catalog) {
                ImGui::PushID(e.title);
                if (e.pattern && !e.big) {
                    PatternPreview* pv = get_preview(e.pattern, true);
                    if (pv) {
                        ImGui::Image((ImTextureID)pv->texture(wnow),
                                     ImVec2(static_cast<float>(pv->width()),
                                            static_cast<float>(pv->height())));
                        ImGui::SameLine();
                    }
                }
                ImGui::BeginGroup();
                ImGui::TextColored(ImVec4(0.85f, 0.95f, 1.0f, 1.0f), "%s", e.title);
                ImGui::TextWrapped("%s", e.desc);
                if (e.pattern) {
                    const bool mp = app_state.multiplayer;
                    if (mp) ImGui::BeginDisabled();
                    if (ImGui::SmallButton("Cargar en la grilla")) {
                        load_example_into_grid(e.pattern);
                    }
                    if (mp) {
                        ImGui::EndDisabled();
                        ImGui::SameLine();
                        ImGui::TextDisabled("(sólo un jugador)");
                    }
                }
                ImGui::EndGroup();
                ImGui::Separator();
                ImGui::PopID();
            }
            ImGui::TextDisabled("(Al cargar un ejemplo se guarda tu grilla anterior; podés restaurarla desde el aviso.)");
        }

        ImGui::EndChild();
        ImGui::End();
    }

    // ============== NOTIFICACIÓN (toast) ==============
    // Mini aviso abajo-centro (p. ej. "grilla previa guardada"), con opción de
    // restaurar la grilla anterior. Se desvanece tras unos segundos.
    if (app_state.notification_time != 0) {
        const Uint32 age = SDL_GetTicks() - app_state.notification_time;
        const Uint32 TOAST_MS = 6000;
        if (age >= TOAST_MS) {
            app_state.notification_time = 0;
        } else {
            float alpha = 1.0f;
            if (age > TOAST_MS - 1000) alpha = (TOAST_MS - age) / 1000.0f;

            ImGuiViewport* mv = ImGui::GetMainViewport();
            ImVec2 pos(mv->WorkPos.x + mv->WorkSize.x * 0.5f,
                       mv->WorkPos.y + mv->WorkSize.y - 55.0f);
            ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(0.5f, 1.0f));
            ImGui::SetNextWindowBgAlpha(0.85f * alpha);
            ImGuiWindowFlags toast_flags =
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
            if (ImGui::Begin("##toast", nullptr, toast_flags)) {
                ImGui::TextUnformatted(app_state.notification_text.c_str());
                if (app_state.has_backup && !app_state.multiplayer) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Restaurar")) {
                        Board& b = app_state.automaton.board();
                        if (static_cast<int>(app_state.grid_backup.size()) == b.size()) {
                            std::copy(app_state.grid_backup.begin(),
                                      app_state.grid_backup.end(), b.data());
                            app_state.automaton.mark_all_dirty();
                            app_state.grid = app_state.automaton.grid();
                        }
                        app_state.has_backup = false;
                        app_state.notify("Grilla anterior restaurada");
                    }
                }
            }
            ImGui::End();
        }
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

    // Liberar las texturas de los previews ANTES de destruir el renderer.
    g_previews.clear();

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();
}