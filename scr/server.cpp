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

#include "net_compat.h"   // sockets portables (POSIX/Winsock) — antes que grid.h/SDL

#include "grid.h"
#include "automaton.h"
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
    socket_t socket_fd = NET_INVALID_SOCKET_VALUE;
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
    // GRID / AUTÓMATA
    // El autómata posee la grilla y el ruleset. `current_grid` es un alias
    // ESTABLE a la grilla actual del autómata (CellValue**) para el código
    // existente (render, patrones, snapshot).
    Automaton automaton;
    CellValue** current_grid;
    int rows, cols;

    // Simulation Parameters
    bool run_sim = true;
    int frecuencia = 10;
    GameMode game_mode = GameMode::NORMAL;
    
    // Communication Parameters
    socket_t tcp_socket = NET_INVALID_SOCKET_VALUE;
    struct sockaddr_in bind_addr;
    int server_port = 6969;

    // Múltiples clientes
    std::vector<socket_t> client_sockets;
    std::vector<pollfd_t> poll_fds;
    
    // Estado de jugadores
    std::array<PlayerState, MAX_PLAYERS + 1> players;  // índice 0 no usado
    
    // Logging
    PacketLogger* logger = nullptr;
    bool logging_enabled = false;

    // Error handling
    int error = 0;
    int enabled = 1;

    ServerState(int initial_rows, int initial_cols)
    : automaton{initial_rows, initial_cols},
      current_grid{automaton.grid()},
      rows{initial_rows},
      cols{initial_cols}
    {
        // Inicializar jugadores
        for (int i = 1; i <= MAX_PLAYERS; i++) {
            players[i].id = i;
        }
        
        memset(&bind_addr, 0, sizeof(bind_addr));
        tcp_socket = socket(AF_INET, SOCK_STREAM, 0);

        if (tcp_socket == NET_INVALID_SOCKET_VALUE) {
            net_perror("socket() failed");
            error = 1;
            return;
        }
        if(DEBUG_INIT) printf("[INIT] Socket creation succeeded\n");

        if (setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&enabled, sizeof(enabled)) == -1) {
            net_perror("setsockopt() failed");
            error = 1;
            net_close(tcp_socket);
            return;
        }

        bind_addr.sin_port = htons(server_port);
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(tcp_socket, (const struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
            net_perror("bind() failed");
            error = 1;
            net_close(tcp_socket);
            return;
        }
        if(DEBUG_INIT) printf("[INIT] Bind succeeded on port %d\n", server_port);

        if (listen(tcp_socket, SOMAXCONN) < 0) {
            net_perror("listen() failed");
            error = 1;
            net_close(tcp_socket);
            return;
        }

        net_set_nonblocking(tcp_socket);

        pollfd_t pfd;
        pfd.fd = tcp_socket;
        pfd.events = POLLIN;
        pfd.revents = 0;
        poll_fds.push_back(pfd);
        
        if(DEBUG_INIT) printf("[INIT] Server ready, listening on port %d\n", server_port);
    }

    ~ServerState() {
        for (socket_t fd : client_sockets) {
            net_close(fd);
        }
        if (tcp_socket != NET_INVALID_SOCKET_VALUE) net_close(tcp_socket);
        // current_grid es propiedad del autómata (RAII); no se libera aquí.
        if (logger) delete logger;
    }

    // Sincroniza el ruleset del autómata con el modo de juego actual.
    void sync_ruleset() {
        automaton.set_ruleset(game_mode == GameMode::COMPETITION
                                  ? &RULE_COMPETITION : &RULE_NORMAL);
    }

    int client_count() const { return static_cast<int>(client_sockets.size()); }
    
    void add_client(socket_t fd) {
        if (client_count() >= MAX_CLIENTS) {
            net_close(fd);
            return;
        }

        net_set_nonblocking(fd);
        client_sockets.push_back(fd);

        pollfd_t pfd;
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

    void remove_client(socket_t fd) {
        // Liberar player slot
        for (int i = 1; i <= MAX_PLAYERS; i++) {
            if (players[i].socket_fd == fd) {
                players[i].connected = false;
                players[i].socket_fd = NET_INVALID_SOCKET_VALUE;
                break;
            }
        }

        client_sockets.erase(
            std::remove(client_sockets.begin(), client_sockets.end(), fd),
            client_sockets.end()
        );

        poll_fds.erase(
            std::remove_if(poll_fds.begin(), poll_fds.end(),
                [fd](const pollfd_t& pfd) { return pfd.fd == fd; }),
            poll_fds.end()
        );

        net_close(fd);
        std::cout << "[SERVER] Cliente desconectado, total: " << client_count() << "\n";
    }
    
    int find_free_player_slot() {
        for (int i = 1; i <= MAX_PLAYERS; i++) {
            if (!players[i].connected) return i;
        }
        return 1;  // Fallback al primero
    }
    
    int get_player_id(socket_t socket_fd) {
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
        // Limpiar grilla (autómata)
        automaton.clear();
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

void send_to_client(socket_t client_fd, const std::string& msg) {
    send(client_fd, msg.c_str(), msg.size(), MSG_NOSIGNAL);
}

void broadcast_to_all(ServerState& state, const std::string& msg) {
    for (socket_t fd : state.client_sockets) {
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

// Forward declarations (definidas en la sección BROADCAST GRILLA)
void broadcast_tick(ServerState& state);
void send_full_grid(ServerState& state, socket_t client_fd);
void broadcast_full_grid(ServerState& state);

// ==================== LÓGICA DE JUEGO ====================

// Las reglas (NORMAL / COMPETITION) viven ahora en automaton.cpp como
// instancias de Ruleset. El servidor avanza la simulación con
// `state.automaton.step()`, compartiendo el MISMO código determinista que el
// cliente (requisito para el lockstep + hash).

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

bool receive_command(socket_t client_socket, ServerState& state) {
    char buffer[1024];
    
    int received = recv(client_socket, buffer, sizeof(buffer)-1, 0);
    
    if (received < 0) {
        if (net_would_block()) return true;
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
            
            // Colocar patrón transformado (y marcar su región como sucia para
            // que el autómata la reevalúe en el próximo paso).
            int min_y = state.rows, min_x = state.cols, max_y = -1, max_x = -1;
            for (const auto& [dx, dy] : transformed_cells) {
                int x = col + dx;
                int y = row + dy;
                if (x >= 0 && x < state.cols && y >= 0 && y < state.rows) {
                    state.current_grid[y][x] = static_cast<CellValue>(player_id);
                    min_y = std::min(min_y, y); max_y = std::max(max_y, y);
                    min_x = std::min(min_x, x); max_x = std::max(max_x, x);
                }
            }
            if (max_y >= 0) {
                state.automaton.mark_region_dirty(min_y, min_x, max_y, max_x);
            }

            // Difundir el evento de patrón a TODOS los clientes para que lo
            // apliquen en la MISMA generación (lockstep determinista). Todos
            // reproducen la misma colocación vía load_pattern_into_grid.
            {
                std::stringstream ps;
                ps << "PATTERN " << state.automaton.generation() << " "
                   << pattern << " " << row << " " << col << " "
                   << player_id << " " << mirror_h << " " << mirror_v << "\n";
                std::string pmsg = ps.str();
                if (state.logging_enabled && state.logger) {
                    state.logger->log(PacketDirection::OUTGOING, pmsg.c_str(), pmsg.size());
                }
                broadcast_to_all(state, pmsg);
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
        state.sync_ruleset();
        std::cout << "[CONFIG] Modo: " << game_mode_to_string(state.game_mode) << "\n";
        state.reset_game();
        broadcast_config(state);
        broadcast_full_grid(state);
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
        state.automaton.step();
        update_player_scores(state);
        broadcast_tick(state);  // avanzar a los clientes en lockstep
        std::cout << "[CONFIG] Step manual\n";
        return true;
    }

    // ========== GET_FULL_GRID (sync / resync) ==========
    if (strcmp(cmd, "GET_FULL_GRID") == 0) {
        send_full_grid(state, client_socket);
        return true;
    }

    // ========== CLEAR ==========
    if (strcmp(cmd, "CLEAR") == 0) {
        state.automaton.clear();
        broadcast_full_grid(state);  // resync inmediato
        std::cout << "[CONFIG] Grilla limpiada\n";
        return true;
    }
    
    // ========== RESET ==========
    if (strcmp(cmd, "RESET") == 0) {
        state.reset_game();
        broadcast_config(state);
        broadcast_full_grid(state);
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

// ==================== BROADCAST GRILLA (lockstep + hash) ====================

// Paquete ligero por tick: sólo la generación y el hash de la grilla del
// servidor. El cliente avanza su propia grilla (mismo Automaton determinista) y
// compara su hash; si difiere, pide una resincronización (GET_FULL_GRID).
void broadcast_tick(ServerState& state) {
    if (state.client_sockets.empty()) return;

    std::stringstream ss;
    ss << "TICK " << state.automaton.generation()
       << " " << state.automaton.hash() << "\n";
    std::string msg = ss.str();

    if (DEBUG_SEND) {
        std::cout << "[SEND] TICK gen=" << state.automaton.generation()
                  << " -> " << state.client_count() << " clients\n";
    }
    if (state.logging_enabled && state.logger) {
        state.logger->log(PacketDirection::OUTGOING, msg.c_str(), msg.size());
    }

    broadcast_to_all(state, msg);
}

// Snapshot completo de la grilla (sólo celdas vivas) + generación. Se envía al
// unirse un cliente, al cambiar de modo/reset, y ante una desincronización.
void send_full_grid(ServerState& state, socket_t client_fd) {
    std::stringstream ss;
    ss << "FULL_GRID " << state.automaton.generation() << "\n";
    for (int i = 0; i < state.rows; ++i) {
        for (int j = 0; j < state.cols; ++j) {
            const CellValue v = state.current_grid[i][j];
            if (v != CELL_DEAD) {
                ss << i << " " << j << " " << static_cast<int>(v) << "\n";
            }
        }
    }
    ss << "END\n";
    std::string msg = ss.str();

    if (state.logging_enabled && state.logger) {
        state.logger->log(PacketDirection::OUTGOING, msg.c_str(), msg.size());
    }
    send_to_client(client_fd, msg);
}

// Envía el snapshot completo a TODOS los clientes (tras CLEAR / RESET / cambio
// de modo, donde la grilla cambia fuera del paso normal de simulación).
void broadcast_full_grid(ServerState& state) {
    for (socket_t fd : state.client_sockets) {
        send_full_grid(state, fd);
    }
}

void accept_new_client(ServerState& state) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    socket_t client_fd = accept(state.tcp_socket, (struct sockaddr*)&client_addr, &client_len);

    if (client_fd == NET_INVALID_SOCKET_VALUE) {
        if (!net_would_block()) {
            net_perror("accept() failed");
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
    // Inicializar la pila de red (Winsock en Windows) antes de crear sockets.
    if (net_init() != 0) {
        std::cerr << "Error inicializando la red (Winsock)\n";
        return 1;
    }

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

    // Alinear el ruleset del autómata con el modo elegido por CLI.
    state.sync_ruleset();
    
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
        int ready = net_poll(state.poll_fds.data(), state.poll_fds.size(), 10);

        if (ready < 0) {
            if (net_interrupted()) continue;
            net_perror("net_poll() failed");
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
                state.automaton.step();
                update_player_scores(state);
                broadcast_tick(state);  // paquete ligero: gen + hash
            }
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
                        broadcast_full_grid(state);
                        break;
                    }
                }
            }
        }
    }

    net_cleanup();
    return 0;
}
