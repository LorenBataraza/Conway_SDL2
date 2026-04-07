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
    
    // Formato: ADD_PATTERN <pattern> <row> <col> [player_id]
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "ADD_PATTERN %s %d %d %d", 
             app_state->current_pattern.c_str(), 
             grid_row, grid_col,
             static_cast<int>(app_state->player_id));
    
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
    
    while (std::getline(ss, line)) {
        if (line == "GRID_UPDATE") continue;
        if (line == "END") break;
        if (line.empty()) continue;
        
        int row, col, state;
        
        // Formato nuevo: <row> <col> <state>
        // state puede ser 0 (muerta) o 1-N (ID del jugador)
        if (sscanf(line.c_str(), "%d %d %d", &row, &col, &state) == 3) {
            if (row >= 0 && row < app_state->rows && 
                col >= 0 && col < app_state->cols) {
                app_state->grid[row][col] = static_cast<CellValue>(state);
            }
        }
        // Formato legacy: <row> <col> (toggle)
        else if (sscanf(line.c_str(), "%d %d", &row, &col) == 2) {
            if (row >= 0 && row < app_state->rows && 
                col >= 0 && col < app_state->cols) {
                // Toggle con player_id local
                app_state->grid[row][col] = 
                    (app_state->grid[row][col] == CELL_DEAD) 
                    ? app_state->player_id : CELL_DEAD;
            }
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
