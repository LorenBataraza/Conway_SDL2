#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <mutex>
#include <ctime>
#include <memory>

/**
 * Dirección del paquete
 */
enum class PacketDirection {
    OUTGOING,  // Paquete saliente
    INCOMING   // Paquete entrante
};

/**
 * Tipo de endpoint
 */
enum class EndpointType {
    CLIENT,
    SERVER
};

/**
 * Logger de paquetes para debugging del protocolo de comunicación
 * 
 * Uso:
 *   PacketLogger logger("client_packets.log", EndpointType::CLIENT);
 *   logger.log(PacketDirection::OUTGOING, "ADD_PATTERN glider 10 20");
 *   logger.log(PacketDirection::INCOMING, "GRID_UPDATE\n5 10 1\nEND");
 */
class PacketLogger {
public:
    /**
     * Constructor
     * @param filename Nombre del archivo de log
     * @param endpoint Tipo de endpoint (CLIENT o SERVER)
     * @param console_output Si true, también imprime en consola
     */
    PacketLogger(const std::string& filename, EndpointType endpoint, bool console_output = false)
        : endpoint_(endpoint)
        , console_output_(console_output)
        , enabled_(true)
        , packet_count_(0)
    {
        log_file_.open(filename, std::ios::out | std::ios::app);
        if (!log_file_.is_open()) {
            std::cerr << "[PacketLogger] Error: No se pudo abrir " << filename << std::endl;
            enabled_ = false;
            return;
        }
        
        // Escribir header del log
        log_file_ << "\n========================================\n";
        log_file_ << "SESSION START: " << get_timestamp() << "\n";
        log_file_ << "Endpoint: " << (endpoint == EndpointType::CLIENT ? "CLIENT" : "SERVER") << "\n";
        log_file_ << "========================================\n\n";
        log_file_.flush();
    }

    ~PacketLogger() {
        if (log_file_.is_open()) {
            log_file_ << "\n========================================\n";
            log_file_ << "SESSION END: " << get_timestamp() << "\n";
            log_file_ << "Total packets: " << packet_count_ << "\n";
            log_file_ << "========================================\n";
            log_file_.close();
        }
    }

    // Deshabilitar copia
    PacketLogger(const PacketLogger&) = delete;
    PacketLogger& operator=(const PacketLogger&) = delete;

    /**
     * Registra un paquete en el log
     * @param direction Dirección del paquete (OUTGOING o INCOMING)
     * @param data Contenido del paquete
     * @param bytes_count Cantidad de bytes (opcional, se calcula si no se proporciona)
     */
    void log(PacketDirection direction, const std::string& data, int bytes_count = -1) {
        if (!enabled_) return;

        std::lock_guard<std::mutex> lock(mutex_);
        
        packet_count_++;
        
        std::string timestamp = get_timestamp();
        std::string dir_str = (direction == PacketDirection::OUTGOING) ? "OUT" : "IN ";
        int size = (bytes_count >= 0) ? bytes_count : static_cast<int>(data.size());
        
        std::stringstream entry;
        entry << "[" << timestamp << "] "
              << "[#" << std::setw(6) << std::setfill('0') << packet_count_ << "] "
              << "[" << dir_str << "] "
              << "[" << std::setw(5) << size << " bytes]\n";
        
        // Formatear el contenido del paquete
        entry << format_packet_data(data);
        entry << "\n";
        
        log_file_ << entry.str();
        log_file_.flush();
        
        if (console_output_) {
            std::cout << entry.str();
        }
    }

    /**
     * Registra un paquete con información adicional del socket
     */
    void log_with_socket(PacketDirection direction, const std::string& data, 
                         int socket_fd, const std::string& remote_addr = "", int remote_port = 0) {
        if (!enabled_) return;

        std::lock_guard<std::mutex> lock(mutex_);
        
        packet_count_++;
        
        std::string timestamp = get_timestamp();
        std::string dir_str = (direction == PacketDirection::OUTGOING) ? "OUT" : "IN ";
        
        std::stringstream entry;
        entry << "[" << timestamp << "] "
              << "[#" << std::setw(6) << std::setfill('0') << packet_count_ << "] "
              << "[" << dir_str << "] "
              << "[sock:" << socket_fd << "]";
        
        if (!remote_addr.empty()) {
            entry << " [" << remote_addr << ":" << remote_port << "]";
        }
        
        entry << " [" << data.size() << " bytes]\n";
        entry << format_packet_data(data);
        entry << "\n";
        
        log_file_ << entry.str();
        log_file_.flush();
        
        if (console_output_) {
            std::cout << entry.str();
        }
    }

