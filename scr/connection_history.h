#pragma once

#include <string>
#include <vector>

/**
 * Historial de conexiones multiplayer previas, persistido en
 * ~/.config/conway_life/connections.txt (una línea "ip puerto" por entrada,
 * más reciente primero).
 */
struct ConnectionEntry {
    std::string ip;
    int port = 6969;
};

// Ruta del archivo de historial.
std::string connection_history_path();

// Carga el historial (más reciente primero). Vacío si no existe.
std::vector<ConnectionEntry> load_connection_history();

// Persiste el historial.
void save_connection_history(const std::vector<ConnectionEntry>& entries);

// Mueve/inserta {ip,port} al frente (dedup), recorta a `max_entries` y guarda.
void remember_connection(std::vector<ConnectionEntry>& entries,
                         const std::string& ip, int port, int max_entries = 8);
