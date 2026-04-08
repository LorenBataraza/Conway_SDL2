# Conway's Game of Life - Multiplayer v0.4.0

Implementación del Juego de la Vida de Conway con soporte para múltiples jugadores simultáneos.

## Características

- **Servidor multi-cliente**: Hasta 10 clientes simultáneos con `poll()`
- **Colores por jugador**: Cada jugador tiene su color distintivo (8 colores)
- **Control remoto**: Los clientes pueden controlar la simulación (pausar, velocidad, etc.)
- **Doble cliente**: SDL2 gráfico + ncurses terminal
- **Minimapa**: Overlay cuando hay zoom activo

## Compilación

```bash
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON -DBUILD_NCURSES_VIEWER=ON
make -j$(nproc)
```

## Ejecutables

| Binario | Descripción |
|---------|-------------|
| `life_game` | Cliente gráfico SDL2 con ImGui |
| `conway_server` | Servidor multi-cliente |
| `ncurses_viewer` | Cliente terminal ncurses |
| `test_server` | Tests del servidor |

## Uso

```bash
# Terminal 1: Iniciar servidor
./conway_server -v

# Terminal 2: Cliente gráfico
./life_game

# Terminal 3: Cliente terminal
./ncurses_viewer

# Terminal 4: Otro cliente terminal
./ncurses_viewer
```

## Controles

### Cliente SDL2 (life_game)

| Tecla | Acción |
|-------|--------|
| Click | Colocar patrón |
| Scroll | Zoom in/out |
| M | Toggle minimapa |
| R | Reset zoom |
| C | Limpiar grilla |
| ESC | Salir |

### Cliente ncurses

| Tecla | Acción |
|-------|--------|
| Flechas | Mover cursor |
| WASD | Mover viewport |
| Space | Colocar patrón |
| 1-9, 0 | Seleccionar patrón |
| P | Pausar/Reanudar simulación |
| +/- | Aumentar/Disminuir frecuencia |
| N | Step manual (cuando pausado) |
| C | Limpiar grilla |
| H | Toggle ayuda |
| M | Toggle menú parámetros |
| R | Reconectar |
| Q | Salir |

## Protocolo de Comunicación

Ver [PROTOCOL.md](PROTOCOL.md) para documentación completa.

### Comandos Cliente → Servidor

```
ADD_PATTERN <nombre> <fila> <col> [player_id]
SET_FREQ <hz>
SET_RUN <0|1>
STEP
CLEAR
GET_CONFIG
```

### Colores por Jugador

| ID | Color |
|----|-------|
| 1 | Verde |
| 2 | Azul |
| 3 | Rojo |
| 4 | Amarillo |
| 5 | Magenta |
| 6 | Cyan |
| 7 | Naranja |
| 8 | Blanco |

## Patrones Disponibles

**Still Life**: point, block, beehive, loaf, boat, flower

**Oscillators**: blinker, toad, beacon, pulsar

**Spaceships**: glider, lwss, mwss, hwss

**Guns**: glider_gun

## Dependencias

```bash
# Ubuntu/Debian
sudo apt install libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev \
                 libncurses5-dev libgtest-dev

# Arch Linux
sudo pacman -S sdl2 sdl2_ttf sdl2_image ncurses gtest
```

## Arquitectura

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  life_game  │     │ncurses_view │     │  Cliente N  │
│   (SDL2)    │     │  (terminal) │     │             │
└──────┬──────┘     └──────┬──────┘     └──────┬──────┘
       │                   │                   │
       └───────────────────┼───────────────────┘
                           │ TCP
                    ┌──────┴──────┐
                    │conway_server│
                    │  (poll())   │
                    │             │
                    │  Grilla     │
                    │  100x100    │
                    └─────────────┘
```
