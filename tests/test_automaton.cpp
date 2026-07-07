#include <gtest/gtest.h>

#include <vector>
#include <string>
#include <algorithm>

#include "board.h"
#include "automaton.h"
#include "grid.h"
#include "rle.h"
#include "patterns.h"

// ============================================================================
// Tests unitarios del motor (Board / Automaton). No requieren servidor.
// ============================================================================

namespace {

int count_alive(const Board& b) {
    int n = 0;
    for (int r = 0; r < b.rows(); ++r)
        for (int c = 0; c < b.cols(); ++c)
            if (b.at(r, c) != CELL_DEAD) ++n;
    return n;
}

bool boards_equal(const Board& a, const Board& b) {
    if (a.rows() != b.rows() || a.cols() != b.cols()) return false;
    for (int r = 0; r < a.rows(); ++r)
        for (int c = 0; c < a.cols(); ++c)
            if (a.at(r, c) != b.at(r, c)) return false;
    return true;
}

}  // namespace

// ---------------------------------------------------------------------------
// Board
// ---------------------------------------------------------------------------

TEST(BoardTest, StartsEmpty) {
    Board b(200, 400);
    EXPECT_EQ(count_alive(b), 0);
}

TEST(BoardTest, RowPtrViewMatchesFlatBuffer) {
    Board b(50, 60);
    b.set(10, 20, 3);
    // La vista CellValue** debe reflejar el buffer plano.
    EXPECT_EQ(b.rows_ptr()[10][20], 3);
    b.rows_ptr()[11][21] = 5;
    EXPECT_EQ(b.at(11, 21), 5);
}

TEST(BoardTest, SwapIsContentCorrect) {
    Board a(10, 10), b(10, 10);
    a.set(1, 1, 7);
    b.set(2, 2, 9);
    a.swap(b);
    EXPECT_EQ(a.at(2, 2), 9);
    EXPECT_EQ(b.at(1, 1), 7);
    // Las tablas de filas siguen siendo consistentes tras el swap.
    EXPECT_EQ(a.rows_ptr()[2][2], 9);
    EXPECT_EQ(b.rows_ptr()[1][1], 7);
}

// ---------------------------------------------------------------------------
// Correctitud de reglas (Conway clásico)
// ---------------------------------------------------------------------------

TEST(AutomatonTest, BlockIsStillLife) {
    Automaton a(60, 60, &RULE_NORMAL);
    a.add_pattern("block", 30, 30, 1);
    int before = count_alive(a.board());
    for (int i = 0; i < 10; ++i) a.step();
    EXPECT_EQ(count_alive(a.board()), before);
}

TEST(AutomatonTest, BlinkerOscillatesPeriod2) {
    Automaton a(60, 60, &RULE_NORMAL);
    a.add_pattern("blinker", 30, 30, 1);
    uint64_t h0 = a.hash();
    a.step();
    uint64_t h1 = a.hash();
    a.step();
    uint64_t h2 = a.hash();
    EXPECT_NE(h0, h1);      // cambia de fase
    EXPECT_EQ(h0, h2);      // vuelve tras 2 pasos
}

TEST(AutomatonTest, GliderMovesAndKeepsFiveCells) {
    Automaton a(80, 80, &RULE_NORMAL);
    a.add_pattern("glider", 10, 10, 1);
    EXPECT_EQ(count_alive(a.board()), 5);
    for (int i = 0; i < 4; ++i) a.step();  // periodo del glider
    EXPECT_EQ(count_alive(a.board()), 5);  // sigue vivo tras un ciclo
}

// ---------------------------------------------------------------------------
// Patrones nuevos (verifica que las coordenadas encodeadas son correctas)
// ---------------------------------------------------------------------------

