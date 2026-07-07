#pragma once

#include <iostream>
#include <sstream>
#include <cstring>

#include "net_compat.h"   // sockets portables (POSIX/Winsock)

#include "appState.h"
#include "grid.h"

/**
 * Inicializa la conexión con el servidor
 */
void init_connection(AppState* app_state) {
    // Crear socket
    app_state->client_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (app_state->client_socket == NET_INVALID_SOCKET_VALUE) {
        net_perror("[CLIENT] socket() failed");
        if (app_state->packet_logger) {
            app_state->packet_logger->log_error("socket() failed", 0);
        }
        return;
    }

    // Configurar dirección del servidor
    memset(&app_state->server_addrs, 0, sizeof(app_state->server_addrs));
    app_state->server_addrs.sin_family = AF_INET;
    app_state->server_addrs.sin_port = htons(app_state->server_port);
    
    if (inet_pton(AF_INET, app_state->server_ip.c_str(), 
                  &app_state->server_addrs.sin_addr) <= 0) {
        net_perror("[CLIENT] inet_pton() failed");
        net_close(app_state->client_socket);
        app_state->client_socket = NET_INVALID_SOCKET_VALUE;
        return;
    }
    
    // Conectar
    if (connect(app_state->client_socket, 
                (const struct sockaddr*)&app_state->server_addrs,
                sizeof(app_state->server_addrs)) < 0) {
        net_perror("[CLIENT] connect() failed");
        if (app_state->packet_logger) {
            app_state->packet_logger->log_error(
                "connect() failed to " + app_state->server_ip + ":" +
                std::to_string(app_state->server_port), 0);
        }
        net_close(app_state->client_socket);
        app_state->client_socket = NET_INVALID_SOCKET_VALUE;
        return;
    }

    // Configurar socket como no bloqueante
    net_set_nonblocking(app_state->client_socket);
    
    // Limpiar grilla y activar multiplayer
    app_state->automaton.clear();
    app_state->multiplayer = true;

    // Reiniciar el estado de parseo de red (arranque limpio).
    app_state->net_rx_buffer.clear();
    app_state->net_parse_mode = 0;   // PM_NONE
    app_state->awaiting_resync = false;
    
    if (app_state->packet_logger) {
        app_state->packet_logger->log_event("CONNECTED", 
            "Conectado a " + app_state->server_ip + ":" + 
            std::to_string(app_state->server_port));
    }
    
    std::cout << "[CLIENT] Conectado a " << app_state->server_ip 
              << ":" << app_state->server_port << std::endl;
}

/**
 * Desconecta del servidor
 */
void disconnect(AppState* app_state) {
    if (app_state->client_socket != NET_INVALID_SOCKET_VALUE) {
        net_close(app_state->client_socket);
        app_state->client_socket = NET_INVALID_SOCKET_VALUE;
    }

    app_state->multiplayer = false;
    app_state->net_rx_buffer.clear();
    app_state->net_parse_mode = 0;   // PM_NONE
    app_state->awaiting_resync = false;
    
    if (app_state->packet_logger) {
        app_state->packet_logger->log_event("DISCONNECTED", "Desconectado del servidor");
    }
    
    std::cout << "[CLIENT] Desconectado" << std::endl;
}

/**
 * Convierte coordenadas de pantalla a coordenadas de grilla
 */
void screen_to_grid(AppState* app_state, SDL_Window* window, viewpoint* vp,
                    int screen_x, int screen_y, int& grid_row, int& grid_col) {
    int window_width, window_height;
    SDL_GetWindowSize(window, &window_width, &window_height);
    
    VisibleRange cols_range = get_visible_columns(vp, app_state->rows, app_state->cols);
    VisibleRange rows_range = get_visible_rows(vp, app_state->rows, app_state->cols);
    
    grid_col = cols_range.start + 
               static_cast<int>(screen_x * (cols_range.end - cols_range.start) / 
                               static_cast<float>(window_width));
    grid_row = rows_range.start + 
               static_cast<int>(screen_y * (rows_range.end - rows_range.start) / 
                               static_cast<float>(window_height));
}

