#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <cstring>

// Networking
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

/**
 * Tests del servidor del Juego de la Vida usando Google Test
 */

// Configuración global
namespace {
    constexpr int DEFAULT_PORT = 6969;
    constexpr int RECV_TIMEOUT_MS = 1000;
    constexpr int RECV_BUFFER_SIZE = 8192;
    constexpr const char* DEFAULT_SERVER_IP = "127.0.0.1";
}

/**
 * Clase helper para manejar conexiones con el servidor
 */
class ServerConnection {
public:
    ServerConnection(const std::string& ip = DEFAULT_SERVER_IP, int port = DEFAULT_PORT)
        : server_ip_(ip), server_port_(port), socket_fd_(-1), connected_(false) {}
    
    ~ServerConnection() {
        disconnect();
    }
    
    // No copiable
    ServerConnection(const ServerConnection&) = delete;
    ServerConnection& operator=(const ServerConnection&) = delete;
    
    bool connect() {
        socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd_ < 0) {
            last_error_ = std::string("socket() failed: ") + strerror(errno);
            return false;
        }
        
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port_);
        
        if (inet_pton(AF_INET, server_ip_.c_str(), &server_addr.sin_addr) <= 0) {
            last_error_ = "inet_pton() failed";
            close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }
        
        if (::connect(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            last_error_ = std::string("connect() failed: ") + strerror(errno);
            close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }
        
        // Configurar timeout para recv
        struct timeval tv;
        tv.tv_sec = RECV_TIMEOUT_MS / 1000;
        tv.tv_usec = (RECV_TIMEOUT_MS % 1000) * 1000;
        setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        
        connected_ = true;
        return true;
    }
    
    void disconnect() {
        if (socket_fd_ >= 0) {
            close(socket_fd_);
            socket_fd_ = -1;
        }
        connected_ = false;
    }
    
    bool send_message(const std::string& msg) {
        if (!connected_) {
            last_error_ = "Not connected";
            return false;
        }
        
        ssize_t sent = send(socket_fd_, msg.c_str(), msg.length(), 0);
        if (sent < 0) {
            last_error_ = std::string("send() failed: ") + strerror(errno);
            return false;
        }
        
        bytes_sent_ += sent;
        return true;
    }
    
    std::string receive_message() {
        if (!connected_) {
            last_error_ = "Not connected";
            return "";
        }
        
        char buffer[RECV_BUFFER_SIZE];
        ssize_t received = recv(socket_fd_, buffer, sizeof(buffer) - 1, 0);
        
        if (received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                last_error_ = "Timeout waiting for response";
            } else {
                last_error_ = std::string("recv() failed: ") + strerror(errno);
            }
            return "";
        }
        
        if (received == 0) {
            last_error_ = "Server closed connection";
            connected_ = false;
            return "";
        }
        
        buffer[received] = '\0';
        bytes_received_ += received;
        return std::string(buffer);
    }
    
    bool is_connected() const { return connected_; }
    std::string get_last_error() const { return last_error_; }
    size_t get_bytes_sent() const { return bytes_sent_; }
    size_t get_bytes_received() const { return bytes_received_; }

private:
    std::string server_ip_;
    int server_port_;
    int socket_fd_;
    bool connected_;
    std::string last_error_;
    size_t bytes_sent_ = 0;
    size_t bytes_received_ = 0;
};

// ============================================================================
// Test Fixture
// ============================================================================

class ServerTest : public ::testing::Test {
protected:
    std::unique_ptr<ServerConnection> conn;
    
    void SetUp() override {
        conn = std::make_unique<ServerConnection>();
    }
    
    void TearDown() override {
        if (conn) {
            conn->disconnect();
        }
    }
    
    // Helper para esperar un poco después de enviar mensajes
    void wait_for_processing(int ms = 50) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
};

// ============================================================================
// Tests de Conexión
// ============================================================================

TEST_F(ServerTest, BasicConnection) {
    ASSERT_TRUE(conn->connect()) 
        << "Fallo al conectar: " << conn->get_last_error();
    EXPECT_TRUE(conn->is_connected());
}

TEST_F(ServerTest, Reconnection) {
    // Primera conexión
    ASSERT_TRUE(conn->connect()) 
        << "Primera conexión falló: " << conn->get_last_error();
    
    conn->disconnect();
    EXPECT_FALSE(conn->is_connected());
    
    // Esperar antes de reconectar
    wait_for_processing(100);
    
    // Segunda conexión
    ASSERT_TRUE(conn->connect()) 
        << "Reconexión falló: " << conn->get_last_error();
    EXPECT_TRUE(conn->is_connected());
}

TEST_F(ServerTest, ConnectionToInvalidPort) {
    auto bad_conn = std::make_unique<ServerConnection>("127.0.0.1", 9999);
    EXPECT_FALSE(bad_conn->connect());
    EXPECT_FALSE(bad_conn->is_connected());
}

// ============================================================================
// Tests de Envío de Patrones
// ============================================================================

class PatternTest : public ServerTest {
protected:
    void SetUp() override {
        ServerTest::SetUp();
        ASSERT_TRUE(conn->connect()) << "Setup: No se pudo conectar al servidor";
    }
};

