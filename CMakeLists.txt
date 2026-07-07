cmake_minimum_required(VERSION 3.15)
project(ConwayLife VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Opciones de compilación
option(BUILD_TESTS "Build test programs" ON)
option(BUILD_NCURSES_VIEWER "Build ncurses terminal viewer" ON)

# Hilos (Threads::Threads; en MinGW usa winpthreads, en Linux pthread).
find_package(Threads REQUIRED)

# Buscar SDL2 vía pkg-config con IMPORTED_TARGET (PkgConfig::SDL2*). Funciona en
# Linux y en el cross MinGW (los dev packages SDL2-mingw traen .pc); propaga
# includes, library dirs y flags (SDL2main, -mwindows) al target.
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SDL2_TTF REQUIRED IMPORTED_TARGET SDL2_ttf)
pkg_check_modules(SDL2_IMAGE REQUIRED IMPORTED_TARGET SDL2_image)

# Buscar ncurses (opcional)
if(BUILD_NCURSES_VIEWER)
    find_package(Curses)
    if(NOT CURSES_FOUND)
        message(WARNING "ncurses not found. ncurses_viewer will not be built.")
        set(BUILD_NCURSES_VIEWER OFF)
    endif()
endif()

# Buscar Google Test (opcional)
if(BUILD_TESTS)
    find_package(GTest)
    if(NOT GTest_FOUND)
        # Intentar con FetchContent si no está instalado
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.14.0
        )
        # Para Windows: Evitar override de las configuraciones de runtime
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
        set(GTest_FOUND TRUE)
        message(STATUS "Google Test fetched from GitHub")
    else()
        message(STATUS "Google Test found: ${GTest_VERSION}")
    endif()
endif()

# ============================================================================
# Librería ImGui (para la interfaz gráfica)
# ============================================================================
add_library(imgui STATIC
    includes/imgui/imgui.cpp
    includes/imgui/imgui_impl_sdl2.cpp
    includes/imgui/imgui_impl_sdlrenderer2.cpp
    includes/imgui/imgui_draw.cpp
    includes/imgui/imgui_widgets.cpp
    includes/imgui/imgui_tables.cpp
)

set_property(TARGET imgui PROPERTY POSITION_INDEPENDENT_CODE ON)

target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/includes
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/imgui
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
)

# ============================================================================
# Librería Graphics (componentes comunes con SDL2)
# ============================================================================
# En Windows se compila ESTÁTICA: así hay una sola copia de libstdc++ (linkeada
# en cada .exe) y no hay que distribuir libGraphics.dll ni las DLL del compilador.
if(WIN32)
    set(GRAPHICS_LIB_TYPE STATIC)
else()
    set(GRAPHICS_LIB_TYPE SHARED)
endif()

add_library(Graphics ${GRAPHICS_LIB_TYPE}
    scr/grid.cpp
    scr/automaton.cpp
    scr/rle.cpp
    scr/dotted_line.cpp
    scr/main_menu.cpp
)

target_include_directories(Graphics PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/includes
    ${CMAKE_CURRENT_SOURCE_DIR}/scr
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
)

target_link_libraries(Graphics
    PkgConfig::SDL2
    PkgConfig::SDL2_TTF
    PkgConfig::SDL2_IMAGE
    imgui
)

# La lib compartida no debe redirigir main() a SDL_main (no tiene main).
if(WIN32)
    target_compile_definitions(Graphics PRIVATE SDL_MAIN_HANDLED)
endif()

# ============================================================================
# Ejecutable principal: Cliente gráfico SDL2
# ============================================================================
add_executable(life_game
    scr/main.cpp
    scr/pattern_preview.cpp
    scr/connection_history.cpp
)

target_include_directories(life_game PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/includes
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/scr
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
)

target_link_libraries(life_game
    imgui
    Graphics
    PkgConfig::SDL2
    PkgConfig::SDL2_TTF
    PkgConfig::SDL2_IMAGE
    Threads::Threads
)

if(WIN32)
    # App gráfica: SDL2main provee WinMain; subsistema Windows (sin consola).
    target_link_libraries(life_game ws2_32)
    set_target_properties(life_game PROPERTIES WIN32_EXECUTABLE ON)
endif()

# ============================================================================
# Servidor (con logging y patterns con enum)
# ============================================================================
add_executable(conway_server
    scr/server.cpp
    scr/grid.cpp
)

target_include_directories(conway_server PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/includes
    ${CMAKE_CURRENT_SOURCE_DIR}/scr
)

