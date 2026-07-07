#include "automaton.h"

#include <algorithm>
#include <cstring>

#include "grid.h"      // load_pattern_into_grid, NUM_PLAYER_COLORS, CELL_DEAD
#include "patterns.h"  // PatternRegistry, pattern_exists, get_pattern_cells

// ==================== REGLAS (punteros a función) ====================
//
// Estas funciones son la ÚNICA implementación de las reglas: antes estaban
// duplicadas en grid.cpp (update_grid) y en server.cpp (update_grid_with_mode).

namespace {

// Modo NORMAL: Conway clásico. Nace con 3 vecinos (hereda el color del jugador
// dominante entre los vecinos); sobrevive con 2 o 3.
CellValue rule_normal_cell(const Board& b, int r, int c) {
    const int rows = b.rows();
    const int cols = b.cols();
    const CellValue cur = b.at(r, c);

    int count = 0;
    int player_count[NUM_PLAYER_COLORS + 1] = {0};
    for (int di = -1; di <= 1; ++di) {
        for (int dj = -1; dj <= 1; ++dj) {
            if (di == 0 && dj == 0) continue;
            const int ni = r + di;
            const int nj = c + dj;
            if (ni < 0 || ni >= rows || nj < 0 || nj >= cols) continue;
            const CellValue v = b.at(ni, nj);
            if (v != CELL_DEAD) {
                ++count;
                if (v > 0 && v <= NUM_PLAYER_COLORS) ++player_count[v];
            }
        }
    }

    if (cur != CELL_DEAD) {
        return (count == 2 || count == 3) ? cur : CELL_DEAD;
    }
    if (count == 3) {
        CellValue dominant = 1;
        int max_count = 0;
        for (int p = 1; p <= NUM_PLAYER_COLORS; ++p) {
            if (player_count[p] > max_count) {
                max_count = player_count[p];
                dominant = static_cast<CellValue>(p);
            }
        }
        return dominant;
    }
    return CELL_DEAD;
}

// Modo COMPETITION: las celdas vivas cuentan aliados (+1) y enemigos (-1); una
// celda muerta nace con 3 vecinos totales (color dominante).
CellValue rule_competition_cell(const Board& b, int r, int c) {
    const int rows = b.rows();
    const int cols = b.cols();
    const CellValue cur = b.at(r, c);

    if (cur != CELL_DEAD) {
        int effective = 0;
        for (int di = -1; di <= 1; ++di) {
            for (int dj = -1; dj <= 1; ++dj) {
                if (di == 0 && dj == 0) continue;
                const int ni = r + di;
                const int nj = c + dj;
                if (ni < 0 || ni >= rows || nj < 0 || nj >= cols) continue;
                const CellValue v = b.at(ni, nj);
                if (v != CELL_DEAD) effective += (v == cur) ? 1 : -1;
            }
        }
        return (effective < 2 || effective > 3) ? CELL_DEAD : cur;
    }

    int total = 0;
    int player_count[NUM_PLAYER_COLORS + 1] = {0};
    for (int di = -1; di <= 1; ++di) {
        for (int dj = -1; dj <= 1; ++dj) {
            if (di == 0 && dj == 0) continue;
            const int ni = r + di;
            const int nj = c + dj;
            if (ni < 0 || ni >= rows || nj < 0 || nj >= cols) continue;
            const CellValue v = b.at(ni, nj);
            if (v != CELL_DEAD) {
                ++total;
                if (v > 0 && v <= NUM_PLAYER_COLORS) ++player_count[v];
            }
        }
    }
    if (total == 3) {
        CellValue dominant = 1;
        int max_count = 0;
        for (int p = 1; p <= NUM_PLAYER_COLORS; ++p) {
            if (player_count[p] > max_count) {
                max_count = player_count[p];
                dominant = static_cast<CellValue>(p);
            }
        }
        return dominant;
    }
    return CELL_DEAD;
}

}  // namespace

const Ruleset RULE_NORMAL      = {"NORMAL", rule_normal_cell};
const Ruleset RULE_COMPETITION = {"COMPETITION", rule_competition_cell};

const Ruleset* find_ruleset(const std::string& name) {
    if (name == "COMPETITION" || name == "1") return &RULE_COMPETITION;
    if (name == "NORMAL" || name == "0")      return &RULE_NORMAL;
    return nullptr;
}

// ==================== AUTOMATON ====================

Automaton::Automaton(int rows, int cols, const Ruleset* rules)
    : tile_rows_{(rows + TILE - 1) / TILE}
    , tile_cols_{(cols + TILE - 1) / TILE}
    , current_{rows, cols}
    , next_{rows, cols}
    , rules_{rules}
    , dirty_(static_cast<size_t>(tile_rows_) * tile_cols_, 0)
    , next_dirty_(static_cast<size_t>(tile_rows_) * tile_cols_, 0)
{}

