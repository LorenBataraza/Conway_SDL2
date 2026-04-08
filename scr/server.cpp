/**
 * server.cpp - Servidor multi-cliente para Conway's Game of Life
 * 
 * Características:
 * - Multi-cliente con poll() (hasta 10 clientes)
 * - Modos de juego: NORMAL y COMPETITION
 * - Sistema de puntos (victoria y consumo)
 * - Control de logging desde clientes
 * - Celdas enemigas se destruyen mutuamente (modo competición)
 */

#include <stdio.h>
#include <iostream>
#include <stdbool.h>
#include <cerrno>
#include <cstring>
#include <vector>
#include <algorithm>
#include <array>
#include <chrono>

#include <sstream> 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include "grid.h"
#include "patterns.h"
#include "packet_logger.h"

#define DEBUG_INIT 1
#define DEBUG_SEND 0
#define DEBUG_RECV 1

constexpr int MAX_CLIENTS = 10;
constexpr int MAX_PLAYERS = 8;
constexpr int INITIAL_CONSUMPTION = 200;  // Puntos iniciales de consumo
constexpr int CONSUMPTION_REGEN = 2;       // Regeneración por tick
constexpr int VICTORY_GOAL = 60000;        // Puntos para ganar (60x más)

// ==================== MODOS DE JUEGO ====================

enum class GameMode {
    NORMAL = 0,      // Conway clásico
    COMPETITION = 1  // Celdas enemigas se destruyen
};

std::string game_mode_to_string(GameMode mode) {
    switch (mode) {
        case GameMode::NORMAL: return "NORMAL";
        case GameMode::COMPETITION: return "COMPETITION";
        default: return "UNKNOWN";
    }
}

GameMode string_to_game_mode(const std::string& str) {
    if (str == "COMPETITION" || str == "1") return GameMode::COMPETITION;
    return GameMode::NORMAL;
}

// ==================== ESTADO DEL JUGADOR ====================

struct PlayerState {
    int id = 0;
    int socket_fd = -1;
    int victory_points = 0;
    int consumption_points = INITIAL_CONSUMPTION;
    int cells_alive = 0;
    bool connected = false;
    
    void reset() {
        victory_points = 0;
        consumption_points = INITIAL_CONSUMPTION;
        cells_alive = 0;
    }
};

// ==================== ESTADO DEL SERVIDOR ====================

struct ServerState {
    // GRID
    CellValue** current_grid;
    CellValue** previous_grid;
    int rows, cols;
    
    // Simulation Parameters 
    bool run_sim = true;
    int frecuencia = 10;
    GameMode game_mode = GameMode::NORMAL;
    
    // Communication Parameters
    int tcp_socket = 0;
    struct sockaddr_in bind_addr;
    int server_port = 6969;

    // Múltiples clientes
    std::vector<int> client_sockets;
    std::vector<struct pollfd> poll_fds;
    
    // Estado de jugadores
    std::array<PlayerState, MAX_PLAYERS + 1> players;  // índice 0 no usado
    
    // Logging
    PacketLogger* logger = nullptr;
    bool logging_enabled = false;

    // Error handling
    int error = 0;
    int enabled = 1;

    ServerState(int initial_rows, int initial_cols) 
    : rows{initial_rows},
      cols{initial_cols},
      current_grid{construct_grid(initial_rows, initial_cols)},
      previous_grid{construct_grid(initial_rows, initial_cols)}
    {
        // Inicializar jugadores
        for (int i = 1; i <= MAX_PLAYERS; i++) {
            players[i].id = i;
        }
        
        memset(&bind_addr, 0, sizeof(bind_addr));
        tcp_socket = socket(AF_INET, SOCK_STREAM, 0);

        if (tcp_socket < 0) {
            perror("socket() failed");
            error = 1;
            return;
        }
        if(DEBUG_INIT) printf("[INIT] Socket creation succeeded\n");

        if (setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) == -1) {
            perror("setsockopt() failed");
            error = 1;
            close(tcp_socket);
            return;
        }
        
