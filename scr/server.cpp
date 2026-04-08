#include <stdio.h>
#include <iostream>
#include <stdbool.h>
#include <cerrno>
#include <cstring>
#include <vector>
#include <algorithm>

// Communication
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

struct ServerState{
    // GRID
    CellValue** current_grid;
    CellValue** previous_grid;
    
    int rows, cols;
    
    // Simulation Parameters 
    bool run_sim = true;
    int frecuencia = 1;

    // Communication Parameters
    int tcp_socket = 0;
    struct sockaddr_in bind_addr;
    int server_port = 6969;

    // Múltiples clientes
    std::vector<int> client_sockets;
    std::vector<struct pollfd> poll_fds;

    // Error handling
    int error = 0;
    int enabled = 1;
    
    // Logger
    PacketLogger* logger = nullptr;

    ServerState(int initial_rows, int initial_cols) 
    : rows{initial_rows},
      cols{initial_cols},
      current_grid{construct_grid(initial_rows, initial_cols)},
      previous_grid{construct_grid(initial_rows, initial_cols)}
    {
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
        
        // Socket de escucha NO bloqueante
        fcntl(tcp_socket, F_SETFL, O_NONBLOCK);
        
        // Agregar socket de escucha al poll
        struct pollfd pfd;
        pfd.fd = tcp_socket;
        pfd.events = POLLIN;
        pfd.revents = 0;
        poll_fds.push_back(pfd);
        
        if(DEBUG_INIT) puts("[INIT] Listen() succeeded");
        if(DEBUG_INIT) printf("[INIT] Waiting for connections (max %d clients)...\n", MAX_CLIENTS);
    }
    
    ~ServerState() {
        if (current_grid) destructor_grid(current_grid, rows, cols);
        if (previous_grid) destructor_grid(previous_grid, rows, cols);
        if (tcp_socket > 0) close(tcp_socket);
        for (int sock : client_sockets) {
            if (sock > 0) close(sock);
        }
    }
    
    void add_client(int client_fd) {
        fcntl(client_fd, F_SETFL, O_NONBLOCK);
        client_sockets.push_back(client_fd);
        
        struct pollfd pfd;
        pfd.fd = client_fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        poll_fds.push_back(pfd);
    }
    
    void remove_client(int client_fd) {
        client_sockets.erase(
            std::remove(client_sockets.begin(), client_sockets.end(), client_fd),
            client_sockets.end()
        );
        
        poll_fds.erase(
            std::remove_if(poll_fds.begin(), poll_fds.end(), 
                [client_fd](const pollfd& pfd) { return pfd.fd == client_fd; }),
            poll_fds.end()
        );
        
        close(client_fd);
    }
    
    int client_count() const {
        return static_cast<int>(client_sockets.size());
    }
};

void accept_new_client(ServerState& state) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int client_fd = accept(state.tcp_socket, (struct sockaddr*)&client_addr, &client_len);
    
    if (client_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("accept() failed");
        }
        return;
    }
    
    if (state.client_count() >= MAX_CLIENTS) {
        std::cerr << "[SERVER] Max clients reached, rejecting connection\n";
        close(client_fd);
        return;
    }
    
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(client_addr.sin_port);
    
    state.add_client(client_fd);
    
    std::cout << "[SERVER] Cliente #" << state.client_count() 
              << " conectado: " << client_ip << ":" << client_port << "\n";
    
    if (state.logger) {
        state.logger->log_event("CLIENT_CONNECTED", 
            std::string(client_ip) + ":" + std::to_string(client_port) + 
            " (total: " + std::to_string(state.client_count()) + ")");
    }
}

// Encontrar índice del cliente en el vector
int find_client_index(ServerState& state, int client_fd) {
    for (size_t i = 0; i < state.client_sockets.size(); i++) {
        if (state.client_sockets[i] == client_fd) {
            return static_cast<int>(i) + 1;  // IDs empiezan en 1
        }
    }
    return 1;
}

// Enviar respuesta a un cliente específico
void send_to_client(int client_fd, const std::string& msg) {
    send(client_fd, msg.c_str(), msg.size(), MSG_NOSIGNAL);
}

// Broadcast de configuración a todos los clientes
void broadcast_config(ServerState& state) {
    std::stringstream ss;
    ss << "CONFIG_UPDATE\n";
    ss << "FREQ " << state.frecuencia << "\n";
    ss << "RUN " << (state.run_sim ? 1 : 0) << "\n";
    ss << "END\n";
    
    std::string msg = ss.str();
    for (int fd : state.client_sockets) {
        send_to_client(fd, msg);
    }
}

