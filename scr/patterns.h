#pragma once

#include <vector>
#include <string>
#include <unordered_map>

/**
 * Enumeración de todos los patrones disponibles en el Juego de la Vida de Conway
 */
enum class Pattern : int {
    // Still Life (Vida estática)
    POINT = 0,
    BLOCK,
    BEEHIVE,
    LOAF,
    BOAT,
    FLOWER,
    
    // Oscillators (Osciladores)
    BLINKER,
    TOAD,
    BEACON,
    PULSAR,
    
    // Spaceships (Naves espaciales)
    GLIDER,
    LWSS,      // Lightweight Spaceship
    MWSS,      // Medium Spaceship
    HWSS,      // Heavy Spaceship
    
    // Guns (Cañones)
    GLIDER_GUN,
    
    // Valor para contar cantidad de patrones
    PATTERN_COUNT
};

/**
 * Estructura que contiene los datos de un patrón
 */
struct PatternData {
    Pattern id;
    std::string name;
    std::string display_name;
    std::vector<std::pair<int, int>> cells;
    int cost;  // Costo en puntos de consumo para modo competición
    
    int cell_count() const { return static_cast<int>(cells.size()); }
};

/**
 * Clase singleton que gestiona todos los patrones
 */
class PatternRegistry {
public:
    static PatternRegistry& instance() {
        static PatternRegistry registry;
        return registry;
    }

    const PatternData& get(Pattern pattern) const {
        return patterns_.at(static_cast<int>(pattern));
    }

    const PatternData& get(const std::string& name) const {
        auto it = name_to_pattern_.find(name);
        if (it != name_to_pattern_.end()) {
            return patterns_.at(static_cast<int>(it->second));
        }
        // Retorna POINT como fallback
        return patterns_.at(0);
    }

    bool exists(const std::string& name) const {
        return name_to_pattern_.find(name) != name_to_pattern_.end();
    }

    Pattern string_to_pattern(const std::string& name) const {
        auto it = name_to_pattern_.find(name);
        if (it != name_to_pattern_.end()) {
            return it->second;
        }
        return Pattern::POINT;
    }

    const std::string& pattern_to_string(Pattern pattern) const {
        return patterns_.at(static_cast<int>(pattern)).name;
    }

    const std::vector<PatternData>& all_patterns() const {
        return patterns_;
    }

    size_t count() const {
        return patterns_.size();
    }

private:
    PatternRegistry() {
        init_patterns();
        build_name_index();
    }

    void init_patterns() {
        // Formato: {id, name, display_name, cells, cost}
        // Costo basado aproximadamente en: celdas * complejidad
        patterns_ = {
            // Still Life (costo bajo - estables)
            {Pattern::POINT, "point", "Point", {{0,0}}, 1},
            
            {Pattern::BLOCK, "block", "Block", {{0,0}, {1,0}, {0,1}, {1,1}}, 4},
            
            {Pattern::BEEHIVE, "beehive", "Beehive", 
                {{1,0}, {2,0}, {0,1}, {3,1}, {1,2}, {2,2}}, 6},
            
            {Pattern::LOAF, "loaf", "Loaf", 
                {{1,0}, {2,0}, {0,1}, {3,1}, {1,2}, {3,2}, {2,3}}, 7},
            
            {Pattern::BOAT, "boat", "Boat", 
                {{0,0}, {1,0}, {0,1}, {2,1}, {1,2}}, 5},
            
            {Pattern::FLOWER, "flower", "Flower", 
                {{1,0}, {0,1}, {2,1}, {1,2}}, 4},

            // Oscillators (costo medio - generan actividad)
            {Pattern::BLINKER, "blinker", "Blinker", 
                {{0,0}, {1,0}, {2,0}}, 5},
            
            {Pattern::TOAD, "toad", "Toad", 
                {{1,0}, {2,0}, {3,0}, {0,1}, {1,1}, {2,1}}, 10},
            
            {Pattern::BEACON, "beacon", "Beacon", 
                {{0,0}, {1,0}, {0,1}, {3,2}, {2,3}, {3,3}}, 10},
            
            {Pattern::PULSAR, "pulsar", "Pulsar", {
                {2,0}, {3,0}, {4,0}, {8,0}, {9,0}, {10,0},
                {0,2}, {5,2}, {7,2}, {12,2},
                {0,3}, {5,3}, {7,3}, {12,3},
                {0,4}, {5,4}, {7,4}, {12,4},
                {2,5}, {3,5}, {4,5}, {8,5}, {9,5}, {10,5},
                {2,7}, {3,7}, {4,7}, {8,7}, {9,7}, {10,7},
                {0,8}, {5,8}, {7,8}, {12,8},
                {0,9}, {5,9}, {7,9}, {12,9},
                {0,10}, {5,10}, {7,10}, {12,10},
                {2,12}, {3,12}, {4,12}, {8,12}, {9,12}, {10,12}
            }, 50},

            // Spaceships (costo alto - invaden territorio)
            {Pattern::GLIDER, "glider", "Glider", 
                {{0,1}, {1,2}, {2,0}, {2,1}, {2,2}}, 15},
            
            {Pattern::LWSS, "lwss", "Lightweight Spaceship", 
                {{0,0}, {3,0}, {4,1}, {0,2}, {4,2}, {1,3}, {2,3}, {3,3}, {4,3}}, 25},
            
            {Pattern::MWSS, "mwss", "Medium Spaceship", 
                {{0,0}, {4,0}, {5,1}, {0,2}, {5,2}, {1,3}, {2,3}, {3,3}, {4,3}, {5,3}}, 35},
            
            {Pattern::HWSS, "hwss", "Heavy Spaceship", 
                {{0,0}, {5,0}, {6,1}, {0,2}, {6,2}, {1,3}, {2,3}, {3,3}, {4,3}, {5,3}, {6,3}}, 45},

            // Guns (costo muy alto - producen infinitas celdas)
            {Pattern::GLIDER_GUN, "glider_gun", "Gosper Glider Gun", {
                // Left square
                {0,4}, {0,5}, {1,4}, {1,5},
                // Right square
                {34,2}, {34,3}, {35,2}, {35,3},
                // Central structure
                {10,4}, {10,5}, {10,6},
                {11,3}, {11,7},
                {12,2}, {12,8},
                {13,2}, {13,8},
                {14,5},
                {15,3}, {15,7},
                {16,4}, {16,5}, {16,6},
                {17,5},
                // Bottom part
                {20,2}, {20,3}, {20,4},
                {21,2}, {21,3}, {21,4},
                {22,1}, {22,5},
                {24,0}, {24,1}, {24,5}, {24,6}
            }, 100}
        };
    }

    void build_name_index() {
        for (const auto& p : patterns_) {
            name_to_pattern_[p.name] = p.id;
        }
    }

    std::vector<PatternData> patterns_;
    std::unordered_map<std::string, Pattern> name_to_pattern_;
};

// Funciones de utilidad para compatibilidad con código existente
inline const std::vector<std::pair<int, int>>& get_pattern_cells(const std::string& name) {
    return PatternRegistry::instance().get(name).cells;
}

inline const std::vector<std::pair<int, int>>& get_pattern_cells(Pattern pattern) {
    return PatternRegistry::instance().get(pattern).cells;
}

inline bool pattern_exists(const std::string& name) {
    return PatternRegistry::instance().exists(name);
}

inline int get_pattern_cost(const std::string& name) {
    return PatternRegistry::instance().get(name).cost;
}

inline int get_pattern_cost(Pattern pattern) {
    return PatternRegistry::instance().get(pattern).cost;
}
