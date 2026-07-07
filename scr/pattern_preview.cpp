#include "pattern_preview.h"

#include <algorithm>

#include "grid.h"      // get_player_color, CELL_DEAD
#include "patterns.h"  // pattern_exists, get_pattern_cells

PatternPreview::PatternPreview(SDL_Renderer* renderer, const std::string& name,
                               int tex_size, bool animate)
    : renderer_(renderer), animate_(animate) {
    // Bounding box del patrón (dx -> columna, dy -> fila).
    int min_dx = 0, max_dx = 0, min_dy = 0, max_dy = 0;
    if (pattern_exists(name)) {
        const auto& cells = get_pattern_cells(name);
        bool first = true;
        for (const auto& [dx, dy] : cells) {
            if (first) { min_dx = max_dx = dx; min_dy = max_dy = dy; first = false; }
            else {
                min_dx = std::min(min_dx, dx); max_dx = std::max(max_dx, dx);
                min_dy = std::min(min_dy, dy); max_dy = std::max(max_dy, dy);
            }
        }
    }
    const int pw = max_dx - min_dx + 1;  // ancho (columnas)
    const int ph = max_dy - min_dy + 1;  // alto (filas)

    // Más margen si animamos (deja espacio para que naves/gliders se muevan).
    const int margin = animate_ ? 3 : 1;
    const int board_rows = std::max(1, ph + 2 * margin);
    const int board_cols = std::max(1, pw + 2 * margin);

    automaton_ = std::make_unique<Automaton>(board_rows, board_cols, &RULE_NORMAL);
    // Colocar la bbox del patrón en (margin, margin).
    automaton_->add_pattern(name, margin - min_dy, margin - min_dx, 1, false, false);

    // Guardar el estado inicial para el loop de animación.
    const Board& b = automaton_->board();
    initial_.assign(b.data(), b.data() + b.size());

    // Altura FIJA para todas las previews; el ancho sigue el aspecto del tablero.
    // Así la fila de íconos en "Estructuras" queda alineada (mismo alto), y los
    // patrones anchos (p. ej. el cañón) simplemente se ven más anchos.
    tex_h_ = tex_size;
    tex_w_ = std::max(8, tex_size * board_cols / board_rows);
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888,
                                 SDL_TEXTUREACCESS_TARGET, tex_w_, tex_h_);
    if (texture_) SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_BLEND);
    render_to_texture();
}

PatternPreview::~PatternPreview() {
    if (texture_) SDL_DestroyTexture(texture_);
}

void PatternPreview::render_to_texture() {
    if (!texture_) return;

    SDL_Texture* prev_target = SDL_GetRenderTarget(renderer_);
    SDL_SetRenderTarget(renderer_, texture_);

    // Fondo oscuro (como la grilla del juego).
    SDL_SetRenderDrawColor(renderer_, 20, 20, 25, 255);
    SDL_RenderClear(renderer_);

    const Board& b = automaton_->board();
    const float cw = static_cast<float>(tex_w_) / b.cols();
    const float ch = static_cast<float>(tex_h_) / b.rows();

    for (int r = 0; r < b.rows(); ++r) {
        for (int c = 0; c < b.cols(); ++c) {
            const CellValue v = b.at(r, c);
            if (v != CELL_DEAD) {
                SDL_Color col = get_player_color(v);
                SDL_SetRenderDrawColor(renderer_, col.r, col.g, col.b, 255);
                SDL_FRect rect{c * cw, r * ch, cw + 0.5f, ch + 0.5f};
                SDL_RenderFillRectF(renderer_, &rect);
            }
        }
    }

    // Cuadrícula de la simulación (igual que en el juego), dibujada por encima.
    // Sólo si las celdas son lo bastante grandes, para no saturar el ícono
    // (los patrones muy anchos como el cañón quedan sin líneas).
    if (cw > 5.0f && ch > 5.0f) {
        SDL_SetRenderDrawColor(renderer_, 44, 44, 52, 255);
        for (int c = 0; c <= b.cols(); ++c) {
            const float x = c * cw;
            SDL_RenderDrawLineF(renderer_, x, 0.0f, x, static_cast<float>(tex_h_));
        }
        for (int r = 0; r <= b.rows(); ++r) {
            const float y = r * ch;
            SDL_RenderDrawLineF(renderer_, 0.0f, y, static_cast<float>(tex_w_), y);
        }
    }

    SDL_SetRenderTarget(renderer_, prev_target);
}

SDL_Texture* PatternPreview::texture(Uint32 now_ms) {
    if (animate_) {
        if (last_step_ms_ == 0) last_step_ms_ = now_ms;
        if (now_ms - last_step_ms_ >= static_cast<Uint32>(period_ms_)) {
            last_step_ms_ = now_ms;
            if (++step_count_ > loop_steps_) {
                // Reiniciar el loop al estado inicial.
                Board& b = automaton_->board();
                std::copy(initial_.begin(), initial_.end(), b.data());
                automaton_->set_generation(0);
                automaton_->mark_all_dirty();
                step_count_ = 0;
            } else {
                automaton_->step();
            }
            render_to_texture();
        }
    }
    return texture_;
}
