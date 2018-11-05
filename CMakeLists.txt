cmake_minimum_required(VERSION 3.12)
project(LightCC C)

set(CMAKE_C_STANDARD 99)
add_definitions(-D_BSD_SOURCE)

include_directories(include)

set(LIGHTCC
        include/lcc_array.h
        include/lcc_builtin_endians.i
        include/lcc_builtin_limits.i
        include/lcc_builtin_sizes.i
        include/lcc_builtin_types.i
        include/lcc_lexer.h
        include/lcc_map.h
        include/lcc_set.h
        include/lcc_string.h
        include/lcc_string_array.h
        include/lcc_utils.h
        src/lcc_array.c
        src/lcc_lexer.c
        src/lcc_map.c
        src/lcc_string.c
        src/lcc_string_array.c)

add_executable(lcc main.c ${LIGHTCC})
