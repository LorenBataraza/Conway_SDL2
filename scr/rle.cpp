#include "rle.h"

#include <cctype>
#include <fstream>
#include <sstream>

namespace {

// ¿La línea es la cabecera "x = m, y = n, rule = ..."?
bool is_header_line(const std::string& line) {
    for (char c : line) {
        if (std::isspace(static_cast<unsigned char>(c))) continue;
        return (c == 'x' || c == 'X');  // la cabecera empieza por 'x ='
    }
    return false;
}

}  // namespace

std::vector<std::pair<int, int>> parse_rle(const std::string& rle) {
    // 1) Quedarse sólo con el cuerpo: descartar comentarios (#...) y la cabecera.
    std::string body;
    std::istringstream in(rle);
    std::string line;
    bool header_skipped = false;
    while (std::getline(in, line)) {
        if (!line.empty() && line[0] == '#') continue;         // comentario
        if (!header_skipped && is_header_line(line)) {          // cabecera x=..
            header_skipped = true;
            continue;
        }
        body += line;
    }

    // 2) Decodificar el run-length del cuerpo.
    std::vector<std::pair<int, int>> cells;
    int x = 0, y = 0, count = 0;
    for (char c : body) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            count = count * 10 + (c - '0');
            continue;
        }
        const int n = (count > 0) ? count : 1;
        switch (c) {
            case 'o': case 'O':                 // celdas vivas
                for (int i = 0; i < n; ++i) cells.emplace_back(x + i, y);
                x += n;
                break;
            case 'b': case 'B': case '.':       // celdas muertas
                x += n;
                break;
            case '$':                           // fin de fila (n saltos)
                y += n;
                x = 0;
                break;
            case '!':                           // fin del patrón
                return cells;
            default:
                // espacios, saltos de línea u otros: se ignoran sin avanzar.
                continue;
        }
        count = 0;
    }
    return cells;
}

std::vector<std::pair<int, int>> load_rle_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return {};
    std::stringstream ss;
    ss << f.rdbuf();
    return parse_rle(ss.str());
}
