#!/usr/bin/env bash
#
# build-windows.sh — cross-compila el juego a Windows x86_64 con MinGW-w64 desde
# Linux y arma un dist/ listo para copiar a una máquina Windows.
#
# Requisitos: mingw-w64 (apt install mingw-w64), cmake, curl, tar.
# Las libs SDL2/SDL2_ttf/SDL2_image (dev packages de MinGW) se descargan de los
# releases de github.com/libsdl-org y se instalan en un prefix local.
#
# Uso:   scripts/build-windows.sh
#
set -euo pipefail

# ---- Config -----------------------------------------------------------------
SDL2_VER="${SDL2_VER:-2.32.10}"
SDL2_TTF_VER="${SDL2_TTF_VER:-2.24.0}"
SDL2_IMAGE_VER="${SDL2_IMAGE_VER:-2.8.12}"
ARCH=x86_64-w64-mingw32

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$REPO/build-win"
PREFIX="$BUILD_DIR/sdl2-mingw"          # aquí se instalan las libs SDL2 de mingw
DL="$BUILD_DIR/downloads"
DIST="$REPO/dist"

echo ">> Repo:   $REPO"
echo ">> Build:  $BUILD_DIR"

# ---- 1) Compilador MinGW ----------------------------------------------------
# Preferencia: (a) mingw del sistema, (b) env conda 'winbuild', (c) apt install.
# El env conda se crea sin sudo con:
#   conda create -n winbuild -c conda-forge gcc_win-64 gxx_win-64
MINGW_SYSROOT_ARG=""
if command -v ${ARCH}-g++ >/dev/null 2>&1; then
    echo ">> Usando MinGW del sistema: $(command -v ${ARCH}-g++)"
elif [ -x "$HOME/miniconda3/envs/winbuild/bin/${ARCH}-g++" ]; then
    WB="$HOME/miniconda3/envs/winbuild"
    export PATH="$WB/bin:$PATH"
    MINGW_SYSROOT_ARG="-DMINGW_SYSROOT=$WB/x86_64-w64-mingw32/sysroot"
    echo ">> Usando MinGW de conda (env winbuild)"
else
    echo ">> MinGW-w64 no encontrado. Instalando con apt (requiere sudo)..."
    sudo apt-get update && sudo apt-get install -y mingw-w64
fi
echo ">> Compilador: $(${ARCH}-g++ --version | head -1)"

# ---- 2) Libs SDL2 de MinGW (descargar + instalar en $PREFIX) ---------------
mkdir -p "$DL" "$PREFIX"
fetch_sdl() {  # $1=nombre repo  $2=version  $3=tarball-basename
    local url="https://github.com/libsdl-org/$1/releases/download/release-$2/$3-devel-$2-mingw.tar.gz"
    local tgz="$DL/$3-devel-$2-mingw.tar.gz"
    [ -f "$tgz" ] || { echo ">> Descargando $3 $2..."; curl -fL --retry 3 -o "$tgz" "$url"; }
    local out="$DL/$3-$2"; rm -rf "$out"; mkdir -p "$out"
    tar -xzf "$tgz" -C "$out" --strip-components=1
    echo ">> Instalando $3 en el prefix..."
    cp -a "$out/$ARCH/." "$PREFIX/"
}
fetch_sdl SDL       "$SDL2_VER"       SDL2
fetch_sdl SDL_ttf   "$SDL2_TTF_VER"   SDL2_ttf
fetch_sdl SDL_image "$SDL2_IMAGE_VER" SDL2_image

# Reapuntar el prefix de las .pc al lugar real de instalación.
for pc in "$PREFIX"/lib/pkgconfig/*.pc; do
    sed -i "s|^prefix=.*|prefix=$PREFIX|" "$pc"
done
echo ">> .pc disponibles: $(ls "$PREFIX"/lib/pkgconfig/ | tr '\n' ' ')"

# ---- 3) Configurar + compilar ----------------------------------------------
rm -f "$BUILD_DIR/CMakeCache.txt"   # asegura que apliquen los *_FLAGS_INIT del toolchain
cmake -S "$REPO" -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$REPO/cmake/toolchain-mingw64.cmake" \
    -DMINGW_SDL2_ROOT="$PREFIX" $MINGW_SYSROOT_ARG \
    -DBUILD_TESTS=OFF \
    -DBUILD_NCURSES_VIEWER=OFF \
    -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --target life_game conway_server -j"$(nproc)"

# ---- 4) Empaquetar dist/ ----------------------------------------------------
echo ">> Empaquetando en $DIST ..."
rm -rf "$DIST"; mkdir -p "$DIST/bin"
cp "$BUILD_DIR/life_game.exe" "$BUILD_DIR/conway_server.exe" "$DIST/bin/"
cp "$PREFIX/bin/"*.dll "$DIST/bin/"                 # SDL2*.dll + deps (png/zlib/freetype/…)
# Assets de runtime: el juego abre ../includes/Roboto-Regular.ttf desde bin/.
mkdir -p "$DIST/includes"
cp "$REPO/includes/Roboto-Regular.ttf" "$DIST/includes/" 2>/dev/null || true
cp -r "$REPO/includes/img" "$DIST/includes/" 2>/dev/null || true
cp -r "$REPO/includes/patterns" "$DIST/includes/" 2>/dev/null || true

echo ""
echo ">> LISTO. Contenido de dist/bin:"
ls -1 "$DIST/bin"
echo ""
echo ">> Para probar en esta máquina:  wine $DIST/bin/conway_server.exe"
echo ">> En Windows: copiar la carpeta dist/ y ejecutar dist/bin/life_game.exe"
