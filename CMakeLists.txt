cmake_minimum_required(VERSION 3.10)

project(Calc VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(Git)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
    set(GIT_VERSION "unknown")
endif()

if(NOT GIT_VERSION)
    set(GIT_VERSION "unknown")
endif()

add_compile_definitions(VERSION="${GIT_VERSION}")

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