    /**
     * Registra un evento personalizado
     */
    void log_event(const std::string& event_type, const std::string& message) {
        if (!enabled_) return;

        std::lock_guard<std::mutex> lock(mutex_);
        
        std::stringstream entry;
        entry << "[" << get_timestamp() << "] "
              << "[EVENT: " << event_type << "] "
              << message << "\n";
        
        log_file_ << entry.str();
        log_file_.flush();
        
        if (console_output_) {
            std::cout << entry.str();
        }
    }

    /**
     * Registra un error
     */
    void log_error(const std::string& error_message, int error_code = 0) {
        if (!enabled_) return;

        std::lock_guard<std::mutex> lock(mutex_);
        
        std::stringstream entry;
        entry << "[" << get_timestamp() << "] "
              << "[ERROR";
        
        if (error_code != 0) {
            entry << " code:" << error_code;
        }
        
        entry << "] " << error_message << "\n";
        
        log_file_ << entry.str();
        log_file_.flush();
        
        // Errores siempre a consola
        std::cerr << entry.str();
    }

    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }
    bool is_enabled() const { return enabled_; }
    
    void set_console_output(bool enabled) { console_output_ = enabled; }
    
    uint64_t get_packet_count() const { return packet_count_; }

private:
    std::string get_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::tm* tm = std::localtime(&time_t_now);
        
        std::stringstream ss;
        ss << std::put_time(tm, "%Y-%m-%d %H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3) << ms.count();
        
        return ss.str();
    }

    std::string format_packet_data(const std::string& data) {
        std::stringstream formatted;
        formatted << "  ┌─ PACKET DATA ─────────────────────────────────\n";
        
        std::istringstream stream(data);
        std::string line;
        int line_num = 0;
        
        while (std::getline(stream, line)) {
            formatted << "  │ " << std::setw(3) << ++line_num << ": ";
            
            // Escapar caracteres no imprimibles
            for (char c : line) {
                if (c >= 32 && c < 127) {
                    formatted << c;
                } else if (c == '\r') {
                    formatted << "\\r";
                } else if (c == '\t') {
                    formatted << "\\t";
                } else {
                    formatted << "\\x" << std::hex << std::setw(2) 
                              << std::setfill('0') << (int)(unsigned char)c;
                }
            }
            formatted << "\n";
        }
        
        formatted << "  └─────────────────────────────────────────────────";
        
        return formatted.str();
    }

    std::ofstream log_file_;
    EndpointType endpoint_;
    bool console_output_;
    bool enabled_;
    uint64_t packet_count_;
    std::mutex mutex_;
};

// Singleton global para acceso fácil
class GlobalPacketLogger {
public:
    static void init_client_logger(const std::string& filename = "client_packets.log", 
                                   bool console = false) {
        client_logger_ = std::make_unique<PacketLogger>(filename, EndpointType::CLIENT, console);
    }

    static void init_server_logger(const std::string& filename = "server_packets.log",
                                   bool console = false) {
        server_logger_ = std::make_unique<PacketLogger>(filename, EndpointType::SERVER, console);
    }

    static PacketLogger* client() { return client_logger_.get(); }
    static PacketLogger* server() { return server_logger_.get(); }

private:
    static inline std::unique_ptr<PacketLogger> client_logger_;
    static inline std::unique_ptr<PacketLogger> server_logger_;
};

// Macros para logging rápido
#define LOG_CLIENT_OUT(data) \
    if (GlobalPacketLogger::client()) GlobalPacketLogger::client()->log(PacketDirection::OUTGOING, data)

#define LOG_CLIENT_IN(data) \
    if (GlobalPacketLogger::client()) GlobalPacketLogger::client()->log(PacketDirection::INCOMING, data)

#define LOG_SERVER_OUT(data) \
    if (GlobalPacketLogger::server()) GlobalPacketLogger::server()->log(PacketDirection::OUTGOING, data)

#define LOG_SERVER_IN(data) \
    if (GlobalPacketLogger::server()) GlobalPacketLogger::server()->log(PacketDirection::INCOMING, data)
