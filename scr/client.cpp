#include <iostream>
#include <stdio.h>
#include <stdbool.h>

#include <sstream> 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "appState.h"

#define DEBUG_SEND 0
#define DEBUG_RECV 1


void init_conection(AppState *app_state){
    /*Abro socket*/
    app_state->client_socket = socket(
        AF_INET,     /* TPV4 */
        SOCK_STREAM, /* TCP */
        0            /* dont care */
    );

    if(app_state->client_socket < 0) {
        perror("socket() failed");
        return;
    }

    
    app_state->server_addrs.sin_port = htons(app_state->server_port);   //6969
    app_state->server_addrs.sin_family = AF_INET;
    app_state->server_addrs.sin_addr.s_addr = INADDR_ANY;               //DEBERÍA IR IP? ES LOCAL AHORA
    
    if (connect(app_state->client_socket, (const struct sockaddr *)&app_state->server_addrs,sizeof(app_state->server_addrs)) < 0){
        perror("connect() failed");
        close(app_state->client_socket);
        return;
    }
    
    //Limpiar Grilla si la conexión fue exitosa
    clear_grid(app_state->grid, app_state->rows, app_state->cols);
    app_state->multiplayer = true;
    std::cout << "Multiplayer conectado\n";
    
    
    fcntl(app_state->client_socket, F_SETFL, O_NONBLOCK);
}

/*

void makeRequest(int client_socket, char* send_buffer, char* rcv_buffer){
    // En cada tick se hace una petición, la misma consiste en un elemento que quiere agregar
    // el cliente a la pantalla (se manda con pattern, posicion) 
    if(send(client_socket, (const void *) send_buffer, message_len, 0)< 0){
        perror("send() failed");
    }
    

    // La recepción debe incluir todos los cambios en la grilla (calculados en el servidor)
    int n = recv(client_socket, (void *)rcv_buffer, sizeof(rcv_buffer)-1, 0);
    if(n< 0){
        perror("recv() failed");
    }else if (n == 0) {
        puts("Server closed connection");
    } 
    
    // Deconvertir msj de llegada en un vector de cambios
    // Aplicar cambios a grilla actual

    if(PRINT_INCOMING_MESSAJES){
        rcv_buffer[n] = '\0'; 
        printf("Mensaje Recibido: \n --- \n%s\n ---\n\r", rcv_buffer);
    }


}


int client_loop()
{
    int error =0;
    ssize_t n=0;
    char rcv_buffer[1024];
    char send_buffer[1024];

    while(true){
        makeRequest(app_state->client_socket, send_buffer, rcv_buffer);
    }
    close(client_socket);
    return error;
}
*/


void send_pattern(AppState* app_state, SDL_Window* window, viewpoint* vp, int x, int y) {
    VisibleRange cols_range = get_visible_columns(vp, ROWS, COLS);
    VisibleRange rows_range = get_visible_rows(vp, ROWS, COLS);

    int current_width, current_height;
    SDL_GetWindowSize(window, &current_width, &current_height);

    // Convertir coordenadas pantalla a grid
    int grid_x = cols_range.start + (x * (cols_range.end - cols_range.start) / static_cast<float>(current_width));
    int grid_y = rows_range.start + (y * (rows_range.end - rows_range.start) / static_cast<float>(current_height));
    
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "ADD_PATTERN %s %d %d", 
             app_state->current_pattern.c_str(), grid_y, grid_x);
    
    if(DEBUG_SEND)std::cout << "ADD_PATTERN "<< app_state->current_pattern << " "<< grid_y << " " << grid_x << "\n";
    
    send(app_state->client_socket, buffer, strlen(buffer), 0);
}

void receive_update(AppState* app_state) {
    char buffer[4096];
    int received = recv(app_state->client_socket, buffer, sizeof(buffer)-1, 0);
    
    if(received<= 0 || errno == EWOULDBLOCK || errno == EAGAIN){
        if(DEBUG_RECV) std::cout << "Sin cambios\n";
        return;  
    }

    if(received > 0) {
        buffer[received] = '\0';
        std::stringstream ss(buffer);
        std::string line;
        
        while(std::getline(ss, line)) {
            if(line == "GRID_UPDATE") continue;
            if(line == "END") break;
            
            int row, col, state;
            sscanf(line.c_str(), "%d %d %d", &row, &col, &state);
            
            if(DEBUG_RECV)std::cout << &row << " "<< &col << " "<<  &state << "\n";
            app_state->grid[row][col] = state;
        }
    }

}