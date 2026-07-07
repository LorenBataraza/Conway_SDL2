#pragma once

#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <memory>

#include "automaton.h"

/**
 * PatternPreview — render nativo de un patrón usando el propio autómata del
 * juego, en lugar de un GIF pre-renderizado.
 *
 * Corre un Automaton pequeño (Board del tamaño del patrón + margen) y lo dibuja
 * a una SDL_Texture (render target) que se expone como ImTextureID para ImGui.
 * Si `animate` es true, avanza la simulación en un temporizador y reinicia al
 * estado inicial cada cierto número de pasos (loop): así se ven osciladores y
 * naves "vivas" sin depender de assets externos.
 *
 * También sirve para diagramas de la Wiki (vecindarios) vía el mismo motor.
 */
class PatternPreview {
public:
    PatternPreview(SDL_Renderer* renderer, const std::string& pattern_name,
                   int tex_size = 64, bool animate = false);
    ~PatternPreview();

    PatternPreview(const PatternPreview&) = delete;
    PatternPreview& operator=(const PatternPreview&) = delete;

    // Avanza la animación (si corresponde) y devuelve la textura actual.
    SDL_Texture* texture(Uint32 now_ms);

    int width() const { return tex_w_; }
    int height() const { return tex_h_; }

private:
    void render_to_texture();

    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    std::unique_ptr<Automaton> automaton_;
    std::vector<CellValue> initial_;   // estado inicial (para el loop)

    int tex_w_ = 0, tex_h_ = 0;
    bool animate_ = false;
    int period_ms_ = 350;              // velocidad de animación
    int loop_steps_ = 12;              // pasos antes de reiniciar el loop
    int step_count_ = 0;
    Uint32 last_step_ms_ = 0;
};
