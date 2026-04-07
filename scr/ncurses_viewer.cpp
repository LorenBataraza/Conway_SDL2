#include <iostream>
#include <ncurses.h>
#include <sstream>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include <cstring>
#include <cerrno>

// Networking
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "packet_logger.h"

/**
 * Cliente ncurses para visualizar la grilla del Juego de la Vida
 * 
 * Controles:
 *   - Flechas: Mover cursor
 *   - WASD: Mover viewport
 *   - Space: Colocar patrón actual
 *   - 1-9: Seleccionar patrón
 *   - +/-: Zoom in/out
 *   - p: Pausar/reanudar
 *   - q: Salir
 *   - r: Reconectar
 */

// Constantes
constexpr int GRID_ROWS = 100;
constexpr int GRID_COLS = 100;
constexpr int DEFAULT_PORT = 6969;

// Caracteres para visualización
constexpr char CELL_ALIVE = '#';
constexpr char CELL_DEAD = '.';
constexpr char CURSOR_CHAR = 'X';

// Colores
enum Colors {
    COLOR_GRID = 1,
    COLOR_ALIVE,
    COLOR_DEAD,
    COLOR_CURSOR,
    COLOR_STATUS,
    COLOR_ERROR,
    COLOR_INFO
};

// Patrones disponibles
const char* PATTERNS[] = {
    "point", "block", "glider", "blinker", "toad",
    "beacon", "lwss", "pulsar", "glider_gun"
};
constexpr int NUM_PATTERNS = sizeof(PATTERNS) / sizeof(PATTERNS[0]);

/**
 * Estado del cliente ncurses
 */
struct NcursesClientState {
    // Grilla
    bool grid[GRID_ROWS][GRID_COLS] = {false};
    
    // Viewport
    int view_row = 0;
    int view_col = 0;
    int view_height = 40;
    int view_width = 80;
    
    // Cursor
    int cursor_row = GRID_ROWS / 2;
    int cursor_col = GRID_COLS / 2;
    
    // Patrón actual
    int current_pattern_idx = 0;
    
    // Conexión
    int socket_fd = -1;
    bool connected = false;
    std::string server_ip = "127.0.0.1";
    int server_port = DEFAULT_PORT;
    
    // Estado
    std::atomic<bool> running{true};
    bool paused = false;
    
    // Estadísticas
    int packets_sent = 0;
    int packets_received = 0;
    int cells_alive = 0;
    
    // Logger
    PacketLogger* logger = nullptr;
};

/**
 * Inicializa ncurses con colores
 */
void init_ncurses() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);
    
    if (has_colors()) {
        start_color();
        use_default_colors();
        
        init_pair(COLOR_GRID, COLOR_WHITE, -1);
        init_pair(COLOR_ALIVE, COLOR_GREEN, -1);
        init_pair(COLOR_DEAD, COLOR_BLACK, -1);
        init_pair(COLOR_CURSOR, COLOR_RED, -1);
        init_pair(COLOR_STATUS, COLOR_CYAN, -1);
        init_pair(COLOR_ERROR, COLOR_RED, -1);
        init_pair(COLOR_INFO, COLOR_YELLOW, -1);
    }
}

/**
 * Limpia ncurses
 */
void cleanup_ncurses() {
    endwin();
}

/**
 * Conecta al servidor
 */
