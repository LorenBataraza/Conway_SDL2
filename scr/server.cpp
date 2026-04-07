#include <stdio.h>
#include <iostream>
#include <stdbool.h>

// Communication
#include <sstream> 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "grid.h"

#define DEBUG_INIT 0
#define DEBUG_SEND 1
#define DEBUG_RECV 0

struct ServerState{
    //GRID
    bool** current_grid;
    bool** previous_grid;
    
    int rows, cols;
    // Simulation Parameters 
    bool run_sim=true;
    int frecuencia=1;

    // Communication Parameters
    int tcp_socket = 0;
    struct sockaddr_in bind_addr;
    int server_port = 6969;

    int client_port_1=0;
    int client_port_2=0;
    int client_socket_1 = 0;
    int client_socket_2 = 0;

    // Temporal value for error handling
    int temp = 0;
    int error = 0;
    int enabled = 1;

    ServerState(int initial_rows, int initial_cols) 
    : rows{initial_rows},          // Inicialización uniforme (C++11)
      cols{initial_cols},
      current_grid{construct_grid(initial_rows, initial_cols)},
      previous_grid{construct_grid(initial_rows, initial_cols)}{
    /* Initilize */
    memset(&bind_addr, 0, sizeof(bind_addr));
    tcp_socket = socket(
        AF_INET,     /* TPV4 */
        SOCK_STREAM, /* TCP */
        0            /* dont care */
    );

    if (tcp_socket < 0)
    {
        perror("socket() failed");
        error=1;
    }
    if(DEBUG_INIT)printf("socket creation succeeded\n");

    /*Set up for multiple uses for the same port*/
    if (setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) == -1) {
        perror("setsockopt() failed");
        error=1;
        close(tcp_socket);
    }
    
    bind_addr.sin_port = htons(server_port);
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = INADDR_ANY;

    temp = bind(tcp_socket, (const struct sockaddr *)&bind_addr, sizeof(bind_addr));
    if (temp < 0)
    {
        perror("bind() failed");
        error = 1;
    }
    if(DEBUG_INIT)printf("bind succeeded\n");

    temp = listen(tcp_socket, SOMAXCONN);
    if (temp < 0)
    {
        perror("listen() failed");
        error = 1;
    };
    if(DEBUG_INIT)puts("listen() succeded");
    if(DEBUG_INIT)printf("Waiting for connections...\n");    

    }

};

void receive_pattern(int client_socket, ServerState& state) {
    char buffer[1024];
    
    int received = recv(client_socket, buffer, sizeof(buffer)-1, 0);
    if(received <= 0 || errno == EWOULDBLOCK || errno == EAGAIN){
        if(DEBUG_RECV) std::cout << "Sin mensajes\n";
        return;   
    }
    
    buffer[received] = '\0';
    char pattern[50];
    int row, col;
    if(sscanf(buffer, "ADD_PATTERN %49s %d %d", pattern, &row, &col) == 3) {
        if(DEBUG_RECV) std::cout << "ADD_PATTERN" << " "<<  pattern<< " "<< row<< " "<<  col << "\n";
        load_pattern_into_grid(pattern, state.current_grid, row, col, ROWS, COLS);
    }
}

void broadcast_update(ServerState& state) {
    std::stringstream ss;
    bool modified=false;
    
    for(int i = 0; i < ROWS; ++i) {
        for(int j = 0; j < COLS; ++j) {
            if(state.current_grid[i][j]!=state.previous_grid[i][j]) {
                
                if(!modified){
                    ss << "GRID_UPDATE\n";
                    modified = true;
                }
                ss << i << " " << j << "\n";
                // Las igualo
                state.previous_grid[i][j]!=state.current_grid[i][j];
            }
        }
    }
    if(modified)ss << "END\n";

    std::string update = ss.str();
    if(DEBUG_SEND) std::cout << update;
    send(state.client_socket_1, update.c_str(), update.size(), 0);
}

int main() {
    ServerState server_state(ROWS,COLS);

    // Aceptar conexión antes de entrar al bucle
    socklen_t addr_len = sizeof(server_state.bind_addr);
    server_state.client_socket_1 = accept(
        server_state.tcp_socket,
        (struct sockaddr*)&server_state.bind_addr,
        &addr_len
    );
    if (server_state.client_socket_1 < 0) {
        perror("accept() failed");
        return 1;
    }
    if(DEBUG_INIT)puts("Got a connection");

    // SETEO SOCKET COMO NO BLOQUEANTE 
    // fcntl(server_state.client_socket_1, F_SETFL, O_NONBLOCK);

    // Bucle principal de simulación
    while(true) {
        receive_pattern(server_state.client_socket_1, server_state);
        update_grid(server_state.current_grid, ROWS, COLS);
        broadcast_update(server_state);
        usleep(10000 / server_state.frecuencia);
    }


    return 0;
}

