#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

/*
  Componentes a probar:
  Recepción correcta de comandos ADD_PATTERN.
  Generación adecuada de GRID_UPDATE.
  Comportamiento con múltiples clientes.
  Tiempos de respuesta bajo carga.
*/


TEST_CASE("Recepción de ADD_PATTERN actualiza la grilla", "[comandos]") {
    // 1. Setup
    ServerState test_state(ROWS, COLS);
    int test_socket = socket(AF_INET, SOCK_STREAM, 0);
    // ... conectar al socket del servidor

    // 2. Enviar comando
    const char* cmd = "ADD_PATTERN glider 0 0";
    send(test_socket, cmd, strlen(cmd), 0);

    // 3. Procesar
    receive_pattern(test_socket, test_state);

    // 4. Verificar cambios en la grilla
    REQUIRE(test_state.current_grid[0][0] == true);
    REQUIRE(test_state.current_grid[0][1] == true);
}



TEST_CASE("Recepción de ADD_PATTERN actualiza la grilla", "[comandos]") {
    // 1. Setup
    ServerState test_state(ROWS, COLS);
    int test_socket = socket(AF_INET, SOCK_STREAM, 0);
    // ... conectar al socket del servidor

    // 2. Enviar comando
    const char* cmd = "ADD_PATTERN glider 0 0";
    send(test_socket, cmd, strlen(cmd), 0);

    // 3. Procesar
    receive_pattern(test_socket, test_state);

    // 4. Verificar cambios en la grilla
    REQUIRE(test_state.current_grid[0][0] == true);
    REQUIRE(test_state.current_grid[0][1] == true);
}


TEST_CASE("Cambios en grilla generan GRID_UPDATE", "[broadcast]") {
    ServerState test_state(ROWS, COLS);
    // ... configuración de sockets

    // Forzar un cambio en la grilla
    test_state.current_grid[5][5] = true;

    // Capturar salida del broadcast
    std::stringstream buffer;
    auto old_cout = std::cout.rdbuf(buffer.rdbuf());
    
    broadcast_update(test_state);
    
    std::cout.rdbuf(old_cout);
    std::string output = buffer.str();

    // Verificar formato
    REQUIRE(output.find("GRID_UPDATE") != std::string::npos);
    REQUIRE(output.find("5 5") != std::string::npos);
    REQUIRE(output.find("END") != std::string::npos);
}


TEST_CASE("Tiempo de respuesta bajo carga", "[performance]") {
    auto start = std::chrono::high_resolution_clock::now();
    
    // 1. Generar 1000 comandos
    for (int i = 0; i < 1000; ++i) {
        std::string cmd = "ADD_PATTERN block " + std::to_string(i%ROWS) + " " + std::to_string(i%COLS);
        send(client_sock, cmd.c_str(), cmd.size(), 0);
    }

    // 2. Medir tiempo hasta recibir todas las actualizaciones
    int updates = 0;
    while (updates < 1000) {
        char buf[1024];
        int received = recv(client_sock, buf, sizeof(buf), 0);
        if (received > 0) {
            std::string response(buf, received);
            updates += std::count(response.begin(), response.end(), '\n') / 3; // Asume formato
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    WARN("Tiempo promedio por operación: " << duration.count()/1000.0 << "ms");
    REQUIRE(duration.count() < 1000); // Umbral de 1 segundo
}


TEST_CASE("Comportamiento con EWOULDBLOCK", "[nonblocking]") {
    // 1. Configurar socket no bloqueante
    int flags = fcntl(test_socket, F_GETFL, 0);
    fcntl(test_socket, F_SETFL, flags | O_NONBLOCK);

    // 2. Intentar leer sin datos
    char buffer[1024];
    int received = recv(test_socket, buffer, sizeof(buffer), 0);
    
    // 3. Verificar comportamiento esperado
    REQUIRE((received == -1 && errno == EAGAIN));
}
