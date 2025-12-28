cmake_minimum_required(VERSION 3.10)

project(Calc VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(calc
    src/main.c
    src/eval.c
    src/stack.c
    src/vars.c
    src/ui.c
)

target_include_directories(calc PRIVATE src)

if(NOT WIN32)
    target_link_libraries(calc PRIVATE m)
endif()