/**
 * Envía un patrón al servidor
 */
void send_pattern(AppState* app_state, SDL_Window* window, viewpoint* vp, 
                  int screen_x, int screen_y) {
    if (!app_state->multiplayer || app_state->client_socket == NET_INVALID_SOCKET_VALUE) return;
    
    int grid_row, grid_col;
    screen_to_grid(app_state, window, vp, screen_x, screen_y, grid_row, grid_col);
    
    // Formato: ADD_PATTERN <pattern> <row> <col> <player_id> <mirror_h> <mirror_v>
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "ADD_PATTERN %s %d %d %d %d %d", 
             app_state->current_pattern.c_str(), 
             grid_row, grid_col,
             static_cast<int>(app_state->player_id),
             app_state->mirror_horizontal ? 1 : 0,
             app_state->mirror_vertical ? 1 : 0);
    
    ssize_t sent = send(app_state->client_socket, buffer, strlen(buffer), MSG_NOSIGNAL);
    
    if (sent < 0) {
        if (!net_would_block()) {
            std::cerr << "[CLIENT] Conexión perdida" << std::endl;
            disconnect(app_state);
        }
        return;
    }
    
    app_state->net_stats.packets_sent++;
    app_state->net_stats.bytes_sent += sent;
    
    if (app_state->packet_logger) {
        app_state->packet_logger->log(PacketDirection::OUTGOING, buffer, sent);
    }
}

/**
 * Envía un comando al servidor
 */
void send_command(AppState* app_state, const std::string& cmd) {
    if (!app_state->multiplayer || app_state->client_socket == NET_INVALID_SOCKET_VALUE) return;
    
    ssize_t sent = send(app_state->client_socket, cmd.c_str(), cmd.size(), MSG_NOSIGNAL);
    
    if (sent < 0) {
        if (!net_would_block()) {
            disconnect(app_state);
        }
        return;
    }
    
    app_state->net_stats.packets_sent++;
    app_state->net_stats.bytes_sent += sent;
}

/**
 * Solicita configuración al servidor
 */
void request_config(AppState* app_state) {
    send_command(app_state, "GET_CONFIG");
}

/**
 * Parsea una línea de configuración
 */
void parse_config_line(AppState* app_state, const std::string& line) {
    int value;
    char str_val[32];
    
    if (sscanf(line.c_str(), "FREQ %d", &value) == 1) {
        app_state->frecuencia = value;
    }
    else if (sscanf(line.c_str(), "RUN %d", &value) == 1) {
        app_state->run_sim = (value != 0);
    }
    else if (sscanf(line.c_str(), "PLAYER_ID %d", &value) == 1) {
        app_state->player_id = static_cast<CellValue>(value);
    }
    else if (sscanf(line.c_str(), "CLIENTS %d", &value) == 1) {
        app_state->num_clients = value;
    }
    else if (sscanf(line.c_str(), "VICTORY_GOAL %d", &value) == 1) {
        app_state->victory_goal = value;
    }
    else if (sscanf(line.c_str(), "MODE %31s", str_val) == 1) {
        if (strcmp(str_val, "COMPETITION") == 0) {
            app_state->game_mode = AppState::GameMode::COMPETITION;
        } else {
            app_state->game_mode = AppState::GameMode::NORMAL;
        }
    }
}

/**
 * Parsea estado de un jugador
 */
void parse_player_state(AppState* app_state, const std::string& line) {
    int pid, value;
    
    if (sscanf(line.c_str(), "PLAYER_ID %d", &pid) == 1) {
        // Solo marcamos el jugador como activo
        if (pid >= 1 && pid <= AppState::MAX_PLAYERS) {
            app_state->player_scores[pid].active = true;
        }
    }
    else if (sscanf(line.c_str(), "VICTORY %d", &value) == 1) {
        int pid = app_state->player_id;
        app_state->player_scores[pid].victory_points = value;
    }
    else if (sscanf(line.c_str(), "CONSUMPTION %d", &value) == 1) {
        int pid = app_state->player_id;
        app_state->player_scores[pid].consumption_points = value;
    }
    else if (sscanf(line.c_str(), "CELLS %d", &value) == 1) {
        int pid = app_state->player_id;
        app_state->player_scores[pid].cells_alive = value;
    }
}

