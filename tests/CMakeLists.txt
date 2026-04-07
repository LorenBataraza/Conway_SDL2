# Solo compilar tests si se especifica -DBUILD_TESTS=ON
option(BUILD_TESTS "Build tests" ON)

if (BUILD_TESTS)
    find_package(Catch2 REQUIRED)  # Busca Catch2 en el sistema

    add_executable(tests src/grid_test.cpp)
    target_link_libraries(tests PRIVATE 
        life_game  # Enlaza con el código principal
        Catch2::Catch2
    )

    enable_testing()
    add_test(NAME grid_tests COMMAND tests)
endif()
