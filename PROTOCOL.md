# Protocolo de Comunicación - Conway's Game of Life v0.5

## Modos de Juego

| Modo | Descripción |
|------|-------------|
| `NORMAL` | Conway clásico. Celdas siguen reglas estándar. |
| `COMPETITION` | Celdas enemigas restan vecinos. Sistema de puntos. |

---

## Comandos Cliente → Servidor

### Patrones
```
ADD_PATTERN <nombre> <fila> <columna> [player_id]
```
Coloca un patrón. En modo COMPETITION consume puntos según el costo del patrón.

**Respuesta de error** (si no hay suficientes puntos):
```
ERROR NOT_ENOUGH_POINTS
```

### Control de Simulación
```
SET_RUN <0|1>        # Pausar (0) o reanudar (1)
SET_FREQ <hz>        # Frecuencia 1-60 Hz
STEP                 # Avanzar un paso (solo pausado)
CLEAR                # Limpiar grilla
RESET                # Reiniciar partida (puntos y grilla)
```

### Modos de Juego
```
SET_MODE <NORMAL|COMPETITION>
```
Cambia el modo de juego y reinicia la partida.

### Logging
```
SET_LOG <0|1>        # Desactivar (0) o activar (1) logging
```

### Consultas
```
GET_CONFIG           # Solicita configuración actual
GET_SCORES           # Solicita puntuaciones (modo COMPETITION)
```

---

## Mensajes Servidor → Cliente

### Actualización de Grilla
```
GRID_UPDATE
<fila> <columna> <estado>
...
END
```
`estado`: 0 = muerta, 1-8 = ID del jugador

### Configuración
```
CONFIG_UPDATE
FREQ <hz>
RUN <0|1>
MODE <NORMAL|COMPETITION>
PLAYER_ID <id>
CLIENTS <count>
VICTORY_GOAL <puntos>
END
```

### Estado del Jugador
```
PLAYER_STATE
PLAYER_ID <id>
VICTORY <puntos>
CONSUMPTION <puntos>
CELLS <cantidad>
END
```

### Puntuaciones (broadcast cada segundo en COMPETITION)
```
SCORES
<player_id> <victory> <consumption> <cells>
<player_id> <victory> <consumption> <cells>
...
END
```

### Ganador
```
WINNER <player_id>
```

---

## Sistema de Puntos (Modo COMPETITION)

### Consumo
- Cada jugador inicia con **200 puntos de consumo**
- Colocar patrones resta puntos según el costo
- Regeneración: **+2 puntos/tick**

### Victoria
- Ganas **+1 punto por cada 10 celdas vivas** tuyas por tick
- Meta: **1000 puntos** para ganar

### Costos de Patrones

| Patrón | Costo | Celdas |
|--------|-------|--------|
| point | 1 | 1 |
| block | 4 | 4 |
| beehive | 6 | 6 |
| loaf | 7 | 7 |
| boat | 5 | 5 |
| flower | 4 | 4 |
| blinker | 5 | 3 |
| toad | 10 | 6 |
| beacon | 10 | 6 |
| pulsar | 50 | 48 |
| glider | 15 | 5 |
| lwss | 25 | 9 |
| mwss | 35 | 10 |
| hwss | 45 | 11 |
| glider_gun | 100 | 36 |

---

## Mecánica de Competición

En modo `COMPETITION`, los vecinos se calculan diferente:

- **Vecino aliado** (mismo jugador): cuenta **+1**
- **Vecino enemigo** (otro jugador): cuenta **-1**

Esto significa que una celda rodeada de enemigos muere más fácilmente, y las células necesitan apoyo de aliados para sobrevivir.

### Reglas de Supervivencia
- Celda viva necesita **2-3 vecinos efectivos** para sobrevivir
- Celda nace con **exactamente 3 vecinos totales** (como Conway)
- La nueva celda hereda el color del jugador dominante entre los vecinos

---

## Colores por Jugador

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

---

## Ejemplo de Sesión

```
# Cliente conecta y pide config
-> GET_CONFIG

<- CONFIG_UPDATE
   FREQ 10
   RUN 1
   MODE COMPETITION
   PLAYER_ID 2
   CLIENTS 2
   VICTORY_GOAL 1000
   END

<- PLAYER_STATE
   PLAYER_ID 2
   VICTORY 0
   CONSUMPTION 200
   CELLS 0
   END

# Cliente coloca un glider (costo 15)
-> ADD_PATTERN glider 50 50

# Si no tiene puntos suficientes:
<- ERROR NOT_ENOUGH_POINTS

# Servidor envía actualizaciones de grilla
<- GRID_UPDATE
   50 51 2
   51 52 2
   52 50 2
   52 51 2
   52 52 2
   END

# Cada segundo, broadcast de scores
<- SCORES
   1 45 180 120
   2 32 165 85
   END

# Alguien gana
<- WINNER 1
```
