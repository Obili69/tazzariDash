cmake_minimum_required(VERSION 3.16)
project(LVGLDashboard)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build configuration options
option(DEPLOYMENT_BUILD "Build for deployment (fullscreen)" OFF)
option(ENABLE_DEBUG_OUTPUT "Enable debug console output" ON)
option(ENABLE_SIMPLE_AUDIO "Enable SimplifiedAudioManager" ON)

# Display build configuration
if(DEPLOYMENT_BUILD)
    message(STATUS "Building for DEPLOYMENT - Fullscreen mode enabled")
    add_compile_definitions(DEPLOYMENT_BUILD)
else()
    message(STATUS "Building for DEVELOPMENT - Windowed mode enabled")
endif()

if(ENABLE_DEBUG_OUTPUT)
    message(STATUS "Debug output enabled")
    add_compile_definitions(ENABLE_DEBUG_OUTPUT)
endif()

if(ENABLE_SIMPLE_AUDIO)
    message(STATUS "SimplifiedAudioManager enabled")
    add_compile_definitions(ENABLE_SIMPLE_AUDIO)
endif()

# Find required packages
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
    src/SerialCommunication.cpp
    ${UI_SOURCES}
)

# Add SimplifiedAudioManager if enabled
if(ENABLE_SIMPLE_AUDIO)
    list(APPEND SOURCES src/SimplifiedAudioManager.cpp)
endif()

# Executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    lvgl
    pthread
    ${SDL2_LIBRARIES}
    ${ALSA_LIBRARIES}
)

target_compile_options(${PROJECT_NAME} PRIVATE ${SDL2_CFLAGS_OTHER})

# Set output names based on build type
if(DEPLOYMENT_BUILD)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "LVGLDashboard_deployment")
else()
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "LVGLDashboard_dev")
endif()

# Install target (optional)
install(TARGETS ${PROJECT_NAME} DESTINATION bin)