TEST(NewPatternsTest, ExtraStillLifesAreStable) {
    for (const char* name : {"tub", "pond", "ship"}) {
        Automaton a(60, 60, &RULE_NORMAL);
        a.add_pattern(name, 25, 25, 1);
        const uint64_t h0 = a.hash();
        for (int i = 0; i < 8; ++i) a.step();
        EXPECT_EQ(a.hash(), h0) << "'" << name << "' no es estable (still life)";
    }
}

TEST(NewPatternsTest, PentadecathlonPeriod15) {
    Automaton a(80, 80, &RULE_NORMAL);
    a.add_pattern("pentadecathlon", 40, 40, 1);
    const uint64_t h0 = a.hash();
    for (int i = 0; i < 15; ++i) a.step();
    EXPECT_EQ(a.hash(), h0) << "pentadecathlon no vuelve a su estado tras 15 pasos";
}

TEST(NewPatternsTest, DiehardVanishesAtGen130) {
    Automaton a(120, 120, &RULE_NORMAL);
    a.add_pattern("diehard", 60, 60, 1);
    for (int i = 0; i < 129; ++i) a.step();
    EXPECT_GT(count_alive(a.board()), 0) << "diehard murió antes de la generación 130";
    a.step();  // generación 130
    EXPECT_EQ(count_alive(a.board()), 0) << "diehard debería desaparecer en la generación 130";
}

TEST(NewPatternsTest, MethuselahsGrow) {
    for (const char* name : {"r_pentomino", "acorn"}) {
        Automaton a(200, 200, &RULE_NORMAL);
        a.add_pattern(name, 100, 100, 1);
        const int start = count_alive(a.board());
        for (int i = 0; i < 60; ++i) a.step();
        EXPECT_GT(count_alive(a.board()), start) << "'" << name << "' no creció como methuselah";
    }
}

// ---------------------------------------------------------------------------
// Optimización: dirty-tiles == barrido completo
// ---------------------------------------------------------------------------

TEST(AutomatonTest, DirtyTilesEqualsFullScan) {
    const std::vector<std::pair<std::string, std::pair<int, int>>> setups = {
        {"glider", {5, 5}},
        {"pulsar", {40, 40}},
        {"glider_gun", {5, 5}},
        {"lwss", {20, 80}},
    };

    for (const auto& [name, pos] : setups) {
        Automaton opt(200, 400, &RULE_NORMAL);   // dirty-tiles (por defecto)
        Automaton full(200, 400, &RULE_NORMAL);  // barrido completo
        full.set_full_scan(true);

        opt.add_pattern(name, pos.first, pos.second, 1);
        full.add_pattern(name, pos.first, pos.second, 1);

        for (int i = 0; i < 60; ++i) {
            opt.step();
            full.step();
            ASSERT_TRUE(boards_equal(opt.board(), full.board()))
                << "Divergencia con patrón '" << name << "' en el paso " << i;
            ASSERT_EQ(opt.hash(), full.hash())
                << "Hash distinto con patrón '" << name << "' en el paso " << i;
        }
    }
}

TEST(AutomatonTest, CompetitionModeDirtyTilesEqualsFullScan) {
    Automaton opt(200, 400, &RULE_COMPETITION);
    Automaton full(200, 400, &RULE_COMPETITION);
    full.set_full_scan(true);

    // Dos jugadores enfrentados.
    opt.add_pattern("glider", 20, 20, 1);
    opt.add_pattern("glider", 25, 40, 2);
    full.add_pattern("glider", 20, 20, 1);
    full.add_pattern("glider", 25, 40, 2);

    for (int i = 0; i < 60; ++i) {
        opt.step();
        full.step();
        ASSERT_EQ(opt.hash(), full.hash()) << "COMPETITION divergió en el paso " << i;
    }
}

// ---------------------------------------------------------------------------
// Determinismo (requisito del lockstep): mismos inputs => misma secuencia de hash
// ---------------------------------------------------------------------------

