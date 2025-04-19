cmake_minimum_required(VERSION 3.15)
project(ConwayLife VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Usar pkg-config para encontrar SDL2 (más confiable en Linux)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)

add_library(imgui STATIC
    includes/imgui/imgui.cpp
    includes/imgui/imgui_impl_sdl2.cpp
    includes/imgui/imgui_impl_sdlrenderer2.cpp
    includes/imgui/imgui_draw.cpp
    includes/imgui/imgui_widgets.cpp
    includes/imgui/imgui_tables.cpp
)


set_property(TARGET imgui PROPERTY POSITION_INDEPENDENT_CODE ON)  # Solución PIC

add_library(Graphics SHARED
    scr/grid.cpp
    scr/dotted_line.cpp
    scr/ui.cpp
)

target_link_libraries(Graphics
    ${SDL2_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    imgui
)


# Directorios de include para ImGui
target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/includes
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/imgui
    ${SDL2_INCLUDE_DIRS}
)


# Ejecutable principal
add_executable(life_game
    scr/main.cpp
)

# Directorios de include para el ejecutable
target_include_directories(life_game PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/includes
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/imgui
    ${SDL2_INCLUDE_DIRS}
)

# Enlazado de librerías
target_link_libraries(life_game
    imgui
    Graphics
    ${SDL2_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
)

# Dependencias adicionales para Linux
if(UNIX AND NOT APPLE)
    target_link_libraries(life_game
        m
        dl
        pthread
    )
endif()

# Mensajes de depuración
message(STATUS "SDL2 include dirs: ${SDL2_INCLUDE_DIRS}")
message(STATUS "SDL2 libraries: ${SDL2_LIBRARIES}")
message(STATUS "SDL2_ttf libraries: ${SDL2_TTF_LIBRARIES}")
