#pragma once

#include <unordered_map>
#include <string>
#include <vector>

#include "net_compat.h"   // sockets portables (POSIX/Winsock) — antes que SDL

#include <SDL2/SDL.h>

#include "grid.h"
#include "automaton.h"
#include "connection_history.h"
#include "packet_logger.h"

// Forward declaration
struct Animation {
    SDL_Texture** frames = nullptr;
    Uint32* delays = nullptr;
    int frame_count = 0;
    int current_frame = 0;
    Uint32 last_update = 0;
    Uint32 total_duration = 0;
    int loop_count = 0;
    int width = 0;
    int height = 0;

    // Move constructor
    Animation() = default;
    Animation(Animation&& other) noexcept 
        : frames(other.frames)
        , delays(other.delays)
        , frame_count(other.frame_count)
        , current_frame(other.current_frame)
        , last_update(other.last_update)
        , total_duration(other.total_duration)
        , loop_count(other.loop_count)
        , width(other.width)
        , height(other.height)
    {
        other.frames = nullptr;
        other.delays = nullptr;
    }

    Animation& operator=(Animation&& other) noexcept {
        if (this != &other) {
            free_resources();
            frames = other.frames;
            delays = other.delays;
            frame_count = other.frame_count;
            current_frame = other.current_frame;
            last_update = other.last_update;
            total_duration = other.total_duration;
            loop_count = other.loop_count;
            width = other.width;
            height = other.height;
            other.frames = nullptr;
            other.delays = nullptr;
        }
        return *this;
    }

    // No copy
    Animation(const Animation&) = delete;
    Animation& operator=(const Animation&) = delete;

    ~Animation() {
        free_resources();
    }

private:
    void free_resources() {
        if (frames) {
            for (int i = 0; i < frame_count; ++i) {
                if (frames[i]) SDL_DestroyTexture(frames[i]);
            }
            delete[] frames;
            frames = nullptr;
        }
        delete[] delays;
        delays = nullptr;
    }
};

/**
 * Estado de la aplicación
 */
struct AppState {
    // ============== Recursos Gráficos ==============
    std::unordered_map<std::string, SDL_Texture*> patterns;
    std::unordered_map<std::string, Animation> animations;
    
    // ============== Grilla / Autómata ==============
    // El autómata posee la grilla (Board) y el conjunto de reglas (Ruleset).
    // `grid` es un alias ESTABLE a la grilla actual del autómata (CellValue**),
    // para el código de render/patrones/IO heredado.
    Automaton automaton;
    CellValue** grid;
    int rows, cols;

    // ============== Simulación Singleplayer ==============
    bool run_sim = true;
    int frecuencia = 10;  // Hz
    std::string current_pattern = "point";
    
    // ============== Multiplayer ==============
    bool multiplayer = false;
    socket_t client_socket = NET_INVALID_SOCKET_VALUE;
    struct sockaddr_in server_addrs;
    std::string server_ip = "127.0.0.1";
    int server_port = 6969;

    // Historial de conexiones previas (persistido en disco).
    std::vector<ConnectionEntry> connection_history;
    
    // ID del jugador local (asignado por el servidor)
    CellValue player_id = 1;

    // ============== Sincronización lockstep + hash ==============
    // El cliente avanza su propia grilla (mismo Automaton determinista) al
    // recibir cada TICK, y verifica el hash contra el del servidor.
    bool awaiting_resync = false;            // ignora TICKs hasta recibir FULL_GRID
    std::string net_rx_buffer;               // acumulador (mensajes parciales entre recv)
    int net_parse_mode = 0;                  // modo de parseo persistente entre recv()
    unsigned long long net_full_gen = 0;     // generación del FULL_GRID en curso

    // Logger de paquetes
    PacketLogger* packet_logger = nullptr;
    
    // Estadísticas de red
    struct NetworkStats {
        uint64_t packets_sent = 0;
        uint64_t packets_received = 0;
        uint64_t bytes_sent = 0;
        uint64_t bytes_received = 0;
        uint32_t latency_ms = 0;
    } net_stats;
    
    // ============== Modo Competición ==============
    enum class GameMode { NORMAL = 0, COMPETITION = 1 };
    GameMode game_mode = GameMode::NORMAL;
    
    // Estado de puntos de todos los jugadores
    struct PlayerScore {
        int victory_points = 0;
        int consumption_points = 200;
        int cells_alive = 0;
        bool active = false;
    };
    static constexpr int MAX_PLAYERS = 8;
    PlayerScore player_scores[MAX_PLAYERS + 1];  // índice 0 no usado
    
    int victory_goal = 60000;
    int num_clients = 1;
    
    // Puntos locales (atajo)
    int my_victory() const { return player_scores[player_id].victory_points; }
    int my_consumption() const { return player_scores[player_id].consumption_points; }
    int my_cells() const { return player_scores[player_id].cells_alive; }
    
    bool can_afford(int cost) const { 
        if (game_mode == GameMode::NORMAL) return true;
        return player_scores[player_id].consumption_points >= cost; 
    }

    // ============== UI ==============
    bool showParameters = true;
    bool showMultiplayerConf = false;
    bool showStructures = true;
    bool showMinimap = true;
    bool showNetworkStats = false;
    bool showScoreBar = true;  // Barra inferior con puntos
    bool showPlayerHUD = true; // HUD superior con info del jugador
    bool showHelp = false;     // Panel de ayuda (?)
    bool showWiki = false;     // Panel wiki (autómatas, vecindarios, etc.)
    
    // Espejado de patrones
    bool mirror_horizontal = false;
    bool mirror_vertical = false;

    // Configuración del minimapa
    int minimap_size = 150;

    // ============== Notificaciones (toast) + backup de grilla ==============
    // Al cargar un "ejemplo" de la Wiki en la grilla se guarda la grilla previa
    // aquí y se muestra una mini notificación (con opción de restaurar).
    std::string notification_text;
    Uint32 notification_time = 0;      // SDL_GetTicks() del último aviso (0 = ninguno)
    std::vector<CellValue> grid_backup;
    bool has_backup = false;

    void notify(const std::string& msg) {
        notification_text = msg;
        notification_time = SDL_GetTicks();
    }
    
    // ============== FPS ==============
    struct FPSCounter {
        Uint32 frame_count = 0;
        Uint32 last_time = 0;
        float current_fps = 0.0f;
        float update_interval = 500.0f;  // Actualizar cada 500ms
        
        void update() {
            frame_count++;
            Uint32 current_time = SDL_GetTicks();
            Uint32 elapsed = current_time - last_time;
            
            if (elapsed >= update_interval) {
                current_fps = (frame_count * 1000.0f) / elapsed;
                frame_count = 0;
                last_time = current_time;
            }
        }
        
        void reset() {
            frame_count = 0;
            last_time = SDL_GetTicks();
            current_fps = 0.0f;
        }
    } fps;

    // ============== Constructores ==============
    AppState(int initial_rows, int initial_cols)
        : automaton{initial_rows, initial_cols}
        , grid{automaton.grid()}
        , rows{initial_rows}
        , cols{initial_cols}
    {
        fps.reset();
    }

    ~AppState() {
        // La grilla es propiedad del autómata (RAII); no se libera aquí.

        // Cerrar socket si está abierto
        if (client_socket != NET_INVALID_SOCKET_VALUE) {
            net_close(client_socket);
        }
        
        // Liberar texturas
        for (auto& [name, tex] : patterns) {
            if (tex) SDL_DestroyTexture(tex);
        }
    }
    
    // No copy
    AppState(const AppState&) = delete;
    AppState& operator=(const AppState&) = delete;
};
