/**
 * Conway's Game of Life - Wallpaper Mode v5 (Optimizado)
 * 
 * Cambios vs v4:
 *   - GNOME: Frecuencia de wallpaper reducida (2 FPS por defecto, simulación sigue a 15 Hz)
 *   - GNOME: gsettings asíncrono (no bloquea el main loop)
 *   - Framebuffer optimizado con memset para fondo
 *   - Opción --fps para ajustar frecuencia de renderizado
 * 
 * Uso:
 *   ./life_wallpaper                    # Auto-detecta
 *   ./life_wallpaper --gnome            # Forzar GNOME
 *   ./life_wallpaper --gnome --fps 5    # GNOME a 5 FPS (más suave pero más I/O)
 *   ./life_wallpaper --windowed         # Testing
 */

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <random>
#include <chrono>
#include <thread>
#include <csignal>
#include <vector>
#include <fstream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <SDL2/SDL.h>

#include "grid.h"
#include "patterns.h"

using namespace std;

// ============================================================================
// Configuración
// ============================================================================
constexpr int WALLPAPER_ROWS = 200;
constexpr int WALLPAPER_COLS = 400;
constexpr float WALLPAPER_ZOOM = 0.80f;
constexpr int SIMULATION_HZ = 15;           // Simulación siempre a 15 Hz
constexpr int GNOME_DEFAULT_FPS = 2;        // GNOME: 2 FPS por defecto (mucho más liviano)
constexpr int X11_DEFAULT_FPS = 15;         // X11: 15 FPS
constexpr int SPAWN_INTERVAL_MS = 2000;
constexpr int MAX_ALIVE_CELLS = 8000;

// Colores (igual que main.cpp)
constexpr uint8_t BG_R = 20, BG_G = 20, BG_B = 25;
constexpr uint8_t GRID_R = 40, GRID_G = 40, GRID_B = 40;

enum class RenderMode { AUTO, GNOME, X11, WINDOWED };

// ============================================================================
// Variables Globales
// ============================================================================
CellValue** grid = nullptr;
volatile bool running = true;
RenderMode render_mode = RenderMode::AUTO;
int render_fps = 0;  // 0 = usar default según modo

// X11
Display* x_display = nullptr;
Window x_root = 0;
GC x_gc = nullptr;
XImage* x_image = nullptr;
Pixmap x_pixmap = 0;
int screen_width = 0;
int screen_height = 0;
int screen_depth = 0;
std::vector<uint32_t> framebuffer;

// SDL2
SDL_Window* sdl_window = nullptr;
SDL_Renderer* sdl_renderer = nullptr;

// GNOME
std::string temp_image_path_a;
std::string temp_image_path_b;
bool use_image_a = true;
std::string original_wallpaper;
pid_t gsettings_pid = 0;  // Para no esperar a que termine

viewpoint vp = {
    .center_x = 0.50f,
    .center_y = 0.50f,
    .zoom = WALLPAPER_ZOOM
};

std::mt19937 rng(std::random_device{}());

const std::vector<std::string> SPAWN_PATTERNS = {
    "glider", "lwss", "mwss", "hwss",
    "blinker", "toad", "beacon", "pulsar",
    "block", "beehive", "loaf", "boat"
};

const std::vector<int> PATTERN_WEIGHTS = {
    10, 8, 6, 4, 5, 5, 5, 3, 2, 2, 2, 2
};

// ============================================================================
// Utilidades
// ============================================================================

void signal_handler(int signum) {
    cout << "\n[WALLPAPER] Cerrando..." << endl;
    running = false;
}

bool is_gnome() {
    const char* desktop = getenv("XDG_CURRENT_DESKTOP");
    if (desktop && (strstr(desktop, "GNOME") || strstr(desktop, "Unity"))) return true;
    const char* session = getenv("DESKTOP_SESSION");
    if (session && (strstr(session, "gnome") || strstr(session, "ubuntu"))) return true;
    return false;
}

int count_alive_cells() {
    int count = 0;
    for (int i = 0; i < WALLPAPER_ROWS; i++)
        for (int j = 0; j < WALLPAPER_COLS; j++)
            if (grid[i][j] != CELL_DEAD) count++;
    return count;
}