TEST_F(PatternTest, SendGlider) {
    EXPECT_TRUE(conn->send_message("ADD_PATTERN glider 10 10"));
    wait_for_processing();
    EXPECT_TRUE(conn->is_connected()) << "Servidor cerró conexión inesperadamente";
}

TEST_F(PatternTest, SendBlock) {
    EXPECT_TRUE(conn->send_message("ADD_PATTERN block 20 20"));
    wait_for_processing();
    EXPECT_TRUE(conn->is_connected());
}

TEST_F(PatternTest, SendBlinker) {
    EXPECT_TRUE(conn->send_message("ADD_PATTERN blinker 30 30"));
    wait_for_processing();
    EXPECT_TRUE(conn->is_connected());
}

TEST_F(PatternTest, SendToad) {
    EXPECT_TRUE(conn->send_message("ADD_PATTERN toad 40 40"));
    wait_for_processing();
    EXPECT_TRUE(conn->is_connected());
}

TEST_F(PatternTest, SendBeacon) {
    EXPECT_TRUE(conn->send_message("ADD_PATTERN beacon 50 50"));
    wait_for_processing();
    EXPECT_TRUE(conn->is_connected());
}

TEST_F(PatternTest, SendPulsar) {
    EXPECT_TRUE(conn->send_message("ADD_PATTERN pulsar 60 60"));
    wait_for_processing();
    EXPECT_TRUE(conn->is_connected());
}

TEST_F(PatternTest, SendLWSS) {
    EXPECT_TRUE(conn->send_message("ADD_PATTERN lwss 70 70"));
    wait_for_processing();
    EXPECT_TRUE(conn->is_connected());
}

TEST_F(PatternTest, SendGliderGun) {
    EXPECT_TRUE(conn->send_message("ADD_PATTERN glider_gun 5 5"));
    wait_for_processing();
    EXPECT_TRUE(conn->is_connected());
}

TEST_F(PatternTest, SendInvalidPattern) {
    // Servidor no debería crashear con patrón inválido
    EXPECT_TRUE(conn->send_message("ADD_PATTERN patron_inexistente 10 10"));
    wait_for_processing();
    EXPECT_TRUE(conn->is_connected()) 
        << "Servidor cerró conexión con patrón inválido";
}

TEST_F(PatternTest, SendMalformedMessage) {
    // Mensaje con formato incorrecto
    EXPECT_TRUE(conn->send_message("INVALID_COMMAND"));
    wait_for_processing();
    EXPECT_TRUE(conn->is_connected()) 
        << "Servidor cerró conexión con mensaje malformado";
}

// ============================================================================
// Tests de Formato de Respuesta
// ============================================================================

TEST_F(PatternTest, GridUpdateFormat) {
    // Enviar patrón para generar actualización
    ASSERT_TRUE(conn->send_message("ADD_PATTERN glider 50 50"));
    wait_for_processing(100);
    
    std::string response = conn->receive_message();
    
    // Puede que no haya respuesta si no hubo cambios
    if (!response.empty()) {
        EXPECT_THAT(response, ::testing::HasSubstr("GRID_UPDATE"))
            << "Respuesta no contiene GRID_UPDATE";
        EXPECT_THAT(response, ::testing::HasSubstr("END"))
            << "Respuesta no contiene END";
    }
}

TEST_F(PatternTest, GridUpdateDataLines) {
    ASSERT_TRUE(conn->send_message("ADD_PATTERN glider 50 50"));
    wait_for_processing(100);
    
    std::string response = conn->receive_message();
    
    if (!response.empty()) {
        std::stringstream ss(response);
        std::string line;
        int data_lines = 0;
        bool in_update = false;
        
        while (std::getline(ss, line)) {
            if (line == "GRID_UPDATE") {
                in_update = true;
                continue;
            }
            if (line == "END") {
                break;
            }
            if (in_update && !line.empty()) {
                data_lines++;
                
                // Verificar formato: debe tener al menos 2 números
                int row, col;
                int parsed = sscanf(line.c_str(), "%d %d", &row, &col);
                EXPECT_GE(parsed, 2) 
                    << "Línea de datos mal formateada: " << line;
            }
        }
        
        // Glider tiene 5 celdas
        EXPECT_GE(data_lines, 1) << "No se recibieron datos de celdas";
    }
}

// ============================================================================
// Tests de Coordenadas Límite
// ============================================================================

TEST_F(PatternTest, CoordinatesLowerBound) {
    EXPECT_TRUE(conn->send_message("ADD_PATTERN point 0 0"));
    wait_for_processing();
    EXPECT_TRUE(conn->is_connected()) 
        << "Servidor falló con coordenadas (0, 0)";
}

TEST_F(PatternTest, CoordinatesUpperBound) {
    EXPECT_TRUE(conn->send_message("ADD_PATTERN point 99 99"));
    wait_for_processing();
    EXPECT_TRUE(conn->is_connected()) 
        << "Servidor falló con coordenadas (99, 99)";
}

