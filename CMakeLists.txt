cmake_minimum_required(VERSION 3.15)
project(ConwayLife VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Buscar paquetes SDL2 (solo necesario para componentes gráficos)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)
pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)

# Configuración común para todos los targets
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

# Librería compartida para componentes comunes
add_library(Graphics SHARED
    scr/grid.cpp
    scr/dotted_line.cpp
    scr/main_menu.cpp
)

target_link_libraries(Graphics
    ${SDL2_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    imgui
)

# Configuración para el cliente gráfico
add_executable(life_game
    scr/main.cpp
)

target_include_directories(life_game PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/includes
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/imgui
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

# Configuración para el servidor
add_executable(conway_server
    scr/server.cpp
    scr/grid.cpp  # Solo si el servidor necesita acceso directo a grid.cpp
)

target_include_directories(conway_server PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/includes
)

target_link_libraries(conway_server
    Graphics  # Solo si necesita la librería Graphics
    # Agregar dependencias de red si son necesarias
    pthread
)

# Mensajes de depuración
message(STATUS "SDL2_image include dirs: ${SDL2_IMAGE_INCLUDE_DIRS}")
message(STATUS "SDL2_image libraries: ${SDL2_IMAGE_LIBRARIES}")