std::string select_random_pattern() {
    int total = 0;
    for (int w : PATTERN_WEIGHTS) total += w;
    std::uniform_int_distribution<int> dist(0, total - 1);
    int roll = dist(rng);
    int cum = 0;
    for (size_t i = 0; i < SPAWN_PATTERNS.size(); i++) {
        cum += PATTERN_WEIGHTS[i];
        if (roll < cum) return SPAWN_PATTERNS[i];
    }
    return SPAWN_PATTERNS[0];
}

void spawn_random_pattern() {
    if (count_alive_cells() > MAX_ALIVE_CELLS) return;
    std::string pattern = select_random_pattern();
    CellValue player_id = 1 + (rng() % NUM_PLAYER_COLORS);
    int row = 10 + (rng() % (WALLPAPER_ROWS - 30));
    int col = 10 + (rng() % (WALLPAPER_COLS - 50));
    load_pattern_into_grid(pattern, grid, row, col, WALLPAPER_ROWS, WALLPAPER_COLS, player_id);
}

void spawn_initial_patterns() {
    for (int i = 0; i < 15; i++) spawn_random_pattern();
    load_pattern_into_grid("glider_gun", grid, 20, 20, WALLPAPER_ROWS, WALLPAPER_COLS, 1);
    load_pattern_into_grid("glider_gun", grid, WALLPAPER_ROWS - 40, WALLPAPER_COLS - 60, 
                           WALLPAPER_ROWS, WALLPAPER_COLS, 5);
}

// ============================================================================
// Framebuffer Rendering (Optimizado)
// ============================================================================

inline uint32_t rgb_to_pixel(uint8_t r, uint8_t g, uint8_t b) {
    return (0xFF << 24) | (r << 16) | (g << 8) | b;
}

void render_to_framebuffer(uint32_t* fb, int fb_width, int fb_height) {
    // Fondo: usar fill optimizado
    uint32_t bg = rgb_to_pixel(BG_R, BG_G, BG_B);
    std::fill(fb, fb + fb_width * fb_height, bg);
    
    VisibleRange row_range = get_visible_rows(&vp, WALLPAPER_ROWS, WALLPAPER_COLS);
    VisibleRange col_range = get_visible_columns(&vp, WALLPAPER_ROWS, WALLPAPER_COLS);
    
    int visible_rows = row_range.end - row_range.start;
    int visible_cols = col_range.end - col_range.start;
    if (visible_rows <= 0 || visible_cols <= 0) return;
    
    float cell_w = static_cast<float>(fb_width) / visible_cols;
    float cell_h = static_cast<float>(fb_height) / visible_rows;
    
    // Celdas vivas
    for (int i = row_range.start; i < row_range.end; i++) {
        for (int j = col_range.start; j < col_range.end; j++) {
            if (grid[i][j] != CELL_DEAD) {
                SDL_Color c = get_player_color(grid[i][j]);
                uint32_t pixel = rgb_to_pixel(c.r, c.g, c.b);
                
                int x0 = (j - col_range.start) * cell_w;
                int y0 = (i - row_range.start) * cell_h;
                int x1 = min((int)((j - col_range.start + 1) * cell_w), fb_width);
                int y1 = min((int)((i - row_range.start + 1) * cell_h), fb_height);
                
                for (int py = y0; py < y1; py++) {
                    uint32_t* row_ptr = fb + py * fb_width + x0;
                    std::fill(row_ptr, row_ptr + (x1 - x0), pixel);
                }
            }
        }
    }
    
    // Líneas de grilla
    if (cell_w > 4 && cell_h > 4) {
        uint32_t grid_color = rgb_to_pixel(GRID_R, GRID_G, GRID_B);
        
        for (int i = 0; i <= visible_rows; i++) {
            int y = static_cast<int>(i * cell_h);
            if (y >= 0 && y < fb_height)
                std::fill(fb + y * fb_width, fb + y * fb_width + fb_width, grid_color);
        }
        
        for (int j = 0; j <= visible_cols; j++) {
            int x = static_cast<int>(j * cell_w);
            if (x >= 0 && x < fb_width)
                for (int y = 0; y < fb_height; y++)
                    fb[y * fb_width + x] = grid_color;
        }
    }
}

// ============================================================================
// GNOME (Optimizado)
// ============================================================================