        bind_addr.sin_port = htons(server_port);
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(tcp_socket, (const struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
            perror("bind() failed");
            error = 1;
            close(tcp_socket);
            return;
        }
        if(DEBUG_INIT) printf("[INIT] Bind succeeded on port %d\n", server_port);

        if (listen(tcp_socket, SOMAXCONN) < 0) {
            perror("listen() failed");
            error = 1;
            close(tcp_socket);
            return;
        }
        
        fcntl(tcp_socket, F_SETFL, O_NONBLOCK);
        
        struct pollfd pfd;
        pfd.fd = tcp_socket;
        pfd.events = POLLIN;
        pfd.revents = 0;
        poll_fds.push_back(pfd);
        
        if(DEBUG_INIT) printf("[INIT] Server ready, listening on port %d\n", server_port);
    }

    ~ServerState() {
        for (int fd : client_sockets) {
            close(fd);
        }
        if (tcp_socket > 0) close(tcp_socket);
        destructor_grid(current_grid, rows, cols);
        destructor_grid(previous_grid, rows, cols);
        if (logger) delete logger;
    }

    int client_count() const { return static_cast<int>(client_sockets.size()); }
    
    void add_client(int fd) {
        if (client_count() >= MAX_CLIENTS) {
            close(fd);
            return;
        }
        
        fcntl(fd, F_SETFL, O_NONBLOCK);
        client_sockets.push_back(fd);
        
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        poll_fds.push_back(pfd);
        
        // Asignar player_id
        int player_id = find_free_player_slot();
        if (player_id > 0) {
            players[player_id].socket_fd = fd;
            players[player_id].connected = true;
            players[player_id].reset();
        }
        
        std::cout << "[SERVER] Cliente conectado (player " << player_id 
                  << "), total: " << client_count() << "\n";
    }

    void remove_client(int fd) {
        // Liberar player slot
        for (int i = 1; i <= MAX_PLAYERS; i++) {
            if (players[i].socket_fd == fd) {
                players[i].connected = false;
                players[i].socket_fd = -1;
                break;
            }
        }
        
        client_sockets.erase(
            std::remove(client_sockets.begin(), client_sockets.end(), fd),
            client_sockets.end()
        );
        
        poll_fds.erase(
            std::remove_if(poll_fds.begin(), poll_fds.end(),
                [fd](const struct pollfd& pfd) { return pfd.fd == fd; }),
            poll_fds.end()
        );
        
        close(fd);
        std::cout << "[SERVER] Cliente desconectado, total: " << client_count() << "\n";
    }
    
    int find_free_player_slot() {
        for (int i = 1; i <= MAX_PLAYERS; i++) {
            if (!players[i].connected) return i;
        }
        return 1;  // Fallback al primero
    }
    
    int get_player_id(int socket_fd) {
        for (int i = 1; i <= MAX_PLAYERS; i++) {
            if (players[i].socket_fd == socket_fd) return i;
        }
        return 1;
    }
    
    void enable_logging(const std::string& filename = "server.log") {
        if (!logger) {
            logger = new PacketLogger(filename, EndpointType::SERVER, true);
        }
        logging_enabled = true;
        std::cout << "[LOG] Logging habilitado: " << filename << "\n";
    }
    
    void disable_logging() {
        logging_enabled = false;
        std::cout << "[LOG] Logging deshabilitado\n";
    }
    
    void reset_game() {
        // Limpiar grilla
        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < cols; j++) {
                current_grid[i][j] = CELL_DEAD;
                previous_grid[i][j] = CELL_DEAD;
            }
        }
        // Reset jugadores
        for (int i = 1; i <= MAX_PLAYERS; i++) {
            if (players[i].connected) {
                players[i].reset();
            }
        }
        std::cout << "[GAME] Partida reiniciada\n";
    }
};

// ==================== COMUNICACIÓN ====================

void send_to_client(int client_fd, const std::string& msg) {
    send(client_fd, msg.c_str(), msg.size(), MSG_NOSIGNAL);
}

void broadcast_to_all(ServerState& state, const std::string& msg) {
    for (int fd : state.client_sockets) {
        send_to_client(fd, msg);
    }
}

