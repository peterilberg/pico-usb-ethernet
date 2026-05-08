cmake_minimum_required(VERSION 3.30)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)

project(tools)

# rest of your project
set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(echo)
add_executable(health)

target_sources(echo PUBLIC
${SRC}/echo.c
)
target_sources(health PUBLIC
${SRC}/health.c
)

target_include_directories(echo PUBLIC
  ${SRC}
)
target_include_directories(health PUBLIC
  ${SRC}
)

target_link_libraries(echo)
target_link_libraries(health)
