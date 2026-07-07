#pragma once

#include <cstdint>
#include <vector>
#include <string>

#include "board.h"

/**
 * Ruleset — el "conjunto de punteros a funciones" que define la actualización
 * del autómata. `step_cell` decide el próximo valor de UNA celda leyendo el
 * tablero actual (de sólo lectura). Cada modo de juego es una instancia de
 * Ruleset, en vez de un enum + if/else.
 */
struct Ruleset {
    const char* name;
    CellValue (*step_cell)(const Board& current, int r, int c);
};

// Reglas concretas (definidas en automaton.cpp), compartidas por cliente y
// servidor para que el paso sea determinista en ambos lados.
extern const Ruleset RULE_NORMAL;
extern const Ruleset RULE_COMPETITION;

// Registro nombre -> ruleset (permite que la Wiki / modos futuros añadan reglas
// sin tocar el motor). Devuelve nullptr si no existe.
const Ruleset* find_ruleset(const std::string& name);

/**
 * Automaton — una grilla (Board) + un Ruleset que la actualiza.
 *
 * Optimizaciones:
 *  - Doble buffer persistente (current_/next_): cero reservas por paso.
 *  - Esquema de "dirty tiles" fijo (TILE x TILE): `step()` sólo procesa las
 *    celdas de tiles marcados como activos + su halo de 1 celda; el resto se
 *    conserva. El resultado es idéntico a un barrido completo (ver
 *    set_full_scan / tests de equivalencia), por lo que el paso es
 *    determinista — requisito del hash de sincronización.
 */
class Automaton {
public:
    static constexpr int TILE = 32;  // tamaño de tile (tunable)

    Automaton(int rows, int cols, const Ruleset* rules = &RULE_NORMAL);

    Board&       board()       { return current_; }
    const Board& board() const { return current_; }
    CellValue**  grid()        { return current_.rows_ptr(); }  // vista heredada
    int rows() const { return current_.rows(); }
    int cols() const { return current_.cols(); }

    void set_ruleset(const Ruleset* rules) { rules_ = rules; }
    const Ruleset* ruleset() const { return rules_; }

    uint64_t generation() const { return generation_; }
    void set_generation(uint64_t gen) { generation_ = gen; }

    // Avanza una generación aplicando el ruleset (double buffer + dirty tiles).
    void step();

    // Vacía la grilla (no toca la generación).
    void clear();

    // Marca regiones como "sucias" para que el próximo step() las reevalúe.
    // Necesario tras una edición externa (patrón, carga de grilla, click).
    void mark_all_dirty();
    void mark_region_dirty(int r0, int c0, int r1, int c1);

    // Coloca un patrón del registro y marca su región como sucia.
    void add_pattern(const std::string& name, int row, int col,
                     CellValue player_id, bool mirror_h = false, bool mirror_v = false);

    // Hash FNV-1a de 64 bits sobre el contenido de la grilla (verificación de
    // integridad entre cliente y servidor).
    uint64_t hash() const;

    // Modo de validación: fuerza barrido completo cada paso (para tests).
    void set_full_scan(bool on) { full_scan_ = on; }
    bool full_scan() const { return full_scan_; }

private:
    int tile_index(int tr, int tc) const { return tr * tile_cols_ + tc; }
    void mark_tile_and_neighbors(int tr, int tc, std::vector<uint8_t>& tiles);

    int tile_rows_, tile_cols_;
    Board current_;
    Board next_;
    const Ruleset* rules_;
    uint64_t generation_ = 0;
    bool full_scan_ = false;

    std::vector<uint8_t> dirty_;       // tiles a procesar este paso
    std::vector<uint8_t> next_dirty_;  // tiles a procesar el próximo paso
};
