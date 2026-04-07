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

AppState app_state(ROWS, COLS);
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
        case SDL_SCANCODE_A: return ImGuiKey_A;
        case SDL_SCANCODE_E: return ImGuiKey_E;
        case SDL_SCANCODE_L: return ImGuiKey_L;
        case SDL_SCANCODE_M: return ImGuiKey_M;
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
                                          app_state.player_id);
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
            }
        } else {
            ImGui::Text("Servidor: %s:%d", 
                       app_state.server_ip.c_str(), 
                       app_state.server_port);
            ImGui::Text("Tu ID: %d", app_state.player_id);
            
            // Mostrar tu color
            SDL_Color c = get_player_color(app_state.player_id);
            ImVec4 color(c.r/255.0f, c.g/255.0f, c.b/255.0f, 1.0f);
            ImGui::ColorButton("Tu color", color, 0, ImVec2(50, 20));
            
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
        
        auto pattern_button = [&](const char* name, const char* tooltip) {
            ImGui::PushID(name);
            bool selected = (name == app_state.current_pattern);
            
            if (ImGui::ImageButton(
                name,
                (ImTextureID)app_state.patterns[name],
                button_size,
                ImVec2(0,0), ImVec2(1,1),
                ImGui::GetStyle().Colors[ImGuiCol_Button],
                selected ? ImVec4(1,1,1,1) : ImVec4(0.5f, 0.5f, 0.5f, 0.5f)
            )) {
                app_state.current_pattern = selected ? "point" : name;
            }
            
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", tooltip);
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
            
            if (ImGui::ImageButton(
                name,
                (ImTextureID)anim.frames[anim.current_frame],
                size,
                ImVec2(0,0), ImVec2(1,1),
                ImGui::GetStyle().Colors[ImGuiCol_Button],
                selected ? ImVec4(1,1,1,1) : ImVec4(0.5f, 0.5f, 0.5f, 0.5f)
            )) {
                app_state.current_pattern = selected ? "point" : name;
            }
            
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", tooltip);
            }
            ImGui::PopID();
        };

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

    // ============== RENDERIZADO ==============
    SDL_SetRenderDrawColor(renderer, 20, 20, 25, 255);
    SDL_RenderClear(renderer);

    // Renderizar grilla
    print_grid(window, renderer, app_state.grid, vp, app_state.rows, app_state.cols);

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
