cmake_minimum_required(VERSION 3.15)
project(ConwayLife VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Opciones de compilación
option(BUILD_TESTS "Build test programs" ON)
option(BUILD_NCURSES_VIEWER "Build ncurses terminal viewer" ON)

# Buscar paquetes SDL2
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)
pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)

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
add_library(Graphics SHARED
    scr/grid.cpp
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
    ${SDL2_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    imgui
)

# ============================================================================
# Ejecutable principal: Cliente gráfico SDL2
# ============================================================================
add_executable(life_game
    scr/main.cpp
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
    ${SDL2_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
)

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
    pthread
)
# Visor ncurses (terminal)
# ============================================================================
if(BUILD_NCURSES_VIEWER)
    add_executable(ncurses_viewer
        scr/ncurses_viewer.cpp
        scr/grid.cpp
    )
    target_include_directories(ncurses_viewer PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/includes
        ${CMAKE_CURRENT_SOURCE_DIR}/scr
        ${CURSES_INCLUDE_DIR}
        ${SDL2_INCLUDE_DIRS}
    )
    target_link_libraries(ncurses_viewer
        ${CURSES_LIBRARIES}
        ${SDL2_LIBRARIES}
        pthread
    )
    
    message(STATUS "ncurses_viewer will be built")
endif()

# ============================================================================
# Tests con Google Test
# ============================================================================
if(BUILD_TESTS AND GTest_FOUND)
    enable_testing()
    
    add_executable(test_server
        tests/test_server.cpp
    )

    target_include_directories(test_server PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/includes
        ${CMAKE_CURRENT_SOURCE_DIR}/scr
    )

    target_link_libraries(test_server
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        pthread
    )
    
    # Registrar tests con CTest
    include(GoogleTest)
    gtest_discover_tests(test_server)
    
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
