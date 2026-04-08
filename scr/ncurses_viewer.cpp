/**
 * ncurses_viewer.cpp - Cliente terminal para Conway's Game of Life
 * 
 * Características:
 * - Colores por jugador
 * - Double buffering para evitar flicker
 * - Menús retraíbles (ayuda y parámetros)
 * - Control de simulación remota
 * 
 * Controles:
 *   Movimiento:     Flechas = cursor, WASD = viewport
 *   Patrones:       Space = colocar, 1-9 = seleccionar patrón
 *   Simulación:     P = pausar/reanudar, +/- = velocidad, N = step
 *   UI:             H = ayuda, M = menú parámetros, C = limpiar
 *   Conexión:       R = reconectar, Q = salir
 */

#include <ncurses.h>
#include <iostream>
#include <sstream>
#include <cstring>
#include <cerrno>
#include <vector>
#include <chrono>
#include <thread>
#include <algorithm>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include "grid.h"
#include "patterns.h"

// ==================== CONFIGURACIÓN ====================

struct Config {
    std::string server_ip = "127.0.0.1";
    int server_port = 6969;
    bool verbose = false;
    std::string log_file;
};

// ==================== COLORES ====================

// Pares de colores: índice = player_id (0=vacío, 1-8=jugadores)
enum ColorPairs {
    PAIR_EMPTY = 1,
    PAIR_PLAYER1,   // Verde
    PAIR_PLAYER2,   // Azul
    PAIR_PLAYER3,   // Rojo
    PAIR_PLAYER4,   // Amarillo
    PAIR_PLAYER5,   // Magenta
    PAIR_PLAYER6,   // Cyan
    PAIR_PLAYER7,   // Blanco brillante
    PAIR_PLAYER8,   // Blanco
    PAIR_UI_TITLE,
    PAIR_UI_BORDER,
    PAIR_UI_TEXT,
    PAIR_UI_HIGHLIGHT,
    PAIR_CURSOR
};

void init_colors() {
    start_color();
    use_default_colors();
    
    // Colores de jugadores
    init_pair(PAIR_EMPTY, COLOR_BLACK, -1);
    init_pair(PAIR_PLAYER1, COLOR_GREEN, -1);
    init_pair(PAIR_PLAYER2, COLOR_BLUE, -1);
    init_pair(PAIR_PLAYER3, COLOR_RED, -1);
    init_pair(PAIR_PLAYER4, COLOR_YELLOW, -1);
    init_pair(PAIR_PLAYER5, COLOR_MAGENTA, -1);
    init_pair(PAIR_PLAYER6, COLOR_CYAN, -1);
    init_pair(PAIR_PLAYER7, COLOR_WHITE, -1);
    init_pair(PAIR_PLAYER8, COLOR_WHITE, -1);
    
    // UI
    init_pair(PAIR_UI_TITLE, COLOR_CYAN, -1);
    init_pair(PAIR_UI_BORDER, COLOR_WHITE, -1);
    init_pair(PAIR_UI_TEXT, COLOR_WHITE, -1);
    init_pair(PAIR_UI_HIGHLIGHT, COLOR_BLACK, COLOR_WHITE);
    init_pair(PAIR_CURSOR, COLOR_BLACK, COLOR_GREEN);
}

int get_player_color_pair(int player_id) {
    if (player_id <= 0) return PAIR_EMPTY;
    if (player_id > 8) player_id = ((player_id - 1) % 8) + 1;
    return PAIR_PLAYER1 + player_id - 1;
}

// ==================== ESTADO ====================

struct ViewerState {
    // Grilla local
    CellValue** grid = nullptr;
    int rows = GRID_ROWS;
    int cols = GRID_COLS;
    
    // Viewport
    int view_row = 0;
    int view_col = 0;
    int cursor_row = 0;
    int cursor_col = 0;
    
    // Conexión
    int socket_fd = -1;
    bool connected = false;
    
    // Configuración del servidor
    int frecuencia = 10;
    bool sim_running = true;
    int player_id = 1;
    int num_clients = 1;
    
    // UI
    bool show_help = false;
    bool show_params = true;
    int current_pattern = 0;
    
    // Estadísticas
    uint64_t updates_received = 0;
    
    ViewerState() {
        grid = construct_grid(rows, cols);
    }
    
    ~ViewerState() {
        if (grid) destructor_grid(grid, rows, cols);
        if (socket_fd >= 0) close(socket_fd);
    }
};