bool save_bmp(const std::string& path, uint32_t* pixels, int w, int h) {
    FILE* file = fopen(path.c_str(), "wb");
    if (!file) return false;
    
    int row_size = ((w * 3 + 3) / 4) * 4;
    int data_size = row_size * h;
    int file_size = 54 + data_size;
    
    uint8_t header[54] = {
        'B', 'M',
        (uint8_t)(file_size), (uint8_t)(file_size >> 8), 
        (uint8_t)(file_size >> 16), (uint8_t)(file_size >> 24),
        0, 0, 0, 0, 54, 0, 0, 0, 40, 0, 0, 0,
        (uint8_t)(w), (uint8_t)(w >> 8), (uint8_t)(w >> 16), (uint8_t)(w >> 24),
        (uint8_t)(h), (uint8_t)(h >> 8), (uint8_t)(h >> 16), (uint8_t)(h >> 24),
        1, 0, 24, 0, 0, 0, 0, 0,
        (uint8_t)(data_size), (uint8_t)(data_size >> 8), 
        (uint8_t)(data_size >> 16), (uint8_t)(data_size >> 24),
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    
    fwrite(header, 1, 54, file);
    
    // Escribir rows (BMP es bottom-up, BGR)
    std::vector<uint8_t> row(row_size, 0);
    for (int y = h - 1; y >= 0; y--) {
        uint32_t* src = pixels + y * w;
        for (int x = 0; x < w; x++) {
            uint32_t p = src[x];
            row[x * 3 + 0] = p & 0xFF;         // B
            row[x * 3 + 1] = (p >> 8) & 0xFF;  // G
            row[x * 3 + 2] = (p >> 16) & 0xFF; // R
        }
        fwrite(row.data(), 1, row_size, file);
    }
    
    fclose(file);
    return true;
}

// gsettings asíncrono (no bloquea)
void gsettings_async(const std::string& path) {
    // Esperar proceso anterior si existe
    if (gsettings_pid > 0) {
        int status;
        waitpid(gsettings_pid, &status, WNOHANG);  // No bloqueante
    }
    
    gsettings_pid = fork();
    if (gsettings_pid == 0) {
        // Proceso hijo: ejecutar gsettings silenciosamente
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        
        std::string uri = "file://" + path;
        execlp("gsettings", "gsettings", "set", 
               "org.gnome.desktop.background", "picture-uri", uri.c_str(), nullptr);
        _exit(1);
    }
    // Padre continúa inmediatamente
}

bool init_gnome() {
    // Obtener resolución
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        cerr << "[ERROR] No se pudo abrir display" << endl;
        return false;
    }
    
    int scr = DefaultScreen(dpy);
    screen_width = DisplayWidth(dpy, scr);
    screen_height = DisplayHeight(dpy, scr);
    XCloseDisplay(dpy);
    
    cout << "[GNOME] Pantalla: " << screen_width << "x" << screen_height << endl;
    
    // Archivos temporales
    mkdir("/tmp/life_wallpaper", 0755);
    temp_image_path_a = "/tmp/life_wallpaper/frame_a.bmp";
    temp_image_path_b = "/tmp/life_wallpaper/frame_b.bmp";
    
    // Guardar wallpaper original
    FILE* pipe = popen("gsettings get org.gnome.desktop.background picture-uri 2>/dev/null", "r");
    if (pipe) {
        char buf[512];
        if (fgets(buf, sizeof(buf), pipe)) {
            original_wallpaper = buf;
            size_t start = original_wallpaper.find_first_not_of("' \n\r");
            size_t end = original_wallpaper.find_last_not_of("' \n\r");
            if (start != string::npos && end != string::npos)
                original_wallpaper = original_wallpaper.substr(start, end - start + 1);
        }
        pclose(pipe);
    }
    
    if (!original_wallpaper.empty())
        cout << "[GNOME] Wallpaper original guardado" << endl;
    
    framebuffer.resize(screen_width * screen_height);
    
    // Configurar gsettings
    system("gsettings set org.gnome.desktop.background picture-options 'stretched' 2>/dev/null");
    
    return true;
}

void render_gnome() {
    render_to_framebuffer(framebuffer.data(), screen_width, screen_height);
    
    const std::string& path = use_image_a ? temp_image_path_a : temp_image_path_b;
    use_image_a = !use_image_a;
    
    if (save_bmp(path, framebuffer.data(), screen_width, screen_height)) {
        gsettings_async(path);
    }
}

void cleanup_gnome() {
    // Esperar gsettings pendiente
    if (gsettings_pid > 0) {
        waitpid(gsettings_pid, nullptr, 0);
    }
    
    // Restaurar wallpaper original
    if (!original_wallpaper.empty()) {
        cout << "[GNOME] Restaurando wallpaper..." << endl;
        std::string cmd = "gsettings set org.gnome.desktop.background picture-uri '" 
                          + original_wallpaper + "' 2>/dev/null";
        system(cmd.c_str());
    }
    
    unlink(temp_image_path_a.c_str());
    unlink(temp_image_path_b.c_str());
    rmdir("/tmp/life_wallpaper");
}