bool receive_command(int client_socket, ServerState& state) {
    char buffer[1024];
    
    int received = recv(client_socket, buffer, sizeof(buffer)-1, 0);
    
    if (received < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return true;
        }
        return false;
    }
    
    if (received == 0) {
        return false;
    }
    
    buffer[received] = '\0';
    
    if(DEBUG_RECV) std::cout << "[RECV] " << buffer << "\n";
    
    if (state.logger) {
        state.logger->log(PacketDirection::INCOMING, buffer, received);
    }
    
    // Obtener player_id basado en el socket
    int player_id = find_client_index(state, client_socket);
    
    // Parsear comandos
    char cmd[50];
    if (sscanf(buffer, "%49s", cmd) != 1) return true;
    
    // ========== COMANDO: ADD_PATTERN ==========
    char pattern[50];
    int row, col, req_player_id;
    if (sscanf(buffer, "ADD_PATTERN %49s %d %d %d", pattern, &row, &col, &req_player_id) == 4) {
        // Cliente especificó su player_id
        player_id = req_player_id;
    } else if (sscanf(buffer, "ADD_PATTERN %49s %d %d", pattern, &row, &col) == 3) {
        // Usar player_id del socket
    } else {
        pattern[0] = '\0';  // No es ADD_PATTERN
    }
    
    if (pattern[0] != '\0' && pattern_exists(pattern)) {
        if(DEBUG_RECV) std::cout << "[RECV] Pattern: " << pattern << " at (" << row << "," << col << ") player=" << player_id << "\n";
        
        const auto& cells = get_pattern_cells(pattern);
        for (const auto& [dx, dy] : cells) {
            int x = col + dx;
            int y = row + dy;
            if (x >= 0 && x < state.cols && y >= 0 && y < state.rows) {
                state.current_grid[y][x] = static_cast<CellValue>(player_id);
            }
        }
        return true;
    }
    
    // ========== COMANDO: SET_FREQ <hz> ==========
    int new_freq;
    if (sscanf(buffer, "SET_FREQ %d", &new_freq) == 1) {
        if (new_freq >= 1 && new_freq <= 60) {
            state.frecuencia = new_freq;
            std::cout << "[CONFIG] Frecuencia cambiada a " << new_freq << " Hz por cliente " << player_id << "\n";
            broadcast_config(state);
        }
        return true;
    }
    
    // ========== COMANDO: SET_RUN <0|1> ==========
    int run_val;
    if (sscanf(buffer, "SET_RUN %d", &run_val) == 1) {
        state.run_sim = (run_val != 0);
        std::cout << "[CONFIG] Simulación " << (state.run_sim ? "ACTIVADA" : "PAUSADA") << " por cliente " << player_id << "\n";
        broadcast_config(state);
        return true;
    }
    
    // ========== COMANDO: CLEAR ==========
    if (strcmp(cmd, "CLEAR") == 0) {
        for (int i = 0; i < state.rows; i++) {
            for (int j = 0; j < state.cols; j++) {
                state.current_grid[i][j] = CELL_DEAD;
            }
        }
        std::cout << "[CONFIG] Grilla limpiada por cliente " << player_id << "\n";
        return true;
    }
    
    // ========== COMANDO: GET_CONFIG ==========
    if (strcmp(cmd, "GET_CONFIG") == 0) {
        std::stringstream ss;
        ss << "CONFIG_UPDATE\n";
        ss << "FREQ " << state.frecuencia << "\n";
        ss << "RUN " << (state.run_sim ? 1 : 0) << "\n";
        ss << "PLAYER_ID " << player_id << "\n";
        ss << "CLIENTS " << state.client_count() << "\n";
        ss << "END\n";
        send_to_client(client_socket, ss.str());
        return true;
    }
    
    // ========== COMANDO: STEP (avanzar un paso manualmente) ==========
    if (strcmp(cmd, "STEP") == 0 && !state.run_sim) {
        update_grid(state.current_grid, GRID_ROWS, GRID_COLS);
        std::cout << "[CONFIG] Step manual por cliente " << player_id << "\n";
        return true;
    }
    
    // Comando desconocido
    if (DEBUG_RECV) {
        std::cerr << "[RECV] Comando desconocido: " << cmd << "\n";
    }
    
    return true;
}