TEST(AutomatonTest, DeterministicHashSequence) {
    auto run = []() {
        Automaton a(200, 400, &RULE_NORMAL);
        a.add_pattern("glider_gun", 5, 5, 1);
        std::vector<uint64_t> hashes;
        for (int i = 0; i < 50; ++i) {
            a.step();
            hashes.push_back(a.hash());
        }
        return hashes;
    };

    EXPECT_EQ(run(), run());
}

// ---------------------------------------------------------------------------
// Importador RLE
// ---------------------------------------------------------------------------

namespace {
std::vector<std::pair<int,int>> sorted_cells(std::vector<std::pair<int,int>> v) {
    std::sort(v.begin(), v.end());
    return v;
}
}  // namespace

TEST(RleParserTest, GliderExact) {
    // Glider en RLE, con cabecera y comentario.
    auto cells = parse_rle("#N glider\nx = 3, y = 3, rule = B3/S23\nbob$2bo$3o!");
    std::vector<std::pair<int,int>> expected = {{1,0},{2,1},{0,2},{1,2},{2,2}};
    EXPECT_EQ(sorted_cells(cells), sorted_cells(expected));
}

TEST(RleParserTest, BlinkerExact) {
    EXPECT_EQ(sorted_cells(parse_rle("3o!")),
              sorted_cells({{0,0},{1,0},{2,0}}));
}

TEST(RleParserTest, MultiNewlineRunsAdvanceRows) {
    // 'o', saltar 2 filas ($ con contador 3), 'o' en la fila 3.
    EXPECT_EQ(sorted_cells(parse_rle("o3$o!")),
              sorted_cells({{0,0},{0,3}}));
}

TEST(ImportedPatternsTest, AllParseNonEmpty) {
    // Los patrones grandes se registran vía parse_rle en el PatternRegistry.
    for (const char* n : {"puffer", "conduit", "agar", "wick"}) {
        EXPECT_TRUE(pattern_exists(n)) << n << " no está registrado";
        EXPECT_GT(static_cast<int>(get_pattern_cells(n).size()), 0)
            << n << " parseó vacío";
    }
}

TEST(ImportedPatternsTest, SpacefillerAgarGrows) {
    Automaton a(400, 400, &RULE_NORMAL);
    a.add_pattern("agar", 200, 200, 1);
    const int start = count_alive(a.board());
    for (int i = 0; i < 40; ++i) a.step();
    EXPECT_GT(count_alive(a.board()), start * 2) << "el spacefiller (agar) debería crecer";
}

TEST(ImportedPatternsTest, PufferSurvivesAndLeavesTrail) {
    Automaton a(300, 300, &RULE_NORMAL);
    a.add_pattern("puffer", 150, 150, 1);
    const int start = count_alive(a.board());
    for (int i = 0; i < 50; ++i) a.step();
    EXPECT_GT(count_alive(a.board()), 0) << "el puffer no debería autodestruirse";
    EXPECT_GT(count_alive(a.board()), start) << "el puffer debería dejar rastro (crecer)";
}

TEST(ImportedPatternsTest, ConduitRacetrackStaysAlive) {
    Automaton a(400, 400, &RULE_NORMAL);
    a.add_pattern("conduit", 100, 40, 1);
    EXPECT_GT(static_cast<int>(get_pattern_cells("conduit").size()), 100);
    for (int i = 0; i < 60; ++i) a.step();
    EXPECT_GT(count_alive(a.board()), 0) << "el racetrack es un oscilador; no debería morir";
}

TEST(ImportedPatternsTest, WickIsActiveAndBounded) {
    Automaton a(400, 400, &RULE_NORMAL);
    a.add_pattern("wick", 130, 130, 1);
    const int start = count_alive(a.board());
    EXPECT_GT(start, 0);
    for (int i = 0; i < 60; ++i) a.step();
    const int now = count_alive(a.board());
    EXPECT_NE(now, start) << "la mecha (fuse) debería estar activa (arder/cambiar)";
    EXPECT_LT(now, start * 5) << "debería permanecer acotada (no explotar)";
}
