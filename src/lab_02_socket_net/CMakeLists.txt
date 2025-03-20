cmake_minimum_required(VERSION 3.22.0)
project(socket net)

set(CMAKE_C_STANDARD 11)

add_executable(task_01_server.out task_01_net_proc/server.c)
add_executable(task_01_client.out task_01_net_proc/client.c)

add_executable(task_02_server.out task_02_net_thread/server.c)
add_executable(task_02_client.out task_02_net_thread/client.c)

add_executable(task_03_server.out task_03_net_epoll/server.c)
add_executable(task_03_client.out task_03_net_epoll/client.c)