void broadcast_update(ServerState& state) {
    if (state.client_sockets.empty()) return;
    
    std::stringstream ss;
    bool modified = false;
    int changes_count = 0;
    
    for(int i = 0; i < GRID_ROWS; ++i) {
        for(int j = 0; j < GRID_COLS; ++j) {
            if(state.current_grid[i][j] != state.previous_grid[i][j]) {
                if(!modified) {
                    ss << "GRID_UPDATE\n";
                    modified = true;
                }
                // Enviar el valor real de la celda (player_id o 0)
                ss << i << " " << j << " " << static_cast<int>(state.current_grid[i][j]) << "\n";
                changes_count++;
                state.previous_grid[i][j] = state.current_grid[i][j];
            }
        }
    }
    
    if(modified) {
        ss << "END\n";
        std::string update = ss.str();
        
        if(DEBUG_SEND) {
            std::cout << "[SEND] " << changes_count << " changes -> " 
                      << state.client_count() << " clients\n";
        }
        
        // Enviar a TODOS los clientes
        std::vector<int> to_remove;
        for (int client_fd : state.client_sockets) {
            ssize_t sent = send(client_fd, update.c_str(), update.size(), MSG_NOSIGNAL);
            if (sent < 0 && (errno == EPIPE || errno == ECONNRESET)) {
                to_remove.push_back(client_fd);
            }
        }
        
        // Remover clientes desconectados
        for (int fd : to_remove) {
            std::cout << "[SERVER] Cliente desconectado durante broadcast\n";
            state.remove_client(fd);
        }
        
        if (state.logger) {
            state.logger->log(PacketDirection::OUTGOING, update, update.size());
        }
    }
}

void show_help() {
    std::cout << "\nServidor Multi-Cliente del Juego de la Vida de Conway\n";
    std::cout << "=====================================================\n\n";
    std::cout << "Uso: conway_server [opciones]\n\n";
    std::cout << "Opciones:\n";
    std::cout << "  -h, --help           Muestra esta ayuda\n";
    std::cout << "  -p, --port PORT      Puerto (default: 6969)\n";
    std::cout << "  -l, --log FILE       Archivo de log (default: server_packets.log)\n";
    std::cout << "  -v, --verbose        Logs en consola\n";
    std::cout << "  -f, --freq FREQ      Frecuencia de simulación (default: 1)\n\n";
}

int main(int argc, char* argv[]) {
    int port = 6969;
    int frecuencia = 1;
    std::string log_file = "server_packets.log";
    bool verbose = false;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            show_help();
            return 0;
        }
        else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        }
        else if ((arg == "-l" || arg == "--log") && i + 1 < argc) {
            log_file = argv[++i];
        }
        else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        }
        else if ((arg == "-f" || arg == "--freq") && i + 1 < argc) {
            frecuencia = std::stoi(argv[++i]);
        }
    }
    
    PacketLogger logger(log_file, EndpointType::SERVER, verbose);
    
    std::cout << "╔═══════════════════════════════════════════════════╗\n";
    std::cout << "║     CONWAY'S GAME OF LIFE - MULTI-CLIENT SERVER   ║\n";
    std::cout << "╚═══════════════════════════════════════════════════╝\n\n";
    std::cout << "[CONFIG] Puerto: " << port << "\n";
    std::cout << "[CONFIG] Max clientes: " << MAX_CLIENTS << "\n";
    std::cout << "[CONFIG] Log: " << log_file << "\n";
    std::cout << "[CONFIG] Patrones: " << PatternRegistry::instance().count() << "\n\n";
    
    ServerState state(GRID_ROWS, GRID_COLS);
    state.logger = &logger;
    state.frecuencia = frecuencia;
    
    if (state.error) {
        std::cerr << "[ERROR] Fallo al inicializar\n";
        return 1;
    }
    
    logger.log_event("SERVER_START", "Puerto " + std::to_string(port));
    std::cout << "[SERVER] Esperando conexiones...\n\n";
    
    // Bucle principal con poll()
    while(true) {
        // Poll con timeout de 10ms
        int poll_count = poll(state.poll_fds.data(), state.poll_fds.size(), 10);
        
        if (poll_count < 0) {
            if (errno != EINTR) perror("poll() failed");
            continue;
        }
        
        // Procesar eventos
        for (size_t i = 0; i < state.poll_fds.size(); i++) {
            if (!(state.poll_fds[i].revents & POLLIN)) continue;
            
            if (state.poll_fds[i].fd == state.tcp_socket) {
                // Nueva conexión
                accept_new_client(state);
            } else {
                // Datos de cliente existente
                if (!receive_command(state.poll_fds[i].fd, state)) {
                    std::cout << "[SERVER] Cliente desconectado\n";
                    state.remove_client(state.poll_fds[i].fd);
                    break;  // poll_fds cambió, salir del loop
                }
            }
        }
        
        // Simulación (solo si está activa)
        if (state.run_sim) {
            update_grid(state.current_grid, GRID_ROWS, GRID_COLS);
        }
        broadcast_update(state);
        
        usleep(100000 / state.frecuencia);
    }

    return 0;
}
