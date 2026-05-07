cmake_minimum_required(VERSION 3.30)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(udp-tools)

# rest of your project
set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(udp-client)
add_executable(udp-server)

target_sources(udp-client PUBLIC
${SRC}/send.c
)
target_sources(udp-server PUBLIC
${SRC}/receive.c
)

target_include_directories(udp-client PUBLIC
  ${SRC}
)
target_include_directories(udp-server PUBLIC
  ${SRC}
)

target_link_libraries(udp-server)
target_link_libraries(udp-client)
