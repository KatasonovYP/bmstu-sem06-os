cmake_minimum_required(VERSION 3.22.0)
project(socket fs)

set(CMAKE_C_STANDARD 11)

add_executable(task_00.out task_00_socket_pair/pair.c)

add_executable(task_01_server.out task_01_file_without_response/server.c)
add_executable(task_01_client.out task_01_file_without_response/client.c)

add_executable(task_02_server.out task_02_file_with_response/server.c)
add_executable(task_02_client.out task_02_file_with_response/client.c)
