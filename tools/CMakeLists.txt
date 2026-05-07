cmake_minimum_required(VERSION 3.30)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(tools)

# rest of your project
set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(send)
add_executable(receive)

target_sources(send PUBLIC
${SRC}/send.c
)
target_sources(receive PUBLIC
${SRC}/receive.c
)

target_include_directories(send PUBLIC
  ${SRC}
)
target_include_directories(receive PUBLIC
  ${SRC}
)

target_link_libraries(receive)
target_link_libraries(send)
