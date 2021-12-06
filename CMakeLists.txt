cmake_minimum_required(VERSION 3.20)
project(fat12 C)

set(CMAKE_C_STANDARD 11)

add_executable(fat12 main.c file_reader.c file_reader.h)
