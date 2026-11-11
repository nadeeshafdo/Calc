cmake_minimum_required(VERSION 3.10)

project(Calc VERSION 3.0.0 LANGUAGES C)

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
if(MSVC)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

add_executable(calc
    src/main.c
    src/eval.c
    src/funcs.c
    src/stack.c
    src/vars.c
    src/ui.c
)

target_include_directories(calc PRIVATE src)

if(NOT WIN32)
    target_link_libraries(calc PRIVATE m)
endif()

enable_testing()
add_test(NAME calc_arithmetic COMMAND bash -c "printf '2+3\\n' | $<TARGET_FILE:calc> | grep -q '= 5'")
add_test(NAME calc_power COMMAND bash -c "printf '2^3^2\\n' | $<TARGET_FILE:calc> | grep -q '= 512'")
add_test(NAME calc_percent COMMAND bash -c "printf '500%%25\\n' | $<TARGET_FILE:calc> | grep -q '= 125'")
add_test(NAME calc_implicit COMMAND bash -c "printf 'x=4\\n2x+3\\n' | $<TARGET_FILE:calc> | grep -q '= 11'")
add_test(NAME calc_func COMMAND bash -c "printf 'f(x)=x*2\\nf(5)\\n' | $<TARGET_FILE:calc> | grep -q '= 10'")
add_test(NAME calc_recursion COMMAND bash -c "printf 'f(x)=f(x)\\nf(1)\\n' | $<TARGET_FILE:calc> | grep -q 'Invalid expression'")
add_test(NAME calc_linear COMMAND bash -c "printf '2x+5=15\\n' | $<TARGET_FILE:calc> | grep -q 'x = 5'")
add_test(NAME calc_quadratic COMMAND bash -c "printf '4x^2-5x-12=0\\n' | $<TARGET_FILE:calc> | grep -q 'x = '")
add_test(NAME calc_system COMMAND bash -c "printf '7x+2y=24; 8x+2y=30\\n' | $<TARGET_FILE:calc> | grep -q 'x = 6'")