void Automaton::mark_tile_and_neighbors(int tr, int tc, std::vector<uint8_t>& tiles) {
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            const int ntr = tr + dr;
            const int ntc = tc + dc;
            if (ntr < 0 || ntr >= tile_rows_ || ntc < 0 || ntc >= tile_cols_) continue;
            tiles[tile_index(ntr, ntc)] = 1;
        }
    }
}

void Automaton::mark_all_dirty() {
    std::fill(dirty_.begin(), dirty_.end(), 1);
}

void Automaton::mark_region_dirty(int r0, int c0, int r1, int c1) {
    const int rows = current_.rows();
    const int cols = current_.cols();
    r0 = std::max(0, r0);           c0 = std::max(0, c0);
    r1 = std::min(rows - 1, r1);    c1 = std::min(cols - 1, c1);
    if (r0 > r1 || c0 > c1) return;
    const int tr0 = r0 / TILE, tr1 = r1 / TILE;
    const int tc0 = c0 / TILE, tc1 = c1 / TILE;
    for (int tr = tr0; tr <= tr1; ++tr) {
        for (int tc = tc0; tc <= tc1; ++tc) {
            mark_tile_and_neighbors(tr, tc, dirty_);
        }
    }
}

void Automaton::step() {
    const int rows = current_.rows();
    const int cols = current_.cols();

    // next_ arranca como copia de current_: las celdas en tiles limpios se
    // conservan tal cual (barrido completo equivalente).
    next_.copy_from(current_);
    std::fill(next_dirty_.begin(), next_dirty_.end(), 0);

    auto process_tile = [&](int tr, int tc) {
        const int r0 = tr * TILE, r1 = std::min(r0 + TILE, rows);
        const int c0 = tc * TILE, c1 = std::min(c0 + TILE, cols);
        bool changed = false;
        for (int r = r0; r < r1; ++r) {
            for (int c = c0; c < c1; ++c) {
                const CellValue nv = rules_->step_cell(current_, r, c);
                if (nv != current_.at(r, c)) {
                    next_.set(r, c, nv);
                    changed = true;
                }
            }
        }
        // Si algo cambió en este tile, sus celdas (y las de tiles vecinos, por
        // el halo de 1 celda) deben reevaluarse el próximo paso.
        if (changed) mark_tile_and_neighbors(tr, tc, next_dirty_);
    };

    for (int tr = 0; tr < tile_rows_; ++tr) {
        for (int tc = 0; tc < tile_cols_; ++tc) {
            if (full_scan_ || dirty_[tile_index(tr, tc)]) {
                process_tile(tr, tc);
            }
        }
    }

    // Copiar el resultado de vuelta a current_ (en lugar de intercambiar los
    // buffers) para que el puntero de la grilla actual sea ESTABLE: el código
    // heredado que cachea `automaton.grid()` (CellValue**) sigue siendo válido
    // tras cada paso. El memcpy extra (~80 KB) es despreciable a estas tasas.
    current_.copy_from(next_);
    dirty_.swap(next_dirty_);
    ++generation_;
}

void Automaton::clear() {
    current_.clear();
    std::fill(dirty_.begin(), dirty_.end(), 0);
}

void Automaton::add_pattern(const std::string& name, int row, int col,
                            CellValue player_id, bool mirror_h, bool mirror_v) {
    if (!pattern_exists(name)) return;

    load_pattern_into_grid(name, current_.rows_ptr(), row, col,
                           current_.rows(), current_.cols(),
                           player_id, mirror_h, mirror_v);

    // Marcar la región del patrón (bounding box) como sucia. dx -> columna,
    // dy -> fila (ver load_pattern_into_grid).
    const auto& cells = get_pattern_cells(name);
    int min_dx = 0, max_dx = 0, min_dy = 0, max_dy = 0;
    for (const auto& [dx, dy] : cells) {
        min_dx = std::min(min_dx, dx); max_dx = std::max(max_dx, dx);
        min_dy = std::min(min_dy, dy); max_dy = std::max(max_dy, dy);
    }
    mark_region_dirty(row + min_dy, col + min_dx, row + max_dy, col + max_dx);
}

uint64_t Automaton::hash() const {
    // FNV-1a de 64 bits sobre el buffer de celdas.
    uint64_t h = 1469598103934665603ULL;
    const CellValue* d = current_.data();
    const int n = current_.size();
    for (int i = 0; i < n; ++i) {
        h ^= static_cast<uint8_t>(d[i]);
        h *= 1099511628211ULL;
    }
    return h;
}
