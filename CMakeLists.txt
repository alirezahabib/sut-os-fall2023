cmake_minimum_required(VERSION 3.26)
project(sut_os_fall2023 C)

set(CMAKE_C_STANDARD 11)

include_directories(P1/hw1)

add_executable(sut_os_fall2023
        P1/hw1/io.c
        P1/hw1/io.h
        P1/hw1/main.c
        P1/hw1/parse.c
        P1/hw1/parse.h
        P1/hw1/process.c
        P1/hw1/process.h
        P1/hw1/shell.c
        P1/hw1/shell.h
        P2/httpserver.c)
