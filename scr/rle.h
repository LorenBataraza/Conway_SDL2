#pragma once

#include <string>
#include <vector>
#include <utility>

/**
 * Importador de patrones en formato RLE (Run Length Encoded), el estándar del
 * Juego de la Vida (conwaylife.com / LifeWiki).
 *
 * Devuelve las celdas vivas como offsets {dx, dy} = {columna, fila}, la misma
 * convención que usa PatternData::cells (ver load_pattern_into_grid en
 * grid.cpp). El origen es la esquina superior-izquierda de la bounding box.
 *
 * Formato soportado:
 *   - Líneas de comentario que empiezan con '#'.
 *   - Cabecera "x = m, y = n, rule = B3/S23" (se ignora; sólo delimita el cuerpo).
 *   - Cuerpo run-length: <n>b (muertas), <n>o (vivas), <n>$ (fin de fila),
 *     '!' (fin del patrón). El contador <n> por defecto es 1. Espacios y saltos
 *     de línea dentro del cuerpo se ignoran. Cualquier otra letra (estados de
 *     autómatas multi-estado) se trata como celda no-viva salvo 'o'.
 */
std::vector<std::pair<int, int>> parse_rle(const std::string& rle);

// Carga un archivo .rle desde disco y lo parsea. Devuelve vacío si no se puede
// abrir. (Para importar patrones nuevos a futuro sin recompilar los embebidos.)
std::vector<std::pair<int, int>> load_rle_file(const std::string& path);
