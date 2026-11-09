cmake_minimum_required(VERSION 3.24)

project(
  hd-sim
  VERSION 0.1.0
  DESCRIPTION "Hydromechanic simulation"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB SOURCE_FILES CONFIGURE_DEPENDS src/*.cpp)

add_executable(hd-sim
  ${SOURCE_FILES}
)

target_include_directories(hd-sim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_options(hd-sim PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

install(TARGETS hd-sim
  RUNTIME DESTINATION bin
)