target_link_libraries(conway_server
    Graphics
    Threads::Threads
)
if(WIN32)
    # Servidor de consola: no redirigir main() a SDL_main y linkear Winsock.
    target_compile_definitions(conway_server PRIVATE SDL_MAIN_HANDLED)
    target_link_libraries(conway_server ws2_32)
endif()

# Visor ncurses (terminal) — sólo Linux (ncurses + sockets POSIX).
# ============================================================================
if(BUILD_NCURSES_VIEWER AND NOT WIN32)
    add_executable(ncurses_viewer
        scr/ncurses_viewer.cpp
        scr/grid.cpp
        scr/automaton.cpp
        scr/rle.cpp
    )
    target_include_directories(ncurses_viewer PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/includes
        ${CMAKE_CURRENT_SOURCE_DIR}/scr
        ${CURSES_INCLUDE_DIR}
        ${SDL2_INCLUDE_DIRS}
    )
    target_link_libraries(ncurses_viewer
        ${CURSES_LIBRARIES}
        PkgConfig::SDL2
        Threads::Threads
    )

    message(STATUS "ncurses_viewer will be built")
endif()

# ============================================================================
# Tests con Google Test
# ============================================================================
if(BUILD_TESTS AND GTest_FOUND)
    enable_testing()
    include(GoogleTest)

    # Tests unitarios del motor (Board/Automaton) — multiplataforma, sin sockets.
    add_executable(test_automaton
        tests/test_automaton.cpp
    )

    target_include_directories(test_automaton PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/includes
        ${CMAKE_CURRENT_SOURCE_DIR}/scr
        ${SDL2_INCLUDE_DIRS}
    )

    target_link_libraries(test_automaton
        Graphics
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        Threads::Threads
    )
    gtest_discover_tests(test_automaton)

    # Tests de integración del servidor — usan sockets POSIX (sólo Linux).
    if(NOT WIN32)
        add_executable(test_server
            tests/test_server.cpp
        )
        target_include_directories(test_server PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/includes
            ${CMAKE_CURRENT_SOURCE_DIR}/scr
            ${SDL2_INCLUDE_DIRS}
        )
        target_link_libraries(test_server
            Graphics          # Board/Automaton para el cliente de referencia lockstep
            GTest::gtest
            GTest::gtest_main
            GTest::gmock
            Threads::Threads
        )
        gtest_discover_tests(test_server)
    endif()

    message(STATUS "Tests will be built with Google Test")
endif()

# ============================================================================
# Información de build
# ============================================================================
message(STATUS "")
message(STATUS "Conway's Game of Life - Build Configuration")
message(STATUS "============================================")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "")
message(STATUS "Targets:")
message(STATUS "  - life_game        : Cliente gráfico SDL2")
message(STATUS "  - conway_server    : Servidor con logging y enum patterns")
if(BUILD_NCURSES_VIEWER)
    message(STATUS "  - ncurses_viewer   : Visor de terminal ncurses")
endif()
if(BUILD_TESTS AND GTest_FOUND)
    message(STATUS "  - test_server      : Tests del servidor (Google Test)")
endif()
message(STATUS "")
message(STATUS "SDL2 include dirs: ${SDL2_INCLUDE_DIRS}")
message(STATUS "SDL2 libraries: ${SDL2_LIBRARIES}")
if(BUILD_NCURSES_VIEWER)
    message(STATUS "ncurses libraries: ${CURSES_LIBRARIES}")
endif()
message(STATUS "")


# ============================================================================
# Wallpaper Mode v3 (renderiza directo a X11 root window) — sólo Linux (X11).
# ============================================================================
if(NOT WIN32)
    find_package(X11 REQUIRED)

    add_executable(life_wallpaper
        scr/wallpaper.cpp
        scr/grid.cpp
        scr/automaton.cpp
        scr/rle.cpp
    )

    target_include_directories(life_wallpaper PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/includes
        ${CMAKE_CURRENT_SOURCE_DIR}/scr
        ${SDL2_INCLUDE_DIRS}
        ${X11_INCLUDE_DIR}
    )

    target_link_libraries(life_wallpaper
        PkgConfig::SDL2
        ${X11_LIBRARIES}
    )
endif()

# Características:
# - Renderiza directamente en la root window de X11
# - Funciona con cualquier window manager
# - Compatible con cualquier librería gráfica (renderiza a buffer)
# - SDL2 solo se usa para modo --windowed (testing)
# - No requiere xwinwrap