/**
 * Parsea línea de SCORES (broadcast de puntos de todos)
 */
void parse_scores_line(AppState* app_state, const std::string& line) {
    int pid, victory, consumption, cells;
    if (sscanf(line.c_str(), "%d %d %d %d", &pid, &victory, &consumption, &cells) == 4) {
        if (pid >= 1 && pid <= AppState::MAX_PLAYERS) {
            app_state->player_scores[pid].victory_points = victory;
            app_state->player_scores[pid].consumption_points = consumption;
            app_state->player_scores[pid].cells_alive = cells;
            app_state->player_scores[pid].active = true;
        }
    }
}

// Modos de parseo persistentes entre recv() (un FULL_GRID puede superar el
// buffer y llegar fragmentado en varias lecturas).
enum {
    PM_NONE = 0,
    PM_CONFIG,
    PM_PLAYER,
    PM_SCORES,
    PM_FULL_GRID
};

/**
 * Pide al servidor un snapshot completo de la grilla (sync inicial / resync).
 */
void request_full_grid(AppState* app_state) {
    app_state->awaiting_resync = true;
    send_command(app_state, "GET_FULL_GRID");
}

/**
 * Dispara una resincronización tras detectar una divergencia de hash.
 */
static void request_resync(AppState* app_state) {
    if (app_state->awaiting_resync) return;
    std::cout << "[CLIENT] Desincronización detectada, resincronizando..." << std::endl;
    request_full_grid(app_state);
}

/**
 * TICK: avanza la grilla local una generación y verifica el hash del servidor.
 */
static void handle_tick(AppState* app_state, const std::string& line) {
    unsigned long long gen = 0, server_hash = 0;
    if (sscanf(line.c_str(), "TICK %llu %llu", &gen, &server_hash) != 2) return;
    if (app_state->awaiting_resync) return;  // esperando FULL_GRID

    const unsigned long long local_gen = app_state->automaton.generation();
    if (gen == local_gen + 1) {
        app_state->automaton.step();
        if (app_state->automaton.hash() != server_hash) {
            request_resync(app_state);
        }
    } else if (gen != local_gen) {
        request_resync(app_state);  // salto de generación inesperado
    }
}

/**
 * PATTERN: coloca el patrón en la MISMA generación que el servidor (reproduce
 * la colocación determinista con load_pattern_into_grid vía add_pattern).
 */
static void handle_pattern(AppState* app_state, const std::string& line) {
    unsigned long long apply_gen = 0;
    char name[64];
    int row, col, player, mh, mv;
    if (sscanf(line.c_str(), "PATTERN %llu %63s %d %d %d %d %d",
               &apply_gen, name, &row, &col, &player, &mh, &mv) != 7) {
        return;
    }
    if (app_state->awaiting_resync) return;  // vendrá incluido en el FULL_GRID

    if (apply_gen == app_state->automaton.generation()) {
        app_state->automaton.add_pattern(name, row, col,
                                         static_cast<CellValue>(player),
                                         mh != 0, mv != 0);
    } else {
        request_resync(app_state);
    }
}

/**
 * Procesa una línea completa del protocolo.
 */
