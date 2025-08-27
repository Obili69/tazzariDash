cmake_minimum_required(VERSION 3.16)
project(LVGLDashboard)

set(CMAKE_CXX_STANDARD 17)

# Find SDL2
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(ALSA REQUIRED alsa)

# LVGL setup
set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/include/lv_conf.h CACHE STRING "" FORCE)
add_subdirectory(lvgl)

# Include directories
include_directories(
    include/
    ui/
    ${SDL2_INCLUDE_DIRS}
)

# Source files
file(GLOB_RECURSE UI_SOURCES "ui/*.c" "ui/*.cpp")
set(SOURCES
    src/main.cpp
    ${UI_SOURCES}
)

# Executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries - removed bluetooth for now
target_link_libraries(${PROJECT_NAME}
    lvgl
    pthread
    ${SDL2_LIBRARIES}
    ${ALSA_LIBRARIES}
)

target_compile_options(${PROJECT_NAME} PRIVATE ${SDL2_CFLAGS_OTHER})