bool connect_to_server(NcursesClientState& state) {
    state.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (state.socket_fd < 0) {
        return false;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(state.server_port);
    
    if (inet_pton(AF_INET, state.server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        close(state.socket_fd);
        state.socket_fd = -1;
        return false;
    }
    
    if (connect(state.socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(state.socket_fd);
        state.socket_fd = -1;
        return false;
    }
    
    // Configurar socket como no bloqueante
    fcntl(state.socket_fd, F_SETFL, O_NONBLOCK);
    
    state.connected = true;
    
    if (state.logger) {
        state.logger->log_event("CONNECT", "Conectado a " + state.server_ip + ":" + std::to_string(state.server_port));
    }
    
    return true;
}

/**
 * Desconecta del servidor
 */
void disconnect_from_server(NcursesClientState& state) {
    if (state.socket_fd >= 0) {
        close(state.socket_fd);
        state.socket_fd = -1;
    }
    state.connected = false;
    
    if (state.logger) {
        state.logger->log_event("DISCONNECT", "Desconectado del servidor");
    }
}

/**
 * Envía un patrón al servidor
 */
void send_pattern(NcursesClientState& state, const char* pattern, int row, int col) {
    if (!state.connected || state.socket_fd < 0) return;
    
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "ADD_PATTERN %s %d %d", pattern, row, col);
    
    ssize_t sent = send(state.socket_fd, buffer, strlen(buffer), 0);
    if (sent > 0) {
        state.packets_sent++;
        if (state.logger) {
            state.logger->log(PacketDirection::OUTGOING, buffer, sent);
        }
    }
}

/**
 * Recibe actualizaciones del servidor
 */
void receive_updates(NcursesClientState& state) {
    if (!state.connected || state.socket_fd < 0) return;
    
    char buffer[4096];
    ssize_t received = recv(state.socket_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (received <= 0) {
        if (received == 0) {
            // Servidor cerró conexión
            state.connected = false;
            if (state.logger) {
                state.logger->log_event("DISCONNECT", "Servidor cerró la conexión");
            }
        }
        return;
    }
    
    buffer[received] = '\0';
    state.packets_received++;
    
    if (state.logger) {
        state.logger->log(PacketDirection::INCOMING, buffer, received);
    }
    
    // Parsear respuesta
    std::stringstream ss(buffer);
    std::string line;
    
    while (std::getline(ss, line)) {
        if (line == "GRID_UPDATE") continue;
        if (line == "END") break;
        
        int row, col, cell_state;
        // Intentar parsear con 3 valores (fila, columna, estado)
        if (sscanf(line.c_str(), "%d %d %d", &row, &col, &cell_state) == 3) {
            if (row >= 0 && row < GRID_ROWS && col >= 0 && col < GRID_COLS) {
                state.grid[row][col] = (cell_state != 0);
            }
        } 
        // Fallback: parsear con 2 valores (toggle)
        else if (sscanf(line.c_str(), "%d %d", &row, &col) == 2) {
            if (row >= 0 && row < GRID_ROWS && col >= 0 && col < GRID_COLS) {
                state.grid[row][col] = !state.grid[row][col];
            }
        }
    }
}

/**
 * Cuenta las celdas vivas
 */
int count_alive_cells(const NcursesClientState& state) {
    int count = 0;
    for (int i = 0; i < GRID_ROWS; i++) {
        for (int j = 0; j < GRID_COLS; j++) {
            if (state.grid[i][j]) count++;
        }
    }
    return count;
}

/**
 * Renderiza la grilla en pantalla
 */
void render_grid(const NcursesClientState& state) {
    int screen_height, screen_width;
    getmaxyx(stdscr, screen_height, screen_width);
    
    // Área de visualización (dejando espacio para status)
    int grid_display_height = screen_height - 4;
    int grid_display_width = screen_width - 2;
    
    // Limpiar pantalla
    clear();
    
    // Dibujar borde
    attron(COLOR_PAIR(COLOR_GRID));
    box(stdscr, 0, 0);
    attroff(COLOR_PAIR(COLOR_GRID));
    
    // Dibujar grilla
    for (int screen_y = 0; screen_y < grid_display_height && screen_y + state.view_row < GRID_ROWS; screen_y++) {
        for (int screen_x = 0; screen_x < grid_display_width && screen_x + state.view_col < GRID_COLS; screen_x++) {
            int grid_row = screen_y + state.view_row;
            int grid_col = screen_x + state.view_col;
            
            // Verificar si es la posición del cursor
            bool is_cursor = (grid_row == state.cursor_row && grid_col == state.cursor_col);
            
            if (is_cursor) {
                attron(COLOR_PAIR(COLOR_CURSOR) | A_BOLD);
                mvaddch(screen_y + 1, screen_x + 1, CURSOR_CHAR);
                attroff(COLOR_PAIR(COLOR_CURSOR) | A_BOLD);
            } else if (state.grid[grid_row][grid_col]) {
                attron(COLOR_PAIR(COLOR_ALIVE) | A_BOLD);
                mvaddch(screen_y + 1, screen_x + 1, CELL_ALIVE);
                attroff(COLOR_PAIR(COLOR_ALIVE) | A_BOLD);
            } else {
                attron(COLOR_PAIR(COLOR_DEAD));
                mvaddch(screen_y + 1, screen_x + 1, CELL_DEAD);
                attroff(COLOR_PAIR(COLOR_DEAD));
            }
        }
    }
    
    // Barra de estado
    int status_y = screen_height - 3;
    
    attron(COLOR_PAIR(COLOR_STATUS));
    mvhline(status_y, 0, ACS_HLINE, screen_width);
    
    // Estado de conexión
    if (state.connected) {
        mvprintw(status_y + 1, 1, "[CONECTADO] ");
    } else {
        attroff(COLOR_PAIR(COLOR_STATUS));
        attron(COLOR_PAIR(COLOR_ERROR));
        mvprintw(status_y + 1, 1, "[DESCONECTADO] ");
        attroff(COLOR_PAIR(COLOR_ERROR));
        attron(COLOR_PAIR(COLOR_STATUS));
    }
    
    // Información
    mvprintw(status_y + 1, 15, "Patrón: %s | Cursor: (%d,%d) | Vista: (%d,%d) | Celdas: %d",
             PATTERNS[state.current_pattern_idx],
             state.cursor_row, state.cursor_col,
             state.view_row, state.view_col,
             count_alive_cells(state));
    
    // Estadísticas de red
    mvprintw(status_y + 2, 1, "TX: %d | RX: %d | %s",
             state.packets_sent, state.packets_received,
             state.paused ? "[PAUSADO]" : "");
    
    attroff(COLOR_PAIR(COLOR_STATUS));
    
    // Ayuda
    attron(COLOR_PAIR(COLOR_INFO));
    mvprintw(0, 2, " Conway's Game of Life - ncurses viewer ");
    attroff(COLOR_PAIR(COLOR_INFO));
    
    refresh();
}

/**
 * Procesa la entrada del usuario
 */
void process_input(NcursesClientState& state) {
    int ch = getch();
    if (ch == ERR) return;
    
    int screen_height, screen_width;
    getmaxyx(stdscr, screen_height, screen_width);
    int grid_display_height = screen_height - 4;
    int grid_display_width = screen_width - 2;
    
    switch (ch) {
        // Movimiento del cursor
        case KEY_UP:
            if (state.cursor_row > 0) state.cursor_row--;
            break;
        case KEY_DOWN:
            if (state.cursor_row < GRID_ROWS - 1) state.cursor_row++;
            break;
        case KEY_LEFT:
            if (state.cursor_col > 0) state.cursor_col--;
            break;
        case KEY_RIGHT:
            if (state.cursor_col < GRID_COLS - 1) state.cursor_col++;
            break;
            
        // Movimiento del viewport (WASD)
        case 'w':
        case 'W':
            if (state.view_row > 0) state.view_row -= 5;
            if (state.view_row < 0) state.view_row = 0;
            break;
        case 's':
        case 'S':
            if (state.view_row < GRID_ROWS - grid_display_height) state.view_row += 5;
            break;
        case 'a':
        case 'A':
            if (state.view_col > 0) state.view_col -= 5;
            if (state.view_col < 0) state.view_col = 0;
            break;
        case 'd':
        case 'D':
            if (state.view_col < GRID_COLS - grid_display_width) state.view_col += 5;
            break;
            
        // Colocar patrón
        case ' ':
            send_pattern(state, PATTERNS[state.current_pattern_idx], 
                        state.cursor_row, state.cursor_col);
            break;
            
        // Selección de patrón (1-9)
        case '1': case '2': case '3': case '4': case '5':
        case '6': case '7': case '8': case '9':
            {
                int idx = ch - '1';
                if (idx < NUM_PATTERNS) {
                    state.current_pattern_idx = idx;
                }
            }
            break;
            
        // Pausar
        case 'p':
        case 'P':
            state.paused = !state.paused;
            break;
            
        // Reconectar
        case 'r':
        case 'R':
            disconnect_from_server(state);
            connect_to_server(state);
            break;
            
        // Limpiar grilla local
        case 'c':
        case 'C':
            memset(state.grid, 0, sizeof(state.grid));
            break;
            
        // Salir
        case 'q':
        case 'Q':
        case 27:  // ESC
            state.running = false;
            break;
    }
    
    // Mantener cursor dentro del viewport visible
    if (state.cursor_row < state.view_row) {
        state.view_row = state.cursor_row;
    }
    if (state.cursor_row >= state.view_row + grid_display_height) {
        state.view_row = state.cursor_row - grid_display_height + 1;
    }
    if (state.cursor_col < state.view_col) {
        state.view_col = state.cursor_col;
    }
    if (state.cursor_col >= state.view_col + grid_display_width) {
        state.view_col = state.cursor_col - grid_display_width + 1;
    }
}

/**
 * Muestra pantalla de ayuda
 */
void show_help() {
    std::cout << "\nConway's Game of Life - Cliente ncurses\n";
    std::cout << "========================================\n\n";
    std::cout << "Uso: ncurses_viewer [opciones]\n\n";
    std::cout << "Opciones:\n";
    std::cout << "  -h, --help           Muestra esta ayuda\n";
    std::cout << "  -s, --server IP      IP del servidor (default: 127.0.0.1)\n";
    std::cout << "  -p, --port PORT      Puerto del servidor (default: 6969)\n";
    std::cout << "  -l, --log FILE       Archivo de log (default: ncurses_client.log)\n";
    std::cout << "  -v, --verbose        Mostrar logs en consola\n";
    std::cout << "\nControles:\n";
    std::cout << "  Flechas       Mover cursor\n";
    std::cout << "  W/A/S/D       Mover viewport\n";
    std::cout << "  Espacio       Colocar patrón\n";
    std::cout << "  1-9           Seleccionar patrón\n";
    std::cout << "  P             Pausar/reanudar\n";
    std::cout << "  R             Reconectar\n";
    std::cout << "  C             Limpiar grilla\n";
    std::cout << "  Q/ESC         Salir\n\n";
}

/**
 * Función principal
 */
int main(int argc, char* argv[]) {
    NcursesClientState state;
    std::string log_file = "ncurses_client.log";
    bool verbose = false;
    
    // Parsear argumentos
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            show_help();
            return 0;
        }
        else if ((arg == "-s" || arg == "--server") && i + 1 < argc) {
            state.server_ip = argv[++i];
        }
        else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            state.server_port = std::stoi(argv[++i]);
        }
        else if ((arg == "-l" || arg == "--log") && i + 1 < argc) {
            log_file = argv[++i];
        }
        else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        }
    }
    
    // Inicializar logger
    PacketLogger logger(log_file, EndpointType::CLIENT, verbose);
    state.logger = &logger;
    
    // Inicializar ncurses
    init_ncurses();
    
    // Conectar al servidor
    if (!connect_to_server(state)) {
        cleanup_ncurses();
        std::cerr << "Error: No se pudo conectar a " 
                  << state.server_ip << ":" << state.server_port << std::endl;
        std::cerr << "Asegúrate de que el servidor esté corriendo.\n";
        return 1;
    }
    
    // Loop principal
    while (state.running) {
        process_input(state);
        
        if (!state.paused) {
            receive_updates(state);
        }
        
        render_grid(state);
        
        // Pequeña pausa para no consumir 100% CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    // Limpieza
    disconnect_from_server(state);
    cleanup_ncurses();
    
    std::cout << "Sesión terminada. Logs guardados en: " << log_file << std::endl;
    
    return 0;
}