void broadcast_config(ServerState& state) {
    std::stringstream ss;
    ss << "CONFIG_UPDATE\n";
    ss << "FREQ " << state.frecuencia << "\n";
    ss << "RUN " << (state.run_sim ? 1 : 0) << "\n";
    ss << "MODE " << game_mode_to_string(state.game_mode) << "\n";
    ss << "CLIENTS " << state.client_count() << "\n";
    ss << "END\n";
    broadcast_to_all(state, ss.str());
}

void send_player_state(ServerState& state, int player_id) {
    if (player_id < 1 || player_id > MAX_PLAYERS) return;
    if (!state.players[player_id].connected) return;
    
    PlayerState& p = state.players[player_id];
    
    std::stringstream ss;
    ss << "PLAYER_STATE\n";
    ss << "PLAYER_ID " << player_id << "\n";
    ss << "VICTORY " << p.victory_points << "\n";
    ss << "CONSUMPTION " << p.consumption_points << "\n";
    ss << "CELLS " << p.cells_alive << "\n";
    ss << "END\n";
    
    send_to_client(p.socket_fd, ss.str());
}

void broadcast_all_player_states(ServerState& state) {
    std::stringstream ss;
    ss << "SCORES\n";
    
    for (int i = 1; i <= MAX_PLAYERS; i++) {
        if (state.players[i].connected) {
            ss << i << " " << state.players[i].victory_points 
               << " " << state.players[i].consumption_points
               << " " << state.players[i].cells_alive << "\n";
        }
    }
    ss << "END\n";
    
    broadcast_to_all(state, ss.str());
}

// ==================== LÓGICA DE JUEGO ====================

// Contar vecinos considerando modo de juego
int count_neighbors_normal(CellValue** grid, int rows, int cols, int row, int col) {
    int count = 0;
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            if (i == 0 && j == 0) continue;
            int ni = row + i;
            int nj = col + j;
            if (ni >= 0 && ni < rows && nj >= 0 && nj < cols) {
                if (grid[ni][nj] != CELL_DEAD) count++;
            }
        }
    }
    return count;
}

// En modo competición: vecinos del mismo jugador cuentan +1, enemigos -1
int count_neighbors_competition(CellValue** grid, int rows, int cols, 
                                 int row, int col, CellValue my_player) {
    int count = 0;
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            if (i == 0 && j == 0) continue;
            int ni = row + i;
            int nj = col + j;
            if (ni >= 0 && ni < rows && nj >= 0 && nj < cols) {
                CellValue neighbor = grid[ni][nj];
                if (neighbor != CELL_DEAD) {
                    if (neighbor == my_player) {
                        count++;  // Aliado: suma
                    } else {
                        count--;  // Enemigo: resta
                    }
                }
            }
        }
    }
    return count;
}

// Encontrar el jugador dominante entre los vecinos (para nacimiento)
CellValue get_dominant_player(CellValue** grid, int rows, int cols, int row, int col) {
    std::array<int, MAX_PLAYERS + 1> player_count = {0};
    
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            if (i == 0 && j == 0) continue;
            int ni = row + i;
            int nj = col + j;
            if (ni >= 0 && ni < rows && nj >= 0 && nj < cols) {
                CellValue cell = grid[ni][nj];
                if (cell > 0 && cell <= MAX_PLAYERS) {
                    player_count[cell]++;
                }
            }
        }
    }
    
    int max_count = 0;
    CellValue dominant = 1;
    for (int i = 1; i <= MAX_PLAYERS; i++) {
        if (player_count[i] > max_count) {
            max_count = player_count[i];
            dominant = static_cast<CellValue>(i);
        }
    }
    return dominant;
}

