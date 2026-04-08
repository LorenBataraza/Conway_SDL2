#pragma once

#include <iostream>
#include <sstream>
#include <cstring>
#include <cerrno>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "appState.h"
#include "grid.h"

/**
 * Inicializa la conexión con el servidor
 */
void init_connection(AppState* app_state) {
    // Crear socket
    app_state->client_socket = socket(AF_INET, SOCK_STREAM, 0);
    
    if (app_state->client_socket < 0) {
        perror("[CLIENT] socket() failed");
        if (app_state->packet_logger) {
            app_state->packet_logger->log_error("socket() failed", errno);
        }
        return;
    }

    // Configurar dirección del servidor
    memset(&app_state->server_addrs, 0, sizeof(app_state->server_addrs));
    app_state->server_addrs.sin_family = AF_INET;
    app_state->server_addrs.sin_port = htons(app_state->server_port);
    
    if (inet_pton(AF_INET, app_state->server_ip.c_str(), 
                  &app_state->server_addrs.sin_addr) <= 0) {
        perror("[CLIENT] inet_pton() failed");
        close(app_state->client_socket);
        app_state->client_socket = -1;
        return;
    }
    
    // Conectar
    if (connect(app_state->client_socket, 
                (const struct sockaddr*)&app_state->server_addrs,
                sizeof(app_state->server_addrs)) < 0) {
        perror("[CLIENT] connect() failed");
        if (app_state->packet_logger) {
            app_state->packet_logger->log_error(
                "connect() failed to " + app_state->server_ip + ":" + 
                std::to_string(app_state->server_port), errno);
        }
        close(app_state->client_socket);
        app_state->client_socket = -1;
        return;
    }
    
    // Configurar socket como no bloqueante
    fcntl(app_state->client_socket, F_SETFL, O_NONBLOCK);
    
    // Limpiar grilla y activar multiplayer
    clear_grid(app_state->grid, app_state->rows, app_state->cols);
    app_state->multiplayer = true;
    
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
    if (app_state->client_socket >= 0) {
        close(app_state->client_socket);
        app_state->client_socket = -1;
    }
    
    app_state->multiplayer = false;
    
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
    if (!app_state->multiplayer || app_state->client_socket < 0) return;
    
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
        if (errno == EPIPE || errno == ECONNRESET) {
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
    if (!app_state->multiplayer || app_state->client_socket < 0) return;
    
    ssize_t sent = send(app_state->client_socket, cmd.c_str(), cmd.size(), MSG_NOSIGNAL);
    
    if (sent < 0) {
        if (errno == EPIPE || errno == ECONNRESET) {
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

/**
 * Recibe actualizaciones del servidor y las aplica a la grilla
 */
void receive_update(AppState* app_state) {
    if (!app_state->multiplayer || app_state->client_socket < 0) return;
    
    char buffer[8192];
    ssize_t received = recv(app_state->client_socket, buffer, sizeof(buffer) - 1, 0);
    
    if (received < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cerr << "[CLIENT] recv() error: " << strerror(errno) << std::endl;
            disconnect(app_state);
        }
        return;
    }
    
    if (received == 0) {
        std::cerr << "[CLIENT] Servidor cerró conexión" << std::endl;
        disconnect(app_state);
        return;
    }
    
    buffer[received] = '\0';
    
    app_state->net_stats.packets_received++;
    app_state->net_stats.bytes_received += received;
    
    if (app_state->packet_logger) {
        app_state->packet_logger->log(PacketDirection::INCOMING, buffer, received);
    }
    
    // Parsear respuesta
    std::stringstream ss(buffer);
    std::string line;
    
    enum class ParseMode { NONE, GRID_UPDATE, CONFIG_UPDATE, PLAYER_STATE, SCORES };
    ParseMode mode = ParseMode::NONE;
    
    while (std::getline(ss, line)) {
        // Detectar inicio de bloque
        if (line == "GRID_UPDATE") {
            mode = ParseMode::GRID_UPDATE;
            continue;
        }
        if (line == "CONFIG_UPDATE") {
            mode = ParseMode::CONFIG_UPDATE;
            continue;
        }
        if (line == "PLAYER_STATE") {
            mode = ParseMode::PLAYER_STATE;
            continue;
        }
        if (line == "SCORES") {
            mode = ParseMode::SCORES;
            // Resetear estado de jugadores activos
            for (int i = 1; i <= AppState::MAX_PLAYERS; i++) {
                app_state->player_scores[i].active = false;
            }
            continue;
        }
        if (line == "END") {
            mode = ParseMode::NONE;
            continue;
        }
        if (line.empty()) continue;
        
        // Mensajes especiales
        if (line.find("ERROR NOT_ENOUGH_POINTS") != std::string::npos) {
            std::cout << "[CLIENT] No hay suficientes puntos de consumo" << std::endl;
            continue;
        }
        
        if (line.find("ERROR OUTSIDE_ZONE") != std::string::npos) {
            std::cout << "[CLIENT] No puedes colocar fuera de tu zona" << std::endl;
            continue;
        }
        
        int winner_id;
        if (sscanf(line.c_str(), "WINNER %d", &winner_id) == 1) {
            std::cout << "[CLIENT] ¡Jugador " << winner_id << " ganó!" << std::endl;
            continue;
        }
        
        // Parsear según el modo actual
        switch (mode) {
            case ParseMode::GRID_UPDATE: {
                int row, col, state;
                if (sscanf(line.c_str(), "%d %d %d", &row, &col, &state) == 3) {
                    if (row >= 0 && row < app_state->rows && 
                        col >= 0 && col < app_state->cols) {
                        app_state->grid[row][col] = static_cast<CellValue>(state);
                    }
                }
                break;
            }
            case ParseMode::CONFIG_UPDATE:
                parse_config_line(app_state, line);
                break;
            case ParseMode::PLAYER_STATE:
                parse_player_state(app_state, line);
                break;
            case ParseMode::SCORES:
                parse_scores_line(app_state, line);
                break;
            default:
                break;
        }
    }
}

/**
 * Verifica si la conexión sigue activa
 */
bool is_connected(AppState* app_state) {
    if (!app_state->multiplayer || app_state->client_socket < 0) {
        return false;
    }
    
    // Verificar con un send vacío
    char test = 0;
    ssize_t result = send(app_state->client_socket, &test, 0, MSG_NOSIGNAL);
    
    if (result < 0 && (errno == EPIPE || errno == ECONNRESET)) {
        disconnect(app_state);
        return false;
    }
    
    return true;
}