// ============================================================================
// X11 Root Window
// ============================================================================

bool init_x11() {
    x_display = XOpenDisplay(nullptr);
    if (!x_display) return false;
    
    int scr = DefaultScreen(x_display);
    x_root = RootWindow(x_display, scr);
    screen_width = DisplayWidth(x_display, scr);
    screen_height = DisplayHeight(x_display, scr);
    screen_depth = DefaultDepth(x_display, scr);
    
    cout << "[X11] Pantalla: " << screen_width << "x" << screen_height << endl;
    
    x_gc = XCreateGC(x_display, x_root, 0, nullptr);
    framebuffer.resize(screen_width * screen_height);
    
    Visual* vis = DefaultVisual(x_display, scr);
    x_image = XCreateImage(x_display, vis, screen_depth, ZPixmap, 0,
                           (char*)framebuffer.data(), screen_width, screen_height, 32, 0);
    
    if (!x_image) return false;
    
    x_pixmap = XCreatePixmap(x_display, x_root, screen_width, screen_height, screen_depth);
    return true;
}

void render_x11() {
    render_to_framebuffer(framebuffer.data(), screen_width, screen_height);
    XPutImage(x_display, x_pixmap, x_gc, x_image, 0, 0, 0, 0, screen_width, screen_height);
    XSetWindowBackgroundPixmap(x_display, x_root, x_pixmap);
    XClearWindow(x_display, x_root);
    XFlush(x_display);
}

void cleanup_x11() {
    if (x_pixmap) XFreePixmap(x_display, x_pixmap);
    if (x_image) { x_image->data = nullptr; XDestroyImage(x_image); }
    if (x_gc) XFreeGC(x_display, x_gc);
    if (x_display) XCloseDisplay(x_display);
}

// ============================================================================
// SDL2 Windowed
// ============================================================================