void update_grid_with_mode(ServerState& state) {
    static CellValue** temp_grid = nullptr;
    if (!temp_grid) {
        temp_grid = construct_grid(state.rows, state.cols);
    }
    
    // Copiar grilla actual a temporal
    for (int i = 0; i < state.rows; i++) {
        for (int j = 0; j < state.cols; j++) {
            temp_grid[i][j] = state.current_grid[i][j];
        }
    }
    
    // Aplicar reglas según modo
    for (int i = 0; i < state.rows; i++) {
        for (int j = 0; j < state.cols; j++) {
            CellValue current = temp_grid[i][j];
            
            if (state.game_mode == GameMode::NORMAL) {
                // Conway clásico
                int neighbors = count_neighbors_normal(temp_grid, state.rows, state.cols, i, j);
                
                if (current != CELL_DEAD) {
                    // Celda viva
                    if (neighbors < 2 || neighbors > 3) {
                        state.current_grid[i][j] = CELL_DEAD;
                    }
                } else {
                    // Celda muerta
                    if (neighbors == 3) {
                        state.current_grid[i][j] = get_dominant_player(temp_grid, state.rows, state.cols, i, j);
                    }
                }
            } else {
                // Modo COMPETITION
                if (current != CELL_DEAD) {
                    // Celda viva: vecinos aliados suman, enemigos restan
                    int effective_neighbors = count_neighbors_competition(
                        temp_grid, state.rows, state.cols, i, j, current);
                    
                    // Necesita al menos 2 vecinos "efectivos" para sobrevivir
                    // y no más de 3
                    if (effective_neighbors < 2 || effective_neighbors > 3) {
                        state.current_grid[i][j] = CELL_DEAD;
                    }
                } else {
                    // Celda muerta: nace si tiene exactamente 3 vecinos
                    int total_neighbors = count_neighbors_normal(temp_grid, state.rows, state.cols, i, j);
                    if (total_neighbors == 3) {
                        state.current_grid[i][j] = get_dominant_player(temp_grid, state.rows, state.cols, i, j);
                    }
                }
            }
        }
    }
}

void update_player_scores(ServerState& state) {
    // Resetear conteo de celdas
    for (int i = 1; i <= MAX_PLAYERS; i++) {
        state.players[i].cells_alive = 0;
    }
    
    // Contar celdas vivas por jugador
    for (int i = 0; i < state.rows; i++) {
        for (int j = 0; j < state.cols; j++) {
            CellValue cell = state.current_grid[i][j];
            if (cell > 0 && cell <= MAX_PLAYERS) {
                state.players[cell].cells_alive++;
            }
        }
    }
    
    // Actualizar puntos de victoria (1 punto por cada 10 celdas vivas)
    for (int i = 1; i <= MAX_PLAYERS; i++) {
        if (state.players[i].connected) {
            state.players[i].victory_points += state.players[i].cells_alive / 10;
            
            // Regenerar consumo (más lento)
            if (state.players[i].consumption_points < INITIAL_CONSUMPTION) {
                state.players[i].consumption_points += CONSUMPTION_REGEN;
                if (state.players[i].consumption_points > INITIAL_CONSUMPTION) {
                    state.players[i].consumption_points = INITIAL_CONSUMPTION;
                }
            }
        }
    }
}

// ==================== COMANDOS ====================

