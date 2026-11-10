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

find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)
find_program(SLANGC_EXECUTABLE NAMES slangc REQUIRED)

include(cmake/embed_spirv.cmake)

file(GLOB_RECURSE SHADER_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.slang"
)

if(NOT SHADER_SOURCES)
  message(FATAL_ERROR "No Slang shader sources found in ${CMAKE_CURRENT_SOURCE_DIR}/shaders")
endif()

file(GLOB SOURCE_FILES CONFIGURE_DEPENDS src/*.cpp)

add_executable(hd-sim
  ${SOURCE_FILES}
)

embed_spirv(hd-sim SOURCES ${SHADER_SOURCES})

target_link_libraries(hd-sim PRIVATE glfw Vulkan::Vulkan)

target_include_directories(hd-sim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_options(hd-sim PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

install(TARGETS hd-sim
  RUNTIME DESTINATION bin
)