TEST_F(PatternTest, CoordinatesNegative) {
    EXPECT_TRUE(conn->send_message("ADD_PATTERN point -5 -5"));
    wait_for_processing();
    EXPECT_TRUE(conn->is_connected()) 
        << "Servidor falló con coordenadas negativas";
}

TEST_F(PatternTest, CoordinatesOutOfRange) {
    EXPECT_TRUE(conn->send_message("ADD_PATTERN point 1000 1000"));
    wait_for_processing();
    EXPECT_TRUE(conn->is_connected()) 
        << "Servidor falló con coordenadas fuera de rango";
}

TEST_F(PatternTest, CoordinatesVeryLarge) {
    EXPECT_TRUE(conn->send_message("ADD_PATTERN point 999999 999999"));
    wait_for_processing();
    EXPECT_TRUE(conn->is_connected()) 
        << "Servidor falló con coordenadas muy grandes";
}

// ============================================================================
// Tests de Múltiples Patrones
// ============================================================================

TEST_F(PatternTest, MultiplePatterns) {
    const std::vector<std::string> patterns = {
        "glider", "block", "blinker", "toad", "beacon"
    };
    
    int sent = 0;
    for (size_t i = 0; i < patterns.size(); i++) {
        std::string msg = "ADD_PATTERN " + patterns[i] + " " + 
                         std::to_string(i * 10) + " " + std::to_string(i * 10);
        
        if (conn->send_message(msg)) {
            sent++;
        }
        wait_for_processing(20);
    }
    
    EXPECT_EQ(sent, static_cast<int>(patterns.size())) 
        << "No se enviaron todos los patrones";
    EXPECT_TRUE(conn->is_connected());
}

TEST_F(PatternTest, RapidFirePatterns) {
    // Enviar 50 patrones rápidamente
    const int NUM_PATTERNS = 50;
    int sent = 0;
    
    for (int i = 0; i < NUM_PATTERNS; i++) {
        std::string msg = "ADD_PATTERN point " + 
                         std::to_string(i % 100) + " " + 
                         std::to_string(i % 100);
        
        if (conn->send_message(msg)) {
            sent++;
        }
    }
    
    wait_for_processing(100);
    
    EXPECT_EQ(sent, NUM_PATTERNS);
    EXPECT_TRUE(conn->is_connected());
}

// ============================================================================
// Tests de Carga (Stress Tests)
// ============================================================================

TEST_F(PatternTest, MessageBurst100) {
    const int NUM_MESSAGES = 100;
    int sent = 0;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_MESSAGES; i++) {
        std::string msg = "ADD_PATTERN point " + 
                         std::to_string(i % 100) + " " + 
                         std::to_string(i % 100);
        
        if (conn->send_message(msg)) {
            sent++;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(end - start).count();
    
    EXPECT_EQ(sent, NUM_MESSAGES);
    EXPECT_TRUE(conn->is_connected());
    
    // Informativo: mostrar throughput
    double msgs_per_sec = sent / (duration / 1000.0);
    std::cout << "  [INFO] " << sent << " mensajes en " << duration 
              << " ms (" << msgs_per_sec << " msg/s)" << std::endl;
}

TEST_F(PatternTest, MessageBurst500) {
    const int NUM_MESSAGES = 500;
    int sent = 0;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_MESSAGES; i++) {
        std::string msg = "ADD_PATTERN point " + 
                         std::to_string(i % 100) + " " + 
                         std::to_string(i % 100);
        
        if (conn->send_message(msg)) {
            sent++;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(end - start).count();
    
    EXPECT_EQ(sent, NUM_MESSAGES);
    EXPECT_TRUE(conn->is_connected());
    
    double msgs_per_sec = sent / (duration / 1000.0);
    std::cout << "  [INFO] " << sent << " mensajes en " << duration 
              << " ms (" << msgs_per_sec << " msg/s)" << std::endl;
}

// ============================================================================
// Tests de Protocolo
// ============================================================================

TEST_F(PatternTest, EmptyMessage) {
    EXPECT_TRUE(conn->send_message(""));
    wait_for_processing();
    EXPECT_TRUE(conn->is_connected());
}

TEST_F(PatternTest, WhitespaceMessage) {
    EXPECT_TRUE(conn->send_message("   \t\n  "));
    wait_for_processing();
    EXPECT_TRUE(conn->is_connected());
}

TEST_F(PatternTest, PartialCommand) {
    EXPECT_TRUE(conn->send_message("ADD_PATTERN"));
    wait_for_processing();
    EXPECT_TRUE(conn->is_connected());
}

TEST_F(PatternTest, ExtraWhitespace) {
    EXPECT_TRUE(conn->send_message("ADD_PATTERN    glider    10    10"));
    wait_for_processing();
    EXPECT_TRUE(conn->is_connected());
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║    TESTS DEL SERVIDOR - JUEGO DE LA VIDA (Google Test)     ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "Servidor: " << DEFAULT_SERVER_IP << ":" << DEFAULT_PORT << "\n";
    std::cout << "NOTA: El servidor debe estar corriendo antes de ejecutar los tests.\n";
    std::cout << "\n";
    
    return RUN_ALL_TESTS();
}