// Lista de patrones disponibles
const char* PATTERN_NAMES[] = {
    "point", "block", "beehive", "loaf", "boat", "flower",
    "blinker", "toad", "beacon", "pulsar",
    "glider", "lwss", "mwss", "hwss", "glider_gun"
};
const int NUM_PATTERNS = sizeof(PATTERN_NAMES) / sizeof(PATTERN_NAMES[0]);

// ==================== CONEXIÓN ====================

bool connect_to_server(ViewerState& state, const Config& config) {
    state.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (state.socket_fd < 0) {
        return false;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config.server_port);
    
    if (inet_pton(AF_INET, config.server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        close(state.socket_fd);
        state.socket_fd = -1;
        return false;
    }
    
    if (connect(state.socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(state.socket_fd);
        state.socket_fd = -1;
        return false;
    }
    
    // Socket no bloqueante
    fcntl(state.socket_fd, F_SETFL, O_NONBLOCK);
    
    // Limpiar grilla local
    for (int i = 0; i < state.rows; i++) {
        for (int j = 0; j < state.cols; j++) {
            state.grid[i][j] = CELL_DEAD;
        }
    }
    
    state.connected = true;
    
    // Pedir configuración inicial
    const char* cmd = "GET_CONFIG";
    send(state.socket_fd, cmd, strlen(cmd), MSG_NOSIGNAL);
    
    return true;
}

void disconnect(ViewerState& state) {
    if (state.socket_fd >= 0) {
        close(state.socket_fd);
        state.socket_fd = -1;
    }
    state.connected = false;
}

void send_command(ViewerState& state, const std::string& cmd) {
    if (!state.connected) return;
    send(state.socket_fd, cmd.c_str(), cmd.size(), MSG_NOSIGNAL);
}

// ==================== RECEPCIÓN ====================

void receive_updates(ViewerState& state) {
    if (!state.connected) return;
    
    char buffer[8192];
    ssize_t received = recv(state.socket_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (received < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            disconnect(state);
        }
        return;
    }
    
    if (received == 0) {
        disconnect(state);
        return;
    }
    
    buffer[received] = '\0';
    
    std::stringstream ss(buffer);
    std::string line;
    
    while (std::getline(ss, line)) {
        if (line == "GRID_UPDATE" || line == "CONFIG_UPDATE") continue;
        if (line == "END") continue;
        if (line.empty()) continue;
        
        // Parsear actualización de grilla: <row> <col> <state>
        int row, col, cell_state;
        if (sscanf(line.c_str(), "%d %d %d", &row, &col, &cell_state) == 3) {
            if (row >= 0 && row < state.rows && col >= 0 && col < state.cols) {
                state.grid[row][col] = static_cast<CellValue>(cell_state);
                state.updates_received++;
            }
            continue;
        }
        
        // Parsear configuración
        int value;
        if (sscanf(line.c_str(), "FREQ %d", &value) == 1) {
            state.frecuencia = value;
        } else if (sscanf(line.c_str(), "RUN %d", &value) == 1) {
            state.sim_running = (value != 0);
        } else if (sscanf(line.c_str(), "PLAYER_ID %d", &value) == 1) {
            state.player_id = value;
        } else if (sscanf(line.c_str(), "CLIENTS %d", &value) == 1) {
            state.num_clients = value;
        }
    }
}

// ==================== RENDERIZADO ====================

void draw_box(WINDOW* win, int y, int x, int h, int w, const char* title) {
    wattron(win, COLOR_PAIR(PAIR_UI_BORDER));
    
    // Esquinas
    mvwaddch(win, y, x, ACS_ULCORNER);
    mvwaddch(win, y, x + w - 1, ACS_URCORNER);
    mvwaddch(win, y + h - 1, x, ACS_LLCORNER);
    mvwaddch(win, y + h - 1, x + w - 1, ACS_LRCORNER);
    
    // Bordes horizontales
    for (int i = 1; i < w - 1; i++) {
        mvwaddch(win, y, x + i, ACS_HLINE);
        mvwaddch(win, y + h - 1, x + i, ACS_HLINE);
    }
    
    // Bordes verticales
    for (int i = 1; i < h - 1; i++) {
        mvwaddch(win, y + i, x, ACS_VLINE);
        mvwaddch(win, y + i, x + w - 1, ACS_VLINE);
    }
    
    // Título
    if (title && strlen(title) > 0) {
        wattron(win, COLOR_PAIR(PAIR_UI_TITLE) | A_BOLD);
        mvwprintw(win, y, x + 2, " %s ", title);
        wattroff(win, A_BOLD);
    }
    
    wattroff(win, COLOR_PAIR(PAIR_UI_BORDER));
}

void render_help(WINDOW* win, int start_y, int start_x) {
    int w = 36, h = 18;
    draw_box(win, start_y, start_x, h, w, "Ayuda [H]");
    
    wattron(win, COLOR_PAIR(PAIR_UI_TEXT));
    int y = start_y + 2;
    
    mvwprintw(win, y++, start_x + 2, "Movimiento:");
    mvwprintw(win, y++, start_x + 4, "Flechas  Mover cursor");
    mvwprintw(win, y++, start_x + 4, "WASD     Mover viewport");
    y++;
    mvwprintw(win, y++, start_x + 2, "Patrones:");
    mvwprintw(win, y++, start_x + 4, "Space    Colocar patron");
    mvwprintw(win, y++, start_x + 4, "1-9,0    Seleccionar");
    y++;
    mvwprintw(win, y++, start_x + 2, "Simulacion:");
    mvwprintw(win, y++, start_x + 4, "P        Pausar/Reanudar");
    mvwprintw(win, y++, start_x + 4, "+/-      Velocidad");
    mvwprintw(win, y++, start_x + 4, "N        Step (pausado)");
    mvwprintw(win, y++, start_x + 4, "C        Limpiar grilla");
    y++;
    mvwprintw(win, y++, start_x + 2, "R=Reconectar  Q=Salir");
    
    wattroff(win, COLOR_PAIR(PAIR_UI_TEXT));
}

void render_params(WINDOW* win, ViewerState& state, int start_y, int start_x) {
    int w = 28, h = 12;
    draw_box(win, start_y, start_x, h, w, "Params [M]");
    
    wattron(win, COLOR_PAIR(PAIR_UI_TEXT));
    int y = start_y + 2;
    
    // Estado de conexión
    if (state.connected) {
        wattron(win, COLOR_PAIR(PAIR_PLAYER1));
        mvwprintw(win, y++, start_x + 2, "* Conectado");
        wattroff(win, COLOR_PAIR(PAIR_PLAYER1));
    } else {
        wattron(win, COLOR_PAIR(PAIR_PLAYER3));
        mvwprintw(win, y++, start_x + 2, "x Desconectado");
        wattroff(win, COLOR_PAIR(PAIR_PLAYER3));
    }
    
    // Simulación
    mvwprintw(win, y++, start_x + 2, "Sim: %s", state.sim_running ? "ON " : "OFF");
    mvwprintw(win, y++, start_x + 2, "Freq: %d Hz", state.frecuencia);
    
    // Jugador
    int cp = get_player_color_pair(state.player_id);
    wattron(win, COLOR_PAIR(cp));
    mvwprintw(win, y++, start_x + 2, "Player: %d", state.player_id);
    wattroff(win, COLOR_PAIR(cp));
    
    mvwprintw(win, y++, start_x + 2, "Clientes: %d", state.num_clients);
    
    y++;
    
    // Patrón actual
    mvwprintw(win, y++, start_x + 2, "Patron:");
    wattron(win, A_BOLD);
    mvwprintw(win, y++, start_x + 4, "%s", PATTERN_NAMES[state.current_pattern]);
    wattroff(win, A_BOLD);
    
    wattroff(win, COLOR_PAIR(PAIR_UI_TEXT));
}

void render_grid(WINDOW* win, ViewerState& state, int max_rows, int max_cols) {
    // Calcular área visible
    int visible_rows = std::min(max_rows, state.rows - state.view_row);
    int visible_cols = std::min(max_cols, state.cols - state.view_col);
    
    for (int i = 0; i < visible_rows; i++) {
        for (int j = 0; j < visible_cols; j++) {
            int grid_row = state.view_row + i;
            int grid_col = state.view_col + j;
            
            CellValue cell = state.grid[grid_row][grid_col];
            
            // Cursor
            bool is_cursor = (grid_row == state.cursor_row && grid_col == state.cursor_col);
            
            if (is_cursor) {
                wattron(win, COLOR_PAIR(PAIR_CURSOR));
                mvwaddch(win, i, j, cell != CELL_DEAD ? '#' : '+');
                wattroff(win, COLOR_PAIR(PAIR_CURSOR));
            } else if (cell != CELL_DEAD) {
                int cp = get_player_color_pair(cell);
                wattron(win, COLOR_PAIR(cp) | A_BOLD);
                mvwaddch(win, i, j, '#');
                wattroff(win, COLOR_PAIR(cp) | A_BOLD);
            } else {
                wattron(win, COLOR_PAIR(PAIR_EMPTY));
                mvwaddch(win, i, j, '.');
                wattroff(win, COLOR_PAIR(PAIR_EMPTY));
            }
        }
    }
}

void render_status_bar(WINDOW* win, ViewerState& state, int y, int width) {
    wattron(win, COLOR_PAIR(PAIR_UI_HIGHLIGHT));
    
    // Limpiar línea
    mvwhline(win, y, 0, ' ', width);
    
    // Info de posición
    mvwprintw(win, y, 1, "Pos: %d,%d | View: %d,%d | Updates: %lu", 
              state.cursor_row, state.cursor_col,
              state.view_row, state.view_col,
              state.updates_received);
    
    // Indicadores a la derecha
    int right_pos = width - 20;
    if (right_pos > 0) {
        mvwprintw(win, y, right_pos, "[H]elp [M]enu [Q]uit");
    }
    
    wattroff(win, COLOR_PAIR(PAIR_UI_HIGHLIGHT));
}

// ==================== INPUT ====================

void handle_input(ViewerState& state, int ch, int grid_height, int grid_width) {
    switch (ch) {
        // Movimiento del cursor
        case KEY_UP:
            if (state.cursor_row > 0) state.cursor_row--;
            break;
        case KEY_DOWN:
            if (state.cursor_row < state.rows - 1) state.cursor_row++;
            break;
        case KEY_LEFT:
            if (state.cursor_col > 0) state.cursor_col--;
            break;
        case KEY_RIGHT:
            if (state.cursor_col < state.cols - 1) state.cursor_col++;
            break;
            
        // Movimiento del viewport
        case 'w': case 'W':
            if (state.view_row > 0) state.view_row -= 5;
            if (state.view_row < 0) state.view_row = 0;
            break;
        case 's': case 'S':
            state.view_row += 5;
            if (state.view_row > state.rows - grid_height) 
                state.view_row = state.rows - grid_height;
            if (state.view_row < 0) state.view_row = 0;
            break;
        case 'a': case 'A':
            if (state.view_col > 0) state.view_col -= 5;
            if (state.view_col < 0) state.view_col = 0;
            break;
        case 'd': case 'D':
            state.view_col += 5;
            if (state.view_col > state.cols - grid_width)
                state.view_col = state.cols - grid_width;
            if (state.view_col < 0) state.view_col = 0;
            break;
            
        // Colocar patrón
        case ' ':
            if (state.connected) {
                std::stringstream cmd;
                cmd << "ADD_PATTERN " << PATTERN_NAMES[state.current_pattern] 
                    << " " << state.cursor_row 
                    << " " << state.cursor_col
                    << " " << state.player_id;
                send_command(state, cmd.str());
            }
            break;
            
        // Selección de patrón (1-9, 0)
        case '1': case '2': case '3': case '4': case '5':
        case '6': case '7': case '8': case '9':
            state.current_pattern = (ch - '1') % NUM_PATTERNS;
            break;
        case '0':
            state.current_pattern = 9 % NUM_PATTERNS;
            break;
            
        // Control de simulación
        case 'p': case 'P':
            state.sim_running = !state.sim_running;
            send_command(state, state.sim_running ? "SET_RUN 1" : "SET_RUN 0");
            break;
            
        case '+': case '=':
            if (state.frecuencia < 60) {
                state.frecuencia += 5;
                send_command(state, "SET_FREQ " + std::to_string(state.frecuencia));
            }
            break;
            
        case '-': case '_':
            if (state.frecuencia > 1) {
                state.frecuencia -= 5;
                if (state.frecuencia < 1) state.frecuencia = 1;
                send_command(state, "SET_FREQ " + std::to_string(state.frecuencia));
            }
            break;
            
        case 'n': case 'N':
            if (!state.sim_running) {
                send_command(state, "STEP");
            }
            break;
            
        case 'c': case 'C':
            send_command(state, "CLEAR");
            break;
            
        // UI toggles
        case 'h': case 'H':
            state.show_help = !state.show_help;
            break;
            
        case 'm': case 'M':
            state.show_params = !state.show_params;
            break;
    }
    
    // Ajustar viewport para seguir al cursor
    if (state.cursor_row < state.view_row) {
        state.view_row = state.cursor_row;
    }
    if (state.cursor_row >= state.view_row + grid_height) {
        state.view_row = state.cursor_row - grid_height + 1;
    }
    if (state.cursor_col < state.view_col) {
        state.view_col = state.cursor_col;
    }
    if (state.cursor_col >= state.view_col + grid_width) {
        state.view_col = state.cursor_col - grid_width + 1;
    }
}

// ==================== MAIN ====================

void show_usage(const char* program) {
    std::cout << "\nVisor ncurses para Conway's Game of Life\n";
    std::cout << "=========================================\n\n";
    std::cout << "Uso: " << program << " [opciones]\n\n";
    std::cout << "Opciones:\n";
    std::cout << "  -s, --server IP    IP del servidor (default: 127.0.0.1)\n";
    std::cout << "  -p, --port PORT    Puerto (default: 6969)\n";
    std::cout << "  -v, --verbose      Modo verbose\n";
    std::cout << "  -h, --help         Mostrar ayuda\n\n";
}

int main(int argc, char* argv[]) {
    Config config;
    
    // Parsear argumentos
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            show_usage(argv[0]);
            return 0;
        }
        else if ((arg == "-s" || arg == "--server") && i + 1 < argc) {
            config.server_ip = argv[++i];
        }
        else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            config.server_port = std::stoi(argv[++i]);
        }
        else if (arg == "-v" || arg == "--verbose") {
            config.verbose = true;
        }
    }
    
    // Inicializar ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);  // Non-blocking input
    curs_set(0);            // Ocultar cursor del sistema
    
    if (has_colors()) {
        init_colors();
    }
    
    // Estado
    ViewerState state;
    
    // Conectar
    if (!connect_to_server(state, config)) {
        endwin();
        std::cerr << "Error: No se pudo conectar a " 
                  << config.server_ip << ":" << config.server_port << std::endl;
        return 1;
    }
    
    // Loop principal con double buffering
    bool running = true;
    auto last_render = std::chrono::steady_clock::now();
    const auto render_interval = std::chrono::milliseconds(33);  // ~30 FPS
    
    while (running) {
        // Recibir actualizaciones del servidor
        receive_updates(state);
        
        // Procesar input
        int ch = getch();
        while (ch != ERR) {
            if (ch == 'q' || ch == 'Q') {
                running = false;
                break;
            }
            else if (ch == 'r' || ch == 'R') {
                disconnect(state);
                connect_to_server(state, config);
            }
            else {
                int max_rows, max_cols;
                getmaxyx(stdscr, max_rows, max_cols);
                handle_input(state, ch, max_rows - 1, max_cols);
            }
            ch = getch();
        }
        
        // Renderizar (con rate limiting)
        auto now = std::chrono::steady_clock::now();
        if (now - last_render >= render_interval) {
            last_render = now;
            
            int max_rows, max_cols;
            getmaxyx(stdscr, max_rows, max_cols);
            
            // Limpiar pantalla (erase es más rápido que clear)
            erase();
            
            // Calcular espacio para UI
            int ui_width = 0;
            if (state.show_params) ui_width = std::max(ui_width, 30);
            if (state.show_help) ui_width = std::max(ui_width, 38);
            
            int grid_width = max_cols - ui_width;
            int grid_height = max_rows - 1;  // Reservar 1 línea para status bar
            
            // Renderizar grilla
            render_grid(stdscr, state, grid_height, grid_width);
            
            // Renderizar UI (lado derecho)
            int ui_x = grid_width + 1;
            int ui_y = 0;
            
            if (state.show_params) {
                render_params(stdscr, state, ui_y, ui_x);
                ui_y += 13;
            }
            
            if (state.show_help) {
                render_help(stdscr, ui_y, ui_x);
            }
            
            // Barra de estado
            render_status_bar(stdscr, state, max_rows - 1, max_cols);
            
            // Actualizar pantalla
            refresh();
        }
        
        // Pequeña pausa para no saturar CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    // Limpiar
    endwin();
    
    std::cout << "Desconectado. Updates recibidos: " << state.updates_received << std::endl;
    
    return 0;
}
