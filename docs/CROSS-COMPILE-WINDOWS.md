# Cómo se crea un `.exe` de Windows desde Linux — guía conceptual y de la toolchain

Este documento explica **cómo funciona** producir un ejecutable de Windows desde una
máquina Linux (cross-compilación), **cómo usar la toolchain a mano** (sin el script),
y **qué se cambió en este proyecto y por qué**. Para el "un comando y listo" está
[BUILD-WINDOWS.md](../BUILD-WINDOWS.md); esto es el "por qué y cómo por dentro".

Índice:
1. [Qué es realmente un `.exe` (y por qué no corre en Linux)](#1-qué-es-realmente-un-exe)
2. [Qué es cross-compilar y qué es MinGW-w64](#2-cross-compilar-y-mingw-w64)
3. [La cadena completa: de `.cpp` a `.exe`](#3-la-cadena-completa-de-cpp-a-exe)
4. [Dependencias de runtime: DLLs, el CRT, y enlace estático](#4-dependencias-de-runtime)
5. [Cómo usar la toolchain paso a paso (a mano)](#5-usar-la-toolchain-paso-a-paso)
6. [Qué se cambió en ESTE proyecto y por qué](#6-qué-se-cambió-en-este-proyecto)
7. [Bitácora: los problemas que aparecieron y cómo se resolvieron](#7-bitácora-de-problemas)
8. [Verificar el `.exe` en Linux con wine](#8-verificar-con-wine)
9. [Cómo extenderlo (agregar una librería nueva)](#9-cómo-extenderlo)

---

## 1. Qué es realmente un `.exe`

Un programa compilado no es "el código": es un archivo binario en un **formato de
contenedor** específico del sistema operativo, que le dice al cargador (loader) del SO
cómo poner el programa en memoria y arrancarlo.

- En **Linux** ese formato es **ELF** (los ejecutables no tienen extensión).
- En **Windows** es **PE/COFF** (*Portable Executable*), y por convención lleva `.exe`.

Son formatos incompatibles: un ELF no lo entiende el loader de Windows y un PE no lo
entiende el de Linux. Por eso "el mismo código" no basta — hay que **regenerar** el
binario en el formato del destino. Un `.exe` PE contiene, entre otras cosas:

- **Secciones** de código (`.text`), datos (`.data`, `.rdata`), etc.
- Un **subsistema**: `CONSOLE` (abre/usa una consola) o `WINDOWS` (app gráfica, sin
  consola). Lo elige el linker; en CMake se controla con `WIN32_EXECUTABLE`.
- Una **import table**: la lista de DLLs y símbolos externos que el programa necesita
  (p. ej. `SDL2.dll`, `ws2_32.dll`, `KERNEL32.dll`). El loader de Windows resuelve esto
  al arrancar, mapeando cada DLL y parcheando las direcciones.
- El **punto de entrada**: en una app de consola es `mainCRTStartup` (que termina
  llamando a tu `main`); en una app gráfica es `WinMainCRTStartup` → `WinMain`.

Como Linux no puede ejecutar PE de forma nativa, para *probar* el `.exe` en Linux se usa
**wine**, que es (a) un cargador de PE y (b) una reimplementación de la API Win32
(`kernel32`, `user32`, `ws2_32`, …). No es un emulador: ejecuta el código x86-64 nativo
y sólo traduce las llamadas al SO.

---

## 2. Cross-compilar y MinGW-w64

**Cross-compilar** = compilar en una plataforma (host = Linux x86-64) un binario para
otra (target = Windows x86-64). Necesitás un **toolchain** cuyo compilador emita PE y
que traiga los headers y las librerías de importación de la API de Windows.

La toolchain que usamos es **MinGW-w64**: es GCC configurado para producir binarios
Win32/PE, más el runtime "mingw-w64" (headers de la API de Windows + import libs). Sus
binarios se llaman con un **prefijo de target**:

```
x86_64-w64-mingw32-gcc      x86_64-w64-mingw32-g++      x86_64-w64-mingw32-windres
└──────┬──────┘ └─┬─┘ └─┬─┘
     arch      vendor  ABI/target
```

Alternativa a MinGW: **MSVC** (el compilador de Microsoft), pero MSVC no corre en Linux;
para cross desde Linux, MinGW es el camino.

### Dos detalles de MinGW que importan

**(a) Modelo de hilos (thread model).** GCC-MinGW se compila con hilos `win32`
(primitivas nativas de Windows) o `posix` (via *winpthreads*). Afecta a
`std::thread`/`std::mutex`/`std::condition_variable`. El modelo `win32` moderno usa las
*condition variables* nativas de Windows, que existen **desde Vista** → requiere que la
macro `_WIN32_WINNT` valga `>= 0x0600` al compilar (ver [bitácora](#7-bitácora-de-problemas)).

**(b) El CRT (C runtime).** Windows tiene dos runtimes de C:
- El clásico **`msvcrt.dll`** (viejo, siempre presente).
- El moderno **UCRT** (*Universal CRT*: `ucrtbase.dll` + los forwarders
  `api-ms-win-crt-*.dll`), incluido de fábrica en **Windows 10/11**.

Qué CRT usás depende de cómo esté armado tu MinGW. El toolchain de conda-forge que usamos
apunta a **UCRT**, así que el `.exe` corre directo en Windows 10/11 sin instalar nada.

---

## 3. La cadena completa: de `.cpp` a `.exe`

Cuando corrés `x86_64-w64-mingw32-g++ archivo.cpp -o archivo.exe`, pasan 4 etapas:

```
 .cpp ──preprocesador──▶ .cpp expandido ──compilador──▶ .s (asm) ──ensamblador──▶ .o (PE/COFF)
                                                                                     │
                                       import libs (libSDL2.dll.a, libws2_32.a, …)   │
                                                                    └──────linker────┘
                                                                                     ▼
                                                                                  .exe (PE)
```

1. **Preprocesado**: resuelve `#include`/`#define`. Acá `net_compat.h` decide, con
   `#ifdef _WIN32`, incluir `<winsock2.h>` en vez de `<sys/socket.h>`.
2. **Compilación**: traduce C++ a assembler del target (x86-64), usando los **headers de
   Windows** del sysroot de MinGW (no los de Linux).
3. **Ensamblado**: genera archivos objeto `.o` en formato PE/COFF (no ELF).
4. **Enlazado (linking)**: junta los `.o`, resuelve los símbolos externos y arma el PE.
   Acá aparece un concepto clave de Windows:

   > **Import library vs DLL.** No linkeás contra la DLL directamente. Linkeás contra una
   > **import library** (`libSDL2.dll.a`, `libws2_32.a`): un stub chico que, por cada
   > función exportada, mete una entrada en la **import table** del `.exe`. La DLL de
   > verdad (`SDL2.dll`) se carga **en runtime**, no en el link. Por eso al final tenés
   > que *distribuir* las DLLs junto al `.exe`, aunque no las pases al linker.

En un proyecto real no invocás `g++` a mano: **CMake** genera esos comandos. El
**toolchain file** le dice a CMake *qué* compilador usar y *dónde* buscar las cosas del
target (ver §5).

---

## 4. Dependencias de runtime

Un `.exe` en Windows, al arrancar, necesita encontrar todas las DLLs de su import table.
Se dividen en:

| Tipo | Ejemplos | ¿Hay que distribuirlas? |
|------|----------|--------------------------|
| API del SO | `KERNEL32.dll`, `USER32.dll`, `ws2_32.dll` (Winsock) | No, vienen con Windows |
| CRT (UCRT) | `ucrtbase.dll`, `api-ms-win-crt-*.dll` | No en Win10/11 (vienen de fábrica) |
| Runtime del compilador | `libstdc++-6.dll`, `libgcc_s_seh-1.dll`, `libwinpthread-1.dll` | **Sí**, salvo que enlaces estático |
| Librerías de terceros | `SDL2.dll`, `SDL2_ttf.dll`, `SDL2_image.dll` | **Sí, siempre** |

Para **no** tener que distribuir el runtime del compilador, en el toolchain file
enlazamos esas de forma **estática**:

```cmake
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++ -static -lpthread")
```

Así `libstdc++`/`libgcc`/`winpthread` quedan *dentro* del `.exe` y sólo hay que shippear
las DLLs de SDL2.

**Cómo se inspeccionan las dependencias** (esto es exactamente lo que hice para depurar):

```bash
x86_64-w64-mingw32-objdump -p dist/bin/life_game.exe | grep "DLL Name"
```

Eso lista la import table. Si aparece una DLL que no viene con Windows y no está en
`dist/bin/`, el programa no arranca (error "DLL not found").

---

## 5. Usar la toolchain paso a paso

Esta es la parte que pediste: **cómo se usa la toolchain a mano**, para entender qué hace
el script por dentro. Son 4 pasos.

### Paso 0 — Tener el compilador cross

Dos formas:

```bash
# A) Debian/Ubuntu (necesita sudo):
sudo apt install mingw-w64

# B) Sin sudo, con conda (lo que usamos en esta máquina):
conda create -n winbuild -c conda-forge gcc_win-64 gxx_win-64
export PATH="$HOME/miniconda3/envs/winbuild/bin:$PATH"
```

Verificás que anda y de paso ves el thread model:

```bash
x86_64-w64-mingw32-g++ --version
x86_64-w64-mingw32-g++ -v 2>&1 | grep "Thread model"   # win32 o posix
```

### Paso 1 — Conseguir las librerías (SDL2) para el target

Las librerías de terceros tienen que ser **builds de Windows** (no las de Linux). SDL2
publica *dev packages* de MinGW. Se descargan, se extrae la carpeta del arch y se ajusta
el `prefix` de los archivos `.pc` (pkg-config) a la ruta real:

```bash
PREFIX=$PWD/build-win/sdl2-mingw          # acá van las libs Windows de SDL2
curl -fL -o sdl2.tgz \
  https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-devel-2.32.10-mingw.tar.gz
tar -xzf sdl2.tgz
cp -a SDL2-2.32.10/x86_64-w64-mingw32/. "$PREFIX/"
# repetir para SDL2_ttf (2.24.0) y SDL2_image (2.8.12)
sed -i "s|^prefix=.*|prefix=$PREFIX|" "$PREFIX"/lib/pkgconfig/*.pc
```

Ese `PREFIX` termina teniendo `include/`, `lib/` (con las import libs y los `.pc`) y
`bin/` (con las **DLLs** `SDL2.dll`, etc. que después se distribuyen).

### Paso 2 — Configurar con el toolchain file

El **toolchain file** ([`cmake/toolchain-mingw64.cmake`](../cmake/toolchain-mingw64.cmake))
es lo que convierte a CMake de "build nativo Linux" a "cross-build Windows". Le decís a
CMake:

```cmake
set(CMAKE_SYSTEM_NAME Windows)              # → CMake activa WIN32, usa sufijo .exe, etc.
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
# Buscar libs/headers del TARGET en el sysroot de mingw + el prefix de SDL2, no en el host:
set(CMAKE_FIND_ROOT_PATH ${MINGW_SYSROOT} ${MINGW_SDL2_ROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)  # find_library sólo mira el target
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER) # pero los programas (cmake, pkg-config) del host
# pkg-config lee las .pc de SDL2-mingw:
set(ENV{PKG_CONFIG_LIBDIR} "${MINGW_SDL2_ROOT}/lib/pkgconfig")
# imprescindible para std::mutex bajo hilos win32 (ver bitácora):
set(CMAKE_CXX_FLAGS_INIT "-D_WIN32_WINNT=0x0601")
```

Se invoca pasándolo con `-DCMAKE_TOOLCHAIN_FILE`:

```bash
cmake -S . -B build-win \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw64.cmake \
  -DMINGW_SDL2_ROOT="$PREFIX" \
  -DMINGW_SYSROOT="$HOME/miniconda3/envs/winbuild/x86_64-w64-mingw32/sysroot" \
  -DBUILD_TESTS=OFF -DBUILD_NCURSES_VIEWER=OFF -DCMAKE_BUILD_TYPE=Release
```

Cuando esto corre, CMake evalúa `if(WIN32)` en `CMakeLists.txt` → excluye los targets
sólo-Linux, linkea `ws2_32`, compila `Graphics` estática, etc.

> Detalle: los `*_FLAGS_INIT` del toolchain sólo se aplican en un caché **fresco**. Si
> reconfigurás sobre un `build-win/` viejo, borrá `build-win/CMakeCache.txt` primero.

### Paso 3 — Compilar y empaquetar

```bash
cmake --build build-win --target life_game conway_server -j$(nproc)
# Distribuir: el .exe + las DLLs de SDL2 + los assets (la fuente)
mkdir -p dist/bin dist/includes
cp build-win/life_game.exe build-win/conway_server.exe dist/bin/
cp "$PREFIX/bin/"*.dll dist/bin/
cp includes/Roboto-Regular.ttf dist/includes/
```

El [script `scripts/build-windows.sh`](../scripts/build-windows.sh) automatiza estos 4
pasos.

---

## 6. Qué se cambió en ESTE proyecto

El objetivo era que el **mismo código** compile para Linux y Windows. Cambios:

### 6.1 Capa de red portable — `scr/net_compat.h` (nuevo)

El único código genuinamente no-portable era el networking: Linux usa **sockets POSIX**;
Windows usa **Winsock2**, que tiene otra API. Las diferencias:

| POSIX (Linux) | Winsock2 (Windows) |
|---|---|
| (nada) | `WSAStartup()` / `WSACleanup()` obligatorios |
| `int` como socket | `SOCKET` (entero sin signo de 64 bits) |
| error = `< 0` | error = `INVALID_SOCKET` |
| `close(fd)` | `closesocket(fd)` |
| `fcntl(fd, F_SETFL, O_NONBLOCK)` | `ioctlsocket(fd, FIONBIO, …)` |
| `poll()` | `WSAPoll()` |
| `errno` / `EWOULDBLOCK` | `WSAGetLastError()` / `WSAEWOULDBLOCK` |
| flag `MSG_NOSIGNAL` | no existe (Windows no tiene `SIGPIPE`) |

En vez de ensuciar `server.cpp`/`client.cpp` con `#ifdef`, todo eso vive en
`net_compat.h`, que expone una API única: `socket_t`, `net_init/net_cleanup`,
`net_close`, `net_set_nonblocking`, `net_poll`, `net_would_block`, `net_perror`, y define
`MSG_NOSIGNAL` como `0` en Windows. Los call-sites quedan casi iguales; sólo cambian
`close→net_close`, `poll→net_poll`, los chequeos de error, y los sockets pasan a
`socket_t`.

Además hay que **llamar `net_init()`** una vez antes de usar sockets (inicializa
Winsock): en el cliente en `init()` de `main.cpp`, en el servidor al empezar `main()`.

> Orden de includes: en Windows `<winsock2.h>` debe ir **antes** que `<windows.h>` (que
> SDL puede arrastrar). Por eso `net_compat.h` se incluye primero en las unidades que
> mezclan red y SDL.

### 6.2 CMake multiplataforma — `CMakeLists.txt`

- **SDL2 con `IMPORTED_TARGET`**: `pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)`
  y se linkea `PkgConfig::SDL2`. Ese target "moderno" propaga includes, rutas de libs y
  flags (incluido `-lSDL2main` y el subsistema) tanto en Linux como en el cross.
- **Guards de plataforma**: `if(NOT WIN32)` excluye `life_wallpaper` (usa X11) y
  `ncurses_viewer` (usa ncurses); el test de red POSIX (`test_server`) también.
  `life_game`, `conway_server` y `test_automaton` quedan multiplataforma.
- **Winsock**: `if(WIN32) target_link_libraries(... ws2_32)`.
- **Threads**: `Threads::Threads` en vez de `pthread` crudo.
- **`Graphics` estática en Windows**: para tener **una sola** copia de `libstdc++` (en
  cada `.exe`) y no tener que distribuir `libGraphics.dll` ni las DLLs del compilador.

### 6.3 Toolchain file y script (nuevos)

- [`cmake/toolchain-mingw64.cmake`](../cmake/toolchain-mingw64.cmake): explicado en §5.
- [`scripts/build-windows.sh`](../scripts/build-windows.sh): detecta el compilador
  (sistema → conda → apt), baja las libs SDL2, configura, compila y arma `dist/`.

### 6.4 Retoque menor de runtime

`connection_history.cpp` guardaba el historial en `$HOME/.config/...`; en Windows `HOME`
no suele existir, así que ahora cae a `%USERPROFILE%`.

---

## 7. Bitácora de problemas

Los obstáculos reales que aparecieron al hacerlo, y cómo se resolvieron. (Sirve para
entender *por qué* el toolchain file quedó como quedó.)

1. **`sudo` no disponible.** No se podía `apt install mingw-w64`. → Se usó el
   cross-compiler de **conda-forge** (`gcc_win-64`/`gxx_win-64`), que es un GCC-MinGW
   hosteado en Linux, sin sudo. El script lo detecta como fallback.

2. **Nombres de los packages SDL2.** Los assets se llaman
   `SDL2-devel-<ver>-mingw.tar.gz` (no `SDL2-<ver>-mingw.tar.gz`). Versiones reales:
   SDL2 `2.32.10`, SDL2_ttf `2.24.0`, SDL2_image `2.8.12`.

3. **`std::mutex` no compilaba** (`__gthread_cond_t does not name a type`). Causa: el
   GCC-MinGW de conda usa el **modelo de hilos win32**, cuyas *condition variables*
   nativas requieren `_WIN32_WINNT >= 0x0600` (Vista). Como los headers de SDL fijaban esa
   macro baja antes de que `<memory>` incluyera el `<mutex>` de libstdc++, faltaba el
   tipo. → Se define **`-D_WIN32_WINNT=0x0601` global** en el toolchain file (aplica a
   todas las unidades, antes de cualquier include). Fue el arreglo clave para que el
   cliente linkeara.

4. **`libGraphics.dll` no encontrada + `libstdc++-6.dll` faltante.** `Graphics` se
   compilaba como DLL y arrastraba `libstdc++`/`libgcc` dinámicos, que no estaban en
   `dist/`. → Se compila `Graphics` **estática** en Windows: desaparece esa DLL y su
   `libstdc++` queda dentro de cada `.exe`.

5. **Dependencia del UCRT.** El `.exe` importa `api-ms-win-crt-*.dll` (UCRT). No es un
   problema: Windows 10/11 lo trae de fábrica. (Sí importa saberlo si el destino fuera
   Windows 7/8 sin UCRT instalado.)

---

## 8. Verificar con wine

wine ejecuta el PE en Linux, así se prueba sin una máquina Windows:

```bash
# Servidor (consola): arranca Winsock y escucha en 0.0.0.0:6969
cd dist/bin && wine conway_server.exe

# Cliente (gráfico): necesita un display X
cd dist/bin && DISPLAY=:1 wine life_game.exe
```

Como wine usa los sockets del host, se puede validar el protocolo conectándose desde un
cliente Linux al servidor-Windows-bajo-wine (se probó: responde `CONFIG_UPDATE` correcto).
`WINEDEBUG=-all` baja el ruido; `wineserver -k` mata procesos colgados.

> `wine` **no** es el entorno de producción: es para smoke-test. El destino real es una
> máquina Windows, donde se copia la carpeta `dist/`.

---

## 9. Cómo extenderlo

Para agregar una librería de terceros nueva (p. ej. `SDL2_mixer`):

1. Conseguir su **dev package de MinGW** e instalarlo en el mismo `PREFIX`
   (`build-win/sdl2-mingw`), con su `.pc` corregido.
2. En `CMakeLists.txt`: `pkg_check_modules(SDL2_MIXER REQUIRED IMPORTED_TARGET SDL2_mixer)`
   y linkear `PkgConfig::SDL2_MIXER`.
3. Copiar su `*.dll` de `$PREFIX/bin` a `dist/bin`.

Para **otro código no-portable** (más allá de red): aislarlo detrás de un header con
`#ifdef _WIN32`, como se hizo con `net_compat.h`. Regla general: mantené el código común
agnóstico y empujá las diferencias del SO a un único lugar.
```
