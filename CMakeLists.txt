cmake_minimum_required(VERSION 3.12)
project(LightCC C)

set(CMAKE_C_STANDARD 99)
include_directories(include)

set(LIGHTCC
        include/lcc_lexer.h
        include/lcc_string.h
        src/lcc_lexer.c
        src/lcc_string.c)

add_executable(lcc main.c ${LIGHTCC})
