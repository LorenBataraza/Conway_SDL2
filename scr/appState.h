#pragma once

#include <iostream>
#include <stdio.h>
#include <vector>
#include <unordered_map>
#include <SDL2/SDL.h>

#include "grid.h"

struct Animation {
    SDL_Texture** frames = nullptr;
    Uint32* delays = nullptr;
    int frame_count = 0;
    int current_frame = 0;
    Uint32 last_update = 0;
    int width = 0;
    int height = 0;
    Uint32 total_duration = 0;
    int loop_count = 0;

    // 1. Constructor por defecto (inicializa todos los miembros)
    Animation() = default;

    // 2. Move constructor
    Animation(Animation&& other) noexcept 
        : frames(other.frames),
          delays(other.delays),
          frame_count(other.frame_count),
          current_frame(other.current_frame),
          last_update(other.last_update),
          width(other.width),
          height(other.height),
          total_duration(other.total_duration),
          loop_count(other.loop_count) {
        other.frames = nullptr;
        other.delays = nullptr;
        other.frame_count = 0;
    }

    // 3. Move assignment operator
    Animation& operator=(Animation&& other) noexcept {
        if (this != &other) {
            // Liberar recursos existentes
            free_resources();
            
            // Transferir ownership
            frames = other.frames;
            delays = other.delays;
            frame_count = other.frame_count;
            current_frame = other.current_frame;
            last_update = other.last_update;
            width = other.width;
            height = other.height;
            total_duration = other.total_duration;
            loop_count = other.loop_count;

            // Invalidar origen
            other.frames = nullptr;
            other.delays = nullptr;
            other.frame_count = 0;
        }
        return *this;
    }

    // 4. Eliminar copias (Rule of Five)
    Animation(const Animation&) = delete;
    Animation& operator=(const Animation&) = delete;

    // 5. Destructor
    ~Animation() {
        free_resources();
    }

private:
    // Helper para liberar recursos
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


// TODO: cambiar strings por enums de patter
enum patters{
	block=0,
	beehive,
	loaf,
	boat,
	flower,
    // Oscilators
	blinker,
	toad,
	beacon,
    // Spaceships
	glider,
	lwss
};



struct AppState {
    // Resorces
    std::unordered_map<std::string, SDL_Texture*> patterns;
	std::unordered_map<std::string, Animation> animations;
    
    // Grid 
    bool** grid;
    int rows, cols;

    //Singleplayer simulation
    bool run_sim=true;
    int frecuencia=1;
    std::string current_pattern="punto";
    
    // Multiplayer simulation
    bool multiplayer= false;
    int server_port = 6969;
    int server_ip=1; 

    // UI
    bool showParameters=true;
    bool showMultiplayerConf=false;
    bool showStructures=true;

    // Constructor
    AppState(int initial_rows, int initial_cols) 
    : rows{initial_rows},          // Inicialización uniforme (C++11)
      cols{initial_cols},
      grid{construct_grid(initial_rows, initial_cols)}{};

    // Destructor (si es necesario)
    ~AppState() {
        destructor_grid(grid, rows, cols);
    }
};
