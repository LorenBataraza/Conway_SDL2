# Toolchain para cross-compilar el juego a Windows x86_64 con MinGW-w64 desde
# Linux. No usar directamente: scripts/build-windows.sh lo invoca y le pasa la
# ruta de las libs SDL2-mingw en MINGW_SDL2_ROOT.
#
#   cmake -S . -B build-win \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw64.cmake \
#         -DMINGW_SDL2_ROOT=/abs/path/al/prefix/sdl2-mingw

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)
set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_RC_COMPILER  ${TOOLCHAIN_PREFIX}-windres)

# Definir _WIN32_WINNT=0x0601 (Windows 7) para TODAS las unidades, ANTES de
# cualquier include. Necesario para que gthr-win32 defina __gthread_cond_t (las
# condition variables nativas requieren Vista+); si no, std::mutex/<memory> no
# compilan bajo el modelo de hilos win32 de MinGW.
set(CMAKE_C_FLAGS_INIT   "-D_WIN32_WINNT=0x0601")
set(CMAKE_CXX_FLAGS_INIT "-D_WIN32_WINNT=0x0601")

# Enlazar libstdc++/libgcc/winpthread de forma estática para no tener que
# distribuir esas DLL del compilador junto al .exe.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++ -static -lpthread")

# Raíces donde buscar headers/libs de Windows: el sysroot de mingw + el prefix
# con SDL2/SDL2_ttf/SDL2_image. MINGW_SYSROOT permite usar un toolchain no estándar
# (p. ej. el de conda-forge); por defecto, el de mingw-w64 de apt.
if(NOT DEFINED MINGW_SYSROOT)
    set(MINGW_SYSROOT /usr/${TOOLCHAIN_PREFIX})
endif()
set(CMAKE_FIND_ROOT_PATH ${MINGW_SYSROOT} ${MINGW_SDL2_ROOT})

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)   # programas: usar los del host
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# pkg-config debe leer las .pc de MinGW (SDL2/SDL2_ttf/SDL2_image) del prefix.
if(MINGW_SDL2_ROOT)
    set(ENV{PKG_CONFIG_LIBDIR} "${MINGW_SDL2_ROOT}/lib/pkgconfig")
    set(ENV{PKG_CONFIG_SYSROOT_DIR} "")
endif()
