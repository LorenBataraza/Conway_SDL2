# Compilar para Windows (cross-compile desde Linux con MinGW-w64)

El código fuente es el mismo en Linux y Windows. La única diferencia real es la
capa de red: en Windows se usa **Winsock2** en vez de sockets POSIX, encapsulado
en [`scr/net_compat.h`](scr/net_compat.h). El sistema de compilación (CMake) ya es
multiplataforma.

## Rápido: un solo comando

```bash
scripts/build-windows.sh
```

Esto:
1. Ubica un compilador `x86_64-w64-mingw32-g++` (del sistema, de un env conda
   `winbuild`, o lo instala con `apt`).
2. Descarga las libs **SDL2 / SDL2_ttf / SDL2_image** (dev packages de MinGW) de
   los releases de libsdl.org y las instala en `build-win/sdl2-mingw`.
3. Cross-compila `life_game.exe` y `conway_server.exe`.
4. Arma `dist/` listo para copiar a Windows:
   ```
   dist/bin/       life_game.exe, conway_server.exe, SDL2.dll, SDL2_ttf.dll, SDL2_image.dll
   dist/includes/  Roboto-Regular.ttf (+ img/, patterns/)
   ```

En Windows 10/11 (que ya traen el UCRT) se ejecuta `dist/bin/life_game.exe`
directamente. `libstdc++`/`libgcc` van enlazados estáticamente, así que no hace
falta distribuir DLLs del compilador.

## Toolchains soportados

- **MinGW-w64 del sistema** (Debian/Ubuntu: `sudo apt install mingw-w64`).
- **conda-forge sin sudo**: `conda create -n winbuild -c conda-forge gcc_win-64 gxx_win-64`.

Ambos usan el toolchain file [`cmake/toolchain-mingw64.cmake`](cmake/toolchain-mingw64.cmake),
que define `_WIN32_WINNT=0x0601` globalmente (necesario para `std::mutex` bajo el
modelo de hilos win32 de MinGW) y enlaza la C++ runtime de forma estática.

## Probar el `.exe` en Linux con wine

```bash
wine dist/bin/conway_server.exe          # servidor (escucha en 0.0.0.0:6969)
DISPLAY=:1 wine dist/bin/life_game.exe    # cliente gráfico
```

## Qué NO se compila en Windows

`life_wallpaper` (X11) y `ncurses_viewer` (ncurses) son sólo-Linux y quedan
excluidos automáticamente (`if(NOT WIN32)` en CMakeLists.txt). Los tests de red
(`test_server`, sockets POSIX) también; `test_automaton` sí es multiplataforma.
