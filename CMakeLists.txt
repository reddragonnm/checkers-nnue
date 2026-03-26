cmake_minimum_required(VERSION 3.28)
project(CheckersAI LANGUAGES CXX)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(EMSCRIPTEN)
    add_executable(web src/web.cpp)
    target_compile_features(web PRIVATE cxx_std_23)
    target_compile_options(web PRIVATE -O3 -ffast-math)
    target_link_options(web PRIVATE
        -O3
        -sALLOW_MEMORY_GROWTH=1
        -sENVIRONMENT=web
        -sWASM=1
        -sNO_EXIT_RUNTIME=1
        "SHELL:-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
        "SHELL:-sEXPORTED_FUNCTIONS=['_malloc','_free','_init_engine','_reset_game','_get_turn','_get_human_side','_get_game_state','_get_piece_at','_get_legal_move_count_from','_get_legal_move_to','_make_human_move','_make_ai_move','_get_last_score','_get_last_depth','_get_last_time','_get_nodes_hit']"
        --preload-file ${CMAKE_SOURCE_DIR}/nnue_best.bin@/nnue_best.bin
    )
    set_target_properties(web PROPERTIES
        OUTPUT_NAME "checkers"
        SUFFIX ".js"
    )
    configure_file(${CMAKE_SOURCE_DIR}/web/index.html ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/index.html COPYONLY)
    configure_file(${CMAKE_SOURCE_DIR}/web/app.js ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/app.js COPYONLY)
    configure_file(${CMAKE_SOURCE_DIR}/web/styles.css ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/styles.css COPYONLY)
else()
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
    target_compile_definitions(bench PRIVATE)

    add_executable(matchmake src/matchmake.cpp)
    target_compile_features(matchmake PRIVATE cxx_std_23)
    target_link_libraries(matchmake PRIVATE SFML::Graphics SFML::Network)
    target_compile_options(matchmake PRIVATE -O3 -march=native -ffast-math)
    target_compile_definitions(matchmake PRIVATE)

    add_executable(trainNNUE src/trainNNUE.cpp)
    target_compile_features(trainNNUE PRIVATE cxx_std_23)
    target_compile_options(trainNNUE PRIVATE -O3 -march=native -ffast-math)
    target_compile_definitions(trainNNUE PRIVATE)

    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT main)
endif()
