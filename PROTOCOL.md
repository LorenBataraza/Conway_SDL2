# Protocolo de Comunicación - Conway's Game of Life

## Comandos del Cliente → Servidor

### Patrones
```
ADD_PATTERN <nombre> <fila> <columna> [player_id]
```
Coloca un patrón en la grilla.
- `nombre`: point, block, beehive, loaf, boat, flower, blinker, toad, beacon, pulsar, glider, lwss, mwss, hwss, glider_gun
- `fila`, `columna`: coordenadas (0-99)
- `player_id`: opcional, ID del jugador (1-8)

### Control de Simulación
```
SET_RUN <0|1>
```
Pausa (0) o reanuda (1) la simulación.

```
SET_FREQ <hz>
```
Cambia la frecuencia de simulación (1-60 Hz).

```
STEP
```
Avanza un paso (solo funciona cuando está pausado).

```
CLEAR
```
Limpia toda la grilla.

### Configuración
```
GET_CONFIG
```
Solicita la configuración actual del servidor.

---

## Mensajes del Servidor → Cliente

### Actualización de Grilla
```
GRID_UPDATE
<fila> <columna> <estado>
<fila> <columna> <estado>
...
END
```
- `estado`: 0 = celda muerta, 1-8 = ID del jugador que la creó

### Actualización de Configuración
```
CONFIG_UPDATE
FREQ <hz>
RUN <0|1>
PLAYER_ID <id>
CLIENTS <count>
END
```

---

## Colores por Jugador

| Player ID | Color     |
|-----------|-----------|
| 1         | Verde     |
| 2         | Azul      |
| 3         | Rojo      |
| 4         | Amarillo  |
| 5         | Magenta   |
| 6         | Cyan      |
| 7         | Naranja   |
| 8         | Blanco    |

---

## Controles del Visor ncurses

| Tecla     | Acción                    |
|-----------|---------------------------|
| Flechas   | Mover cursor              |
| WASD      | Mover viewport            |
| Space     | Colocar patrón            |
| 1-9, 0    | Seleccionar patrón        |
| P         | Pausar/Reanudar           |
| +/-       | Aumentar/Disminuir freq   |
| N         | Step (cuando pausado)     |
| C         | Limpiar grilla            |
| H         | Toggle ayuda              |
| M         | Toggle menú parámetros    |
| R         | Reconectar                |
| Q         | Salir                     |
