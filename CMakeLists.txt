cmake_minimum_required(VERSION 3.14)
project(chess-engine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)
string(CONCAT CMAKE_CXX_FLAGS
  "${CMAKE_CXX_FLAGS}"
  " -Wall -Wextra")

set(BUILD_SHARED_LIBS OFF)

add_subdirectory("deps/raylib-4.2.0")
add_subdirectory("deps/fmt")

include_directories("${CMAKE_HOME_DIRECTORY}/include/")

set(chess_engine_sources
  src/zobrist.cpp
  src/bitboard.cpp
  src/pawn_move_gen.cpp
  src/move_gen.cpp
  src/load_fen.cpp
  src/board.cpp
)
add_library(chess_engine ${chess_engine_sources})

if(BUILD_TESTING)
  set(tests_sources src/tests.cpp)
  add_executable(chess_tests ${tests_sources})
  target_link_libraries(chess_tests chess_engine fmt)
endif()

set(perft_sources src/perft.cpp)
add_executable(chess_perft ${perft_sources})
target_link_libraries(chess_perft chess_engine)

set(gui_sources src/gui_chess.cpp)
add_executable(chess_gui ${gui_sources})
target_link_libraries(chess_gui chess_engine raylib)
