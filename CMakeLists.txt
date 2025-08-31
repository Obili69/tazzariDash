cmake_minimum_required(VERSION 3.16)
project(LVGLDashboard)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build configuration options
option(DEPLOYMENT_BUILD "Build for deployment (fullscreen)" OFF)
option(ENABLE_DEBUG_OUTPUT "Enable debug console output" ON)
option(ENABLE_BLUETOOTH "Enable Bluetooth audio support" ON)

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

if(ENABLE_BLUETOOTH)
    message(STATUS "Bluetooth audio support enabled")
    add_compile_definitions(ENABLE_BLUETOOTH)
endif()

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(ALSA REQUIRED alsa)

# Bluetooth dependencies (with better error handling)
if(ENABLE_BLUETOOTH)
    pkg_check_modules(DBUS REQUIRED dbus-1)
    pkg_check_modules(PULSE REQUIRED libpulse)
    
    if(NOT DBUS_FOUND)
        message(FATAL_ERROR "D-Bus development libraries required for Bluetooth support. Install: sudo apt install libdbus-1-dev")
    endif()
    
    if(NOT PULSE_FOUND)
        message(FATAL_ERROR "PulseAudio development libraries required. Install: sudo apt install libpulse-dev")
    endif()
    
    message(STATUS "D-Bus version: ${DBUS_VERSION}")
    message(STATUS "PulseAudio version: ${PULSE_VERSION}")
endif()

# LVGL setup
set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/include/lv_conf.h CACHE STRING "" FORCE)
add_subdirectory(lvgl)

# Include directories
include_directories(
    include/
    ui/
    ${SDL2_INCLUDE_DIRS}
)

if(ENABLE_BLUETOOTH)
    include_directories(${DBUS_INCLUDE_DIRS} ${PULSE_INCLUDE_DIRS})
endif()

# Source files
file(GLOB_RECURSE UI_SOURCES "ui/*.c" "ui/*.cpp")
set(SOURCES
    src/main.cpp
    src/SerialCommunication.cpp
    ${UI_SOURCES}
)

# Add Bluetooth source conditionally
if(ENABLE_BLUETOOTH)
    list(APPEND SOURCES src/BluetoothAudioManager.cpp)
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

# Add Bluetooth libraries if available
if(ENABLE_BLUETOOTH)
    target_link_libraries(${PROJECT_NAME} 
        ${DBUS_LIBRARIES} 
        ${PULSE_LIBRARIES}
    )
endif()

target_compile_options(${PROJECT_NAME} PRIVATE ${SDL2_CFLAGS_OTHER})

# Set output names based on build type
if(DEPLOYMENT_BUILD)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "LVGLDashboard_deployment")
else()
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "LVGLDashboard_dev")
endif()

# Install target (optional)
install(TARGETS ${PROJECT_NAME} DESTINATION bin)