bool receive_command(int client_socket, ServerState& state) {
    char buffer[1024];
    
    int received = recv(client_socket, buffer, sizeof(buffer)-1, 0);
    
    if (received < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) return true;
        return false;
    }
    
    if (received == 0) return false;
    
    buffer[received] = '\0';
    
    if (DEBUG_RECV) std::cout << "[RECV] " << buffer << "\n";
    
    if (state.logging_enabled && state.logger) {
        state.logger->log(PacketDirection::INCOMING, buffer, received);
    }
    
    int player_id = state.get_player_id(client_socket);
    
    // Parsear comando
    char cmd[50];
    if (sscanf(buffer, "%49s", cmd) != 1) return true;
    
    // ========== ADD_PATTERN ==========
    char pattern[50];
    int row, col, req_player_id, mirror_h = 0, mirror_v = 0;
    int parsed = sscanf(buffer, "ADD_PATTERN %49s %d %d %d %d %d", 
                        pattern, &row, &col, &req_player_id, &mirror_h, &mirror_v);
    if (parsed >= 3) {
        if (pattern_exists(pattern)) {
            const auto& pattern_data = PatternRegistry::instance().get(pattern);
            int cost = pattern_data.cost;
            
            // Calcular bounding box del patrón para espejado
            int min_dx = 0, max_dx = 0, min_dy = 0, max_dy = 0;
            for (const auto& [dx, dy] : pattern_data.cells) {
                min_dx = std::min(min_dx, dx);
                max_dx = std::max(max_dx, dx);
                min_dy = std::min(min_dy, dy);
                max_dy = std::max(max_dy, dy);
            }
            int width = max_dx - min_dx;
            int height = max_dy - min_dy;
            
            // Generar celdas con espejado aplicado
            std::vector<std::pair<int, int>> transformed_cells;
            for (const auto& [dx, dy] : pattern_data.cells) {
                int tx = dx;
                int ty = dy;
                
                // Espejado horizontal (invertir X)
                if (mirror_h) {
                    tx = max_dx - (dx - min_dx);
                }
                // Espejado vertical (invertir Y)
                if (mirror_v) {
                    ty = max_dy - (dy - min_dy);
                }
                
                transformed_cells.push_back({tx, ty});
            }
            
            // Verificar modo competición
            if (state.game_mode == GameMode::COMPETITION) {
                // Verificar si tiene suficientes puntos de consumo
                if (state.players[player_id].consumption_points < cost) {
                    send_to_client(client_socket, "ERROR NOT_ENOUGH_POINTS\n");
                    return true;
                }
                
                // Verificar número válido de jugadores para zonas
                int active_players = 0;
                for (int i = 1; i <= MAX_PLAYERS; i++) {
                    if (state.players[i].connected) active_players++;
                }
                
                // Si hay 2,4,6,8 jugadores, verificar zona de spawn
                if (is_valid_player_count(active_players)) {
                    SpawnZone zone = get_spawn_zone(player_id, active_players, state.rows, state.cols);
                    
                    // Verificar que TODAS las celdas transformadas estén en la zona
                    bool all_in_zone = true;
                    for (const auto& [dx, dy] : transformed_cells) {
                        int x = col + dx;
                        int y = row + dy;
                        if (!zone.contains(y, x)) {
                            all_in_zone = false;
                            break;
                        }
                    }
                    
                    if (!all_in_zone) {
                        send_to_client(client_socket, "ERROR OUTSIDE_ZONE\n");
                        return true;
                    }
                }
                
                state.players[player_id].consumption_points -= cost;
            }
            
            // Colocar patrón transformado
            for (const auto& [dx, dy] : transformed_cells) {
                int x = col + dx;
                int y = row + dy;
                if (x >= 0 && x < state.cols && y >= 0 && y < state.rows) {
                    state.current_grid[y][x] = static_cast<CellValue>(player_id);
                }
            }
            
            if (DEBUG_RECV) {
                std::cout << "[PATTERN] " << pattern << " at (" << row << "," << col 
                          << ") by player " << player_id << " cost=" << cost 
                          << " mirror_h=" << mirror_h << " mirror_v=" << mirror_v << "\n";
            }
        }
        return true;
    }
    
    // ========== SET_FREQ ==========
    int new_freq;
    if (sscanf(buffer, "SET_FREQ %d", &new_freq) == 1) {
        if (new_freq >= 1 && new_freq <= 60) {
            state.frecuencia = new_freq;
            std::cout << "[CONFIG] Frecuencia: " << new_freq << " Hz\n";
            broadcast_config(state);
        }
        return true;
    }
    
    // ========== SET_RUN ==========
    int run_val;
    if (sscanf(buffer, "SET_RUN %d", &run_val) == 1) {
        state.run_sim = (run_val != 0);
        std::cout << "[CONFIG] Simulación: " << (state.run_sim ? "ON" : "OFF") << "\n";
        broadcast_config(state);
        return true;
    }
    
    // ========== SET_MODE ==========
    char mode_str[20];
    if (sscanf(buffer, "SET_MODE %19s", mode_str) == 1) {
        state.game_mode = string_to_game_mode(mode_str);
        std::cout << "[CONFIG] Modo: " << game_mode_to_string(state.game_mode) << "\n";
        state.reset_game();
        broadcast_config(state);
        return true;
    }
    
    // ========== SET_LOG ==========
    int log_val;
    if (sscanf(buffer, "SET_LOG %d", &log_val) == 1) {
        if (log_val) {
            state.enable_logging();
        } else {
            state.disable_logging();
        }
        return true;
    }
    
    // ========== STEP ==========
    if (strcmp(cmd, "STEP") == 0 && !state.run_sim) {
        update_grid_with_mode(state);
        update_player_scores(state);
        std::cout << "[CONFIG] Step manual\n";
        return true;
    }
    
    // ========== CLEAR ==========
    if (strcmp(cmd, "CLEAR") == 0) {
        for (int i = 0; i < state.rows; i++) {
            for (int j = 0; j < state.cols; j++) {
                state.current_grid[i][j] = CELL_DEAD;
            }
        }
        std::cout << "[CONFIG] Grilla limpiada\n";
        return true;
    }
    
    // ========== RESET ==========
    if (strcmp(cmd, "RESET") == 0) {
        state.reset_game();
        broadcast_config(state);
        return true;
    }
    
    // ========== GET_CONFIG ==========
    if (strcmp(cmd, "GET_CONFIG") == 0) {
        std::stringstream ss;
        ss << "CONFIG_UPDATE\n";
        ss << "FREQ " << state.frecuencia << "\n";
        ss << "RUN " << (state.run_sim ? 1 : 0) << "\n";
        ss << "MODE " << game_mode_to_string(state.game_mode) << "\n";
        ss << "PLAYER_ID " << player_id << "\n";
        ss << "CLIENTS " << state.client_count() << "\n";
        ss << "VICTORY_GOAL " << VICTORY_GOAL << "\n";
        ss << "END\n";
        send_to_client(client_socket, ss.str());
        send_player_state(state, player_id);
        return true;
    }
    
    // ========== GET_SCORES ==========
    if (strcmp(cmd, "GET_SCORES") == 0) {
        broadcast_all_player_states(state);
        return true;
    }
    
    return true;
}

// ==================== BROADCAST GRILLA ====================

void broadcast_update(ServerState& state) {
    if (state.client_sockets.empty()) return;
    
    std::stringstream ss;
    bool modified = false;
    int changes_count = 0;
    
    for (int i = 0; i < state.rows; ++i) {
        for (int j = 0; j < state.cols; ++j) {
            if (state.current_grid[i][j] != state.previous_grid[i][j]) {
                if (!modified) {
                    ss << "GRID_UPDATE\n";
                    modified = true;
                }
                ss << i << " " << j << " " << static_cast<int>(state.current_grid[i][j]) << "\n";
                changes_count++;
                state.previous_grid[i][j] = state.current_grid[i][j];
            }
        }
    }
    
    if (modified) {
        ss << "END\n";
        std::string update = ss.str();
        
        if (DEBUG_SEND) {
            std::cout << "[SEND] " << changes_count << " changes -> " 
                      << state.client_count() << " clients\n";
        }
        
        if (state.logging_enabled && state.logger) {
            state.logger->log(PacketDirection::OUTGOING, update.c_str(), update.size());
        }
        
        broadcast_to_all(state, update);
    }
}

void accept_new_client(ServerState& state) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int client_fd = accept(state.tcp_socket, (struct sockaddr*)&client_addr, &client_len);
    
    if (client_fd < 0) {
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            perror("accept() failed");
        }
        return;
    }
    
    state.add_client(client_fd);
    
    if (state.logging_enabled && state.logger) {
        char addr_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, addr_str, INET_ADDRSTRLEN);
        state.logger->log_event("CLIENT_CONNECTED", 
            std::string(addr_str) + ":" + std::to_string(ntohs(client_addr.sin_port)));
    }
}

// ==================== MAIN ====================