bool init_sdl() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return false;
    
    sdl_window = SDL_CreateWindow("Conway Wallpaper (Testing)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!sdl_window) return false;
    
    sdl_renderer = SDL_CreateRenderer(sdl_window, -1, 
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    return sdl_renderer != nullptr;
}

void render_sdl() {
    int w, h;
    SDL_GetWindowSize(sdl_window, &w, &h);
    
    SDL_SetRenderDrawColor(sdl_renderer, BG_R, BG_G, BG_B, 255);
    SDL_RenderClear(sdl_renderer);
    
    VisibleRange rr = get_visible_rows(&vp, WALLPAPER_ROWS, WALLPAPER_COLS);
    VisibleRange cr = get_visible_columns(&vp, WALLPAPER_ROWS, WALLPAPER_COLS);
    
    int vr = rr.end - rr.start, vc = cr.end - cr.start;
    if (vr <= 0 || vc <= 0) { SDL_RenderPresent(sdl_renderer); return; }
    
    float cw = (float)w / vc, ch = (float)h / vr;
    
    for (int i = rr.start; i < rr.end; i++) {
        for (int j = cr.start; j < cr.end; j++) {
            if (grid[i][j] != CELL_DEAD) {
                SDL_Color c = get_player_color(grid[i][j]);
                SDL_SetRenderDrawColor(sdl_renderer, c.r, c.g, c.b, 255);
                SDL_FRect rect = {(j - cr.start) * cw, (i - rr.start) * ch, cw, ch};
                SDL_RenderFillRectF(sdl_renderer, &rect);
            }
        }
    }
    
    if (cw > 4 && ch > 4) {
        SDL_SetRenderDrawColor(sdl_renderer, GRID_R, GRID_G, GRID_B, 255);
        for (int i = 0; i <= vr; i++) SDL_RenderDrawLine(sdl_renderer, 0, i*ch, w, i*ch);
        for (int j = 0; j <= vc; j++) SDL_RenderDrawLine(sdl_renderer, j*cw, 0, j*cw, h);
    }
    
    SDL_RenderPresent(sdl_renderer);
}

void cleanup_sdl() {
    if (sdl_renderer) SDL_DestroyRenderer(sdl_renderer);
    if (sdl_window) SDL_DestroyWindow(sdl_window);
    SDL_Quit();
}

// ============================================================================
// Main
// ============================================================================

void show_help(const char* name) {
    cout << "\nConway's Game of Life - Wallpaper v5 (Optimizado)\n\n";
    cout << "Uso: " << name << " [opciones]\n\n";
    cout << "  --gnome        Forzar modo GNOME\n";
    cout << "  --x11          Forzar modo X11\n";
    cout << "  --windowed     Modo ventana (testing)\n";
    cout << "  --fps N        Frames por segundo para renderizado (default: GNOME=2, X11=15)\n";
    cout << "  --help         Esta ayuda\n\n";
    cout << "Nota: La simulación siempre corre a 15 Hz internamente.\n";
    cout << "      En GNOME, el wallpaper se actualiza más lento para no saturar el sistema.\n\n";
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--help" || arg == "-h") { show_help(argv[0]); return 0; }
        else if (arg == "--windowed" || arg == "-w") render_mode = RenderMode::WINDOWED;
        else if (arg == "--gnome") render_mode = RenderMode::GNOME;
        else if (arg == "--x11") render_mode = RenderMode::X11;
        else if ((arg == "--fps") && i + 1 < argc) render_fps = atoi(argv[++i]);
    }
    
    // Auto-detectar
    if (render_mode == RenderMode::AUTO) {
        render_mode = is_gnome() ? RenderMode::GNOME : RenderMode::X11;
        cout << "[AUTO] " << (render_mode == RenderMode::GNOME ? "GNOME" : "X11") << endl;
    }
    
    // FPS por defecto según modo
    if (render_fps <= 0) {
        render_fps = (render_mode == RenderMode::GNOME) ? GNOME_DEFAULT_FPS : X11_DEFAULT_FPS;
    }
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    grid = construct_grid(WALLPAPER_ROWS, WALLPAPER_COLS);
    if (!grid) return 1;
    spawn_initial_patterns();
    
    bool ok = false;
    switch (render_mode) {
        case RenderMode::GNOME:    ok = init_gnome(); break;
        case RenderMode::X11:      ok = init_x11(); break;
        case RenderMode::WINDOWED: ok = init_sdl(); break;
        default: break;
    }
    
    if (!ok) {
        destructor_grid(grid, WALLPAPER_ROWS, WALLPAPER_COLS);
        return 1;
    }
    
    cout << "[CONFIG] Simulación: " << SIMULATION_HZ << " Hz" << endl;
    cout << "[CONFIG] Renderizado: " << render_fps << " FPS" << endl;
    cout << "[CONFIG] Ctrl+C para detener" << endl;
    
    auto sim_interval = chrono::milliseconds(1000 / SIMULATION_HZ);
    auto render_interval = chrono::milliseconds(1000 / render_fps);
    auto spawn_interval = chrono::milliseconds(SPAWN_INTERVAL_MS);
    
    auto last_sim = chrono::steady_clock::now();
    auto last_render = chrono::steady_clock::now();
    auto last_spawn = chrono::steady_clock::now();
    
    while (running) {
        auto now = chrono::steady_clock::now();
        
        // Eventos SDL
        if (render_mode == RenderMode::WINDOWED) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
                    running = false;
            }
        }
        
        // Spawn aleatorio
        if (now - last_spawn >= spawn_interval) {
            spawn_random_pattern();
            last_spawn = now;
        }
        
        // Simulación (siempre a SIMULATION_HZ)
        if (now - last_sim >= sim_interval) {
            update_grid(grid, WALLPAPER_ROWS, WALLPAPER_COLS);
            last_sim = now;
        }
        
        // Renderizado (a render_fps, puede ser menor que simulación)
        if (now - last_render >= render_interval) {
            switch (render_mode) {
                case RenderMode::GNOME:    render_gnome(); break;
                case RenderMode::X11:      render_x11(); break;
                case RenderMode::WINDOWED: render_sdl(); break;
                default: break;
            }
            last_render = now;
        }
        
        // Sleep corto para no quemar CPU
        this_thread::sleep_for(chrono::milliseconds(5));
    }
    
    switch (render_mode) {
        case RenderMode::GNOME:    cleanup_gnome(); break;
        case RenderMode::X11:      cleanup_x11(); break;
        case RenderMode::WINDOWED: cleanup_sdl(); break;
        default: break;
    }
    
    destructor_grid(grid, WALLPAPER_ROWS, WALLPAPER_COLS);
    cout << "[WALLPAPER] Finalizado." << endl;
    return 0;
}
