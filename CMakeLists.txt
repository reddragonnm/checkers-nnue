cmake_minimum_required(VERSION 3.28)
project(CheckersAI LANGUAGES CXX)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)
FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.2
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(SFML)

add_executable(main src/main.cpp)
target_compile_features(main PRIVATE cxx_std_23)
target_link_libraries(main PRIVATE SFML::Graphics SFML::Network)
target_compile_options(main PRIVATE -O3 -march=native -ffast-math)

add_executable(bench src/bench.cpp)
target_compile_features(bench PRIVATE cxx_std_23)
target_compile_options(bench PRIVATE -O3 -march=native -ffast-math)
target_compile_definitions(bench PRIVATE NDEBUG)

add_executable(matchmake src/matchmake.cpp)
target_compile_features(matchmake PRIVATE cxx_std_23)
target_compile_options(matchmake PRIVATE -O3 -march=native -ffast-math)
target_compile_definitions(matchmake PRIVATE NDEBUG)

add_executable(trainNNUE src/trainNNUE.cpp)
target_compile_features(trainNNUE PRIVATE cxx_std_23)
target_compile_options(trainNNUE PRIVATE -O3 -march=native -ffast-math)
target_compile_definitions(trainNNUE PRIVATE NDEBUG)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT main)
