#include "connection_history.h"

#include <cstdlib>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

std::string connection_history_path() {
    // En Linux/macOS: $HOME; en Windows: %USERPROFILE% (HOME suele no existir).
    const char* home = std::getenv("HOME");
    if (!home) home = std::getenv("USERPROFILE");
    std::string base = home ? std::string(home) : std::string(".");
    return base + "/.config/conway_life/connections.txt";
}

std::vector<ConnectionEntry> load_connection_history() {
    std::vector<ConnectionEntry> entries;
    std::ifstream file(connection_history_path());
    if (!file.is_open()) return entries;

    std::string ip;
    int port;
    while (file >> ip >> port) {
        if (!ip.empty() && port > 0 && port <= 65535) {
            entries.push_back({ip, port});
        }
    }
    return entries;
}

void save_connection_history(const std::vector<ConnectionEntry>& entries) {
    const std::string path = connection_history_path();
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);

    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) return;
    for (const auto& e : entries) {
        file << e.ip << " " << e.port << "\n";
    }
}

void remember_connection(std::vector<ConnectionEntry>& entries,
                         const std::string& ip, int port, int max_entries) {
    // Quitar duplicados de {ip,port}.
    for (auto it = entries.begin(); it != entries.end();) {
        if (it->ip == ip && it->port == port) it = entries.erase(it);
        else ++it;
    }
    // Insertar al frente (más reciente).
    entries.insert(entries.begin(), {ip, port});
    // Recortar.
    if (static_cast<int>(entries.size()) > max_entries) {
        entries.resize(max_entries);
    }
    save_connection_history(entries);
}
