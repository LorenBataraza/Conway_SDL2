# Conway's Game of Life - Actualizaciones v0.3.0

## Resumen de Cambios

### Bugs Corregidos en `server.cpp`

1. **Bug crítico en `broadcast_update()`:**
```cpp
// ANTES (incorrecto - comparación, no asigna nada)
state.previous_grid[i][j]!=state.current_grid[i][j];

// DESPUÉS (correcto - asignación)
state.previous_grid[i][j] = state.current_grid[i][j];
```

2. **Falta de estado en GRID_UPDATE:**
```cpp
// ANTES (solo fila y columna)
ss << i << " " << j << "\n";

// DESPUÉS (fila, columna y estado)
ss << i << " " << j << " " << (state.current_grid[i][j] ? 1 : 0) << "\n";
```

---

### 1. Sistema de Patterns con Enumeración (`scr/patterns.h`)

```cpp
#include "patterns.h"

// Con enum (type-safe)
Pattern p = Pattern::GLIDER;
const auto& cells = get_pattern_cells(p);

// Con string (compatibilidad)
const auto& cells = get_pattern_cells("glider");

// Verificar existencia
if (pattern_exists("glider")) { ... }

// Acceso al registro
auto& registry = PatternRegistry::instance();
for (const auto& pattern : registry.all_patterns()) {
    std::cout << pattern.display_name << "\n";
}
```

**Patrones disponibles:**
- Still Life: `POINT`, `BLOCK`, `BEEHIVE`, `LOAF`, `BOAT`, `FLOWER`
- Oscillators: `BLINKER`, `TOAD`, `BEACON`, `PULSAR`
- Spaceships: `GLIDER`, `LWSS`, `MWSS`, `HWSS`
- Guns: `GLIDER_GUN`

---

### 2. Sistema de Logging (`scr/packet_logger.h`)

```cpp
#include "packet_logger.h"

PacketLogger logger("server.log", EndpointType::SERVER, true);

logger.log(PacketDirection::OUTGOING, "GRID_UPDATE\n5 10 1\nEND");
logger.log(PacketDirection::INCOMING, "ADD_PATTERN glider 10 20");
logger.log_event("CLIENT_CONNECTED", "192.168.1.100:54321");
logger.log_error("recv() failed", errno);
```

---

### 3. Visor ncurses (`scr/ncurses_viewer.cpp`)

| Tecla | Acción |
|-------|--------|
| Flechas | Mover cursor |
| W/A/S/D | Mover viewport |
| Espacio | Colocar patrón |
| 1-9 | Seleccionar patrón |
| P | Pausar/reanudar |
| R | Reconectar |
| Q/ESC | Salir |

---

### 4. Tests con Google Test (`tests/test_server.cpp`)

**Tests incluidos:**
- Conexión básica y reconexión
- Envío de patrones: glider, block, blinker, toad, beacon, pulsar, lwss, glider_gun
- Patrones inválidos y mensajes malformados
- Formato de respuesta GRID_UPDATE
- Coordenadas límite (0,0), (99,99), negativas, fuera de rango
- Múltiples patrones y ráfaga rápida
- Stress test: 100 y 500 mensajes

---

## Compilación

```bash
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON -DBUILD_NCURSES_VIEWER=ON
make -j$(nproc)
```

---

## Ejecución

```bash
# Servidor (en una terminal)
./conway_server -v

# Tests (en otra terminal, servidor corriendo)
./test_server

# Cliente ncurses
./ncurses_viewer
```

---

## Dependencias

```bash
# Ubuntu/Debian
sudo apt install libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev \
                 libncurses5-dev libgtest-dev

# Arch Linux
sudo pacman -S sdl2 sdl2_ttf sdl2_image ncurses gtest
```

---

## Estructura de Archivos

```
scr/
├── patterns.h         # Enum de patrones + PatternRegistry
├── packet_logger.h    # Logger de paquetes
├── server.cpp         # Servidor corregido
├── ncurses_viewer.cpp # Cliente terminal
├── grid.cpp/.h        # (existente)
└── ...

tests/
└── test_server.cpp    # Tests Google Test

CMakeLists.txt
```

---

## Protocolo

**Cliente → Servidor:**
```
ADD_PATTERN <nombre> <fila> <columna>
```

**Servidor → Cliente:**
```
GRID_UPDATE
<fila1> <col1> <estado1>
<fila2> <col2> <estado2>
...
END
```