static void process_line(AppState* app_state, const std::string& line) {
    // Cabeceras de bloque
    if (line == "CONFIG_UPDATE") { app_state->net_parse_mode = PM_CONFIG; return; }
    if (line == "PLAYER_STATE")  { app_state->net_parse_mode = PM_PLAYER; return; }
    if (line == "SCORES") {
        app_state->net_parse_mode = PM_SCORES;
        for (int i = 1; i <= AppState::MAX_PLAYERS; i++) {
            app_state->player_scores[i].active = false;
        }
        return;
    }
    if (line == "END") {
        if (app_state->net_parse_mode == PM_FULL_GRID) {
            app_state->automaton.set_generation(app_state->net_full_gen);
            app_state->automaton.mark_all_dirty();
            app_state->awaiting_resync = false;
        }
        app_state->net_parse_mode = PM_NONE;
        return;
    }
    if (line.empty()) return;

    // Mensajes de una línea / cabecera con datos
    if (line.rfind("TICK ", 0) == 0)    { handle_tick(app_state, line); return; }
    if (line.rfind("PATTERN ", 0) == 0) { handle_pattern(app_state, line); return; }
    if (line.rfind("FULL_GRID ", 0) == 0) {
        unsigned long long gen = 0;
        sscanf(line.c_str(), "FULL_GRID %llu", &gen);
        app_state->net_full_gen = gen;
        app_state->automaton.clear();
        app_state->net_parse_mode = PM_FULL_GRID;
        return;
    }

    // Mensajes especiales
    if (line.find("ERROR NOT_ENOUGH_POINTS") != std::string::npos) {
        std::cout << "[CLIENT] No hay suficientes puntos de consumo" << std::endl;
        return;
    }
    if (line.find("ERROR OUTSIDE_ZONE") != std::string::npos) {
        std::cout << "[CLIENT] No puedes colocar fuera de tu zona" << std::endl;
        return;
    }
    int winner_id;
    if (sscanf(line.c_str(), "WINNER %d", &winner_id) == 1) {
        std::cout << "[CLIENT] ¡Jugador " << winner_id << " ganó!" << std::endl;
        return;
    }

    // Líneas de datos según el modo actual
    switch (app_state->net_parse_mode) {
        case PM_CONFIG: parse_config_line(app_state, line); break;
        case PM_PLAYER: parse_player_state(app_state, line); break;
        case PM_SCORES: parse_scores_line(app_state, line); break;
        case PM_FULL_GRID: {
            int row, col, val;
            if (sscanf(line.c_str(), "%d %d %d", &row, &col, &val) == 3) {
                if (row >= 0 && row < app_state->rows &&
                    col >= 0 && col < app_state->cols) {
                    app_state->automaton.board().set(row, col, static_cast<CellValue>(val));
                }
            }
            break;
        }
        default: break;
    }
}

/**
 * Recibe datos del servidor (protocolo lockstep + hash), avanza la grilla
 * localmente y verifica su integridad.
 */
void receive_update(AppState* app_state) {
    if (!app_state->multiplayer || app_state->client_socket == NET_INVALID_SOCKET_VALUE) return;

    char buffer[8192];
    bool got_data = false;

    // Drenar todo lo disponible (FULL_GRID puede ser grande y fragmentarse).
    while (true) {
        ssize_t received = recv(app_state->client_socket, buffer, sizeof(buffer) - 1, 0);
        if (received > 0) {
            got_data = true;
            app_state->net_stats.packets_received++;
            app_state->net_stats.bytes_received += received;
            if (app_state->packet_logger) {
                app_state->packet_logger->log(PacketDirection::INCOMING, buffer, received);
            }
            app_state->net_rx_buffer.append(buffer, received);
            continue;
        }
        if (received == 0) {
            std::cerr << "[CLIENT] Servidor cerró conexión" << std::endl;
            disconnect(app_state);
            return;
        }
        // received < 0
        if (net_would_block()) break;  // nada más por ahora
        net_perror("[CLIENT] recv() error");
        disconnect(app_state);
        return;
    }

    if (!got_data) return;

    // Procesar líneas completas; conservar el fragmento parcial restante.
    size_t pos;
    while ((pos = app_state->net_rx_buffer.find('\n')) != std::string::npos) {
        std::string line = app_state->net_rx_buffer.substr(0, pos);
        app_state->net_rx_buffer.erase(0, pos + 1);
        process_line(app_state, line);
    }
}

/**
 * Verifica si la conexión sigue activa
 */
bool is_connected(AppState* app_state) {
    if (!app_state->multiplayer || app_state->client_socket == NET_INVALID_SOCKET_VALUE) {
        return false;
    }
    
    // Verificar con un send vacío
    char test = 0;
    ssize_t result = send(app_state->client_socket, &test, 0, MSG_NOSIGNAL);
    
    if (result < 0 && !net_would_block()) {
        disconnect(app_state);
        return false;
    }
    
    return true;
}
