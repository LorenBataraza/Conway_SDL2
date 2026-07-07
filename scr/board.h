#pragma once

#include <memory>
#include <cstring>
#include <utility>

#include "grid.h"  // CellValue, CELL_DEAD

/**
 * Board — estado puro de la grilla (sin reglas de actualización).
 *
 * La grilla es de tamaño FIJO, así que se almacena en un único bloque
 * contiguo (row-major) de largo `rows*cols`, reservado una sola vez con
 * `std::unique_ptr<CellValue[]>` — sin la sobrecarga de capacidad/resize de
 * std::vector, y con localidad de caché.
 *
 * Se mantiene además una tabla de punteros a filas (`CellValue**`) que apunta
 * dentro del bloque contiguo, para que TODO el código existente basado en
 * `CellValue**` (render, patrones, minimapa, I/O) siga funcionando sin cambios
 * vía `rows_ptr()`.
 */
class Board {
public:
    Board(int rows, int cols)
        : rows_{rows}
        , cols_{cols}
        , data_{std::make_unique<CellValue[]>(static_cast<size_t>(rows) * cols)}
        , row_table_{std::make_unique<CellValue*[]>(rows)}
    {
        // make_unique<T[]> value-inicializa => todo CELL_DEAD (0)
        rebuild_row_table();
    }

    // Copia profunda (rara vez usada; el hot path usa copy_from + swap)
    Board(const Board& other)
        : Board(other.rows_, other.cols_)
    {
        std::memcpy(data_.get(), other.data_.get(), byte_size());
    }

    Board& operator=(const Board& other) {
        if (this != &other) {
            copy_from(other);
        }
        return *this;
    }

    // Mover transfiere los bloques del heap (las direcciones no cambian, por lo
    // que la tabla de filas movida sigue siendo válida).
    Board(Board&&) noexcept = default;
    Board& operator=(Board&&) noexcept = default;

    int rows() const { return rows_; }
    int cols() const { return cols_; }
    int size() const { return rows_ * cols_; }
    size_t byte_size() const { return static_cast<size_t>(rows_) * cols_ * sizeof(CellValue); }

    CellValue at(int r, int c) const { return data_[static_cast<size_t>(r) * cols_ + c]; }
    void set(int r, int c, CellValue v) { data_[static_cast<size_t>(r) * cols_ + c] = v; }

    CellValue* data() { return data_.get(); }
    const CellValue* data() const { return data_.get(); }

    // Vista `CellValue**` para el código heredado (render / patrones / I/O).
    CellValue** rows_ptr() { return row_table_.get(); }

    void clear() { std::memset(data_.get(), CELL_DEAD, byte_size()); }

    void copy_from(const Board& other) {
        // Mismas dimensiones (grilla fija): copia byte a byte.
        std::memcpy(data_.get(), other.data_.get(), byte_size());
    }

    // O(1): intercambia los bloques del heap y sus tablas de filas.
    void swap(Board& other) noexcept {
        std::swap(rows_, other.rows_);
        std::swap(cols_, other.cols_);
        std::swap(data_, other.data_);
        std::swap(row_table_, other.row_table_);
    }

private:
    void rebuild_row_table() {
        for (int i = 0; i < rows_; ++i) {
            row_table_[i] = data_.get() + static_cast<size_t>(i) * cols_;
        }
    }

    int rows_;
    int cols_;
    std::unique_ptr<CellValue[]> data_;
    std::unique_ptr<CellValue*[]> row_table_;
};
