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

// Motor (para el cliente de referencia que valida el lockstep)
#include "board.h"
#include "automaton.h"

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

TEST_F(PatternTest, TickFormat) {
    // El servidor emite un TICK ligero (gen + hash) por paso, en lockstep.
    ASSERT_TRUE(conn->send_message("ADD_PATTERN glider 50 50"));
    wait_for_processing(150);

    std::string response = conn->receive_message();
    if (!response.empty()) {
        EXPECT_THAT(response, ::testing::HasSubstr("TICK"))
            << "Respuesta no contiene TICK. Recibido: " << response;
    }
}

TEST_F(PatternTest, TickHasGenAndHash) {
    ASSERT_TRUE(conn->send_message("ADD_PATTERN glider 50 50"));
    wait_for_processing(150);

    std::string response = conn->receive_message();
    if (!response.empty()) {
        std::stringstream ss(response);
        std::string line;
        bool found_valid_tick = false;

        while (std::getline(ss, line)) {
            unsigned long long gen = 0, hash = 0;
            if (sscanf(line.c_str(), "TICK %llu %llu", &gen, &hash) == 2) {
                found_valid_tick = true;
                break;
            }
        }
        EXPECT_TRUE(found_valid_tick)
            << "No se recibió un TICK válido. Recibido: " << response;
    }
}

TEST_F(PatternTest, FullGridSnapshotOnRequest) {
    // GET_FULL_GRID debe devolver un snapshot completo (sync/resync).
    ASSERT_TRUE(conn->send_message("ADD_PATTERN block 60 60"));
    wait_for_processing(100);
    conn->receive_message();  // drenar TICKs pendientes

    ASSERT_TRUE(conn->send_message("GET_FULL_GRID"));
    wait_for_processing(100);

    std::string response = conn->receive_message();
    // El stream contiene TICKs y/o el bloque FULL_GRID.
    EXPECT_THAT(response, ::testing::AnyOf(
        ::testing::HasSubstr("FULL_GRID"),
        ::testing::HasSubstr("TICK")))
        << "No se recibió FULL_GRID ni TICK. Recibido: " << response;
    EXPECT_TRUE(conn->is_connected());
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
// Tests de Lockstep + Hash
// ============================================================================

// Cliente de REFERENCIA: reproduce el protocolo lockstep (FULL_GRID / PATTERN /
// TICK) con el mismo Automaton determinista y verifica que su hash local
// coincide con el hash que anuncia el servidor en cada TICK.
TEST_F(ServerTest, LockstepReferenceClientStaysInSync) {
    ASSERT_TRUE(conn->connect()) << conn->get_last_error();

    // Sync inicial + algo de actividad (el cañón genera gliders continuamente).
    // Nota: el servidor procesa un comando por recv(), así que espaciamos los
    // envíos para que no se fusionen en un mismo segmento TCP (como ocurre en el
    // uso real, donde los comandos vienen de acciones del usuario).
    ASSERT_TRUE(conn->send_message("GET_FULL_GRID"));
    wait_for_processing(100);
    ASSERT_TRUE(conn->send_message("ADD_PATTERN glider_gun 5 5 1 0 0"));

    Automaton local(200, 400, &RULE_NORMAL);
    bool awaiting = true;    // ignora TICKs hasta el FULL_GRID
    bool synced = false;
    int parse_mode = 0;      // 0=none, 4=FULL_GRID
    unsigned long long full_gen = 0;
    int ticks_ok = 0, ticks_bad = 0;

    auto process_line = [&](const std::string& line) {
        if (line.rfind("FULL_GRID ", 0) == 0) {
            sscanf(line.c_str(), "FULL_GRID %llu", &full_gen);
            local.clear();
            parse_mode = 4;
            return;
        }
        if (line == "END") {
            if (parse_mode == 4) {
                local.set_generation(full_gen);
                local.mark_all_dirty();
                awaiting = false;
                synced = true;
            }
            parse_mode = 0;
            return;
        }
        if (line.rfind("TICK ", 0) == 0) {
            unsigned long long gen = 0, h = 0;
            if (sscanf(line.c_str(), "TICK %llu %llu", &gen, &h) == 2 && !awaiting) {
                if (gen == local.generation() + 1) {
                    local.step();
                    if (local.hash() == h) ticks_ok++; else ticks_bad++;
                }
            }
            return;
        }
        if (line.rfind("PATTERN ", 0) == 0) {
            unsigned long long ag = 0; char name[64]; int r, c, p, mh, mv;
            if (sscanf(line.c_str(), "PATTERN %llu %63s %d %d %d %d %d",
                       &ag, name, &r, &c, &p, &mh, &mv) == 7 && !awaiting) {
                if (ag == local.generation())
                    local.add_pattern(name, r, c, static_cast<CellValue>(p), mh != 0, mv != 0);
            }
            return;
        }
        if (parse_mode == 4) {
            int r, c, v;
            if (sscanf(line.c_str(), "%d %d %d", &r, &c, &v) == 3)
                local.board().set(r, c, static_cast<CellValue>(v));
        }
    };

    // Seguir el stream durante ~1.5 s.
    std::string acc;
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count() < 1500) {
        std::string chunk = conn->receive_message();
        if (chunk.empty()) continue;
        acc += chunk;
        size_t pos;
        while ((pos = acc.find('\n')) != std::string::npos) {
            process_line(acc.substr(0, pos));
            acc.erase(0, pos + 1);
        }
    }

    EXPECT_TRUE(synced) << "No se recibió el FULL_GRID inicial";
    EXPECT_GT(ticks_ok, 0) << "No se validó ningún TICK";
    EXPECT_EQ(ticks_bad, 0)
        << "El cliente de referencia se desincronizó del servidor ("
        << ticks_ok << " ok, " << ticks_bad << " fallidos)";
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
