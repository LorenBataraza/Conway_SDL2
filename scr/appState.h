#pragma once

#include <unordered_map>
#include <string>
#include <netinet/in.h>

#include <SDL2/SDL.h>

#include "grid.h"
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
    
    // ============== Grilla ==============
    CellValue** grid;
    int rows, cols;

    // ============== Simulación Singleplayer ==============
    bool run_sim = true;
    int frecuencia = 10;  // Hz
    std::string current_pattern = "point";
    
    // ============== Multiplayer ==============
    bool multiplayer = false;
    int client_socket = -1;
    struct sockaddr_in server_addrs;
    std::string server_ip = "127.0.0.1";
    int server_port = 6969;
    
    // ID del jugador local (asignado por el servidor)
    CellValue player_id = 1;
    
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
    
    // Espejado de patrones
    bool mirror_horizontal = false;
    bool mirror_vertical = false;
    
    // Configuración del minimapa
    int minimap_size = 150;
    
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
        : rows{initial_rows}
        , cols{initial_cols}
        , grid{construct_grid(initial_rows, initial_cols)}
    {
        fps.reset();
    }

    ~AppState() {
        destructor_grid(grid, rows, cols);
        
        // Cerrar socket si está abierto
        if (client_socket >= 0) {
            close(client_socket);
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