void show_usage(const char* program) {
    std::cout << "\nServidor Conway's Game of Life - Multiplayer\n";
    std::cout << "=============================================\n\n";
    std::cout << "Uso: " << program << " [opciones]\n\n";
    std::cout << "Opciones:\n";
    std::cout << "  -p, --port PORT    Puerto (default: 6969)\n";
    std::cout << "  -f, --freq HZ      Frecuencia (default: 10)\n";
    std::cout << "  -m, --mode MODE    Modo: NORMAL, COMPETITION (default: NORMAL)\n";
    std::cout << "  -l, --log FILE     Habilitar logging\n";
    std::cout << "  -v, --verbose      Modo verbose\n";
    std::cout << "  -h, --help         Mostrar ayuda\n\n";
    std::cout << "Comandos del cliente:\n";
    std::cout << "  ADD_PATTERN <name> <row> <col>  Colocar patrón\n";
    std::cout << "  SET_FREQ <hz>                   Cambiar frecuencia\n";
    std::cout << "  SET_RUN <0|1>                   Pausar/reanudar\n";
    std::cout << "  SET_MODE <NORMAL|COMPETITION>  Cambiar modo\n";
    std::cout << "  SET_LOG <0|1>                   Activar/desactivar log\n";
    std::cout << "  STEP                            Avanzar un paso\n";
    std::cout << "  CLEAR                           Limpiar grilla\n";
    std::cout << "  RESET                           Reiniciar partida\n";
    std::cout << "  GET_CONFIG                      Obtener configuración\n";
    std::cout << "  GET_SCORES                      Obtener puntuaciones\n\n";
}

int main(int argc, char* argv[]) {
    ServerState state(GRID_ROWS, GRID_COLS);
    bool verbose = false;
    
    // Parsear argumentos
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            show_usage(argv[0]);
            return 0;
        }
        else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            state.server_port = std::stoi(argv[++i]);
        }
        else if ((arg == "-f" || arg == "--freq") && i + 1 < argc) {
            state.frecuencia = std::stoi(argv[++i]);
        }
        else if ((arg == "-m" || arg == "--mode") && i + 1 < argc) {
            state.game_mode = string_to_game_mode(argv[++i]);
        }
        else if ((arg == "-l" || arg == "--log") && i + 1 < argc) {
            state.enable_logging(argv[++i]);
        }
        else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        }
    }
    
    if (state.error) {
        std::cerr << "Error inicializando servidor\n";
        return 1;
    }
    
    std::cout << "\n=== Conway's Game of Life Server ===\n";
    std::cout << "Puerto: " << state.server_port << "\n";
    std::cout << "Frecuencia: " << state.frecuencia << " Hz\n";
    std::cout << "Modo: " << game_mode_to_string(state.game_mode) << "\n";
    std::cout << "Esperando conexiones...\n\n";
    
    // Variables para timing
    auto last_update = std::chrono::steady_clock::now();
    auto last_score_broadcast = std::chrono::steady_clock::now();
    
    while (true) {
        // Poll con timeout corto
        int ready = poll(state.poll_fds.data(), state.poll_fds.size(), 10);
        
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("poll() failed");
            break;
        }
        
        // Procesar eventos
        for (size_t i = 0; i < state.poll_fds.size(); i++) {
            if (!(state.poll_fds[i].revents & POLLIN)) continue;
            
            if (state.poll_fds[i].fd == state.tcp_socket) {
                accept_new_client(state);
            } else {
                if (!receive_command(state.poll_fds[i].fd, state)) {
                    state.remove_client(state.poll_fds[i].fd);
                    break;
                }
            }
        }
        
        // Timing para simulación
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update).count();
        
        if (elapsed >= (1000 / state.frecuencia)) {
            last_update = now;
            
            if (state.run_sim) {
                update_grid_with_mode(state);
                update_player_scores(state);
            }
            
            broadcast_update(state);
        }
        
        // Broadcast de scores cada segundo (modo competición)
        if (state.game_mode == GameMode::COMPETITION) {
            auto score_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_score_broadcast).count();
            
            if (score_elapsed >= 1000) {
                last_score_broadcast = now;
                broadcast_all_player_states(state);
                
                // Verificar victoria
                for (int i = 1; i <= MAX_PLAYERS; i++) {
                    if (state.players[i].connected && 
                        state.players[i].victory_points >= VICTORY_GOAL) {
                        std::stringstream ss;
                        ss << "WINNER " << i << "\n";
                        broadcast_to_all(state, ss.str());
                        std::cout << "[GAME] ¡Jugador " << i << " gana!\n";
                        state.reset_game();
                        break;
                    }
                }
            }
        }
    }
    
    return 0;
}
