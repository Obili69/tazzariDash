cmake_minimum_required(VERSION 3.16)
project(LVGLDashboard)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build configuration options
option(DEPLOYMENT_BUILD "Build for deployment (fullscreen)" OFF)
option(ENABLE_DEBUG_OUTPUT "Enable debug console output" ON)

# Audio hardware selection (mutually exclusive)
set(AUDIO_HARDWARE "BEOCREATE4" CACHE STRING "Audio hardware type")
set_property(CACHE AUDIO_HARDWARE PROPERTY STRINGS "AUX" "DAC" "AMP4" "BEOCREATE4")

# Validate audio hardware selection
if(NOT AUDIO_HARDWARE MATCHES "^(AUX|DAC|AMP4|BEOCREATE4)$")
    message(FATAL_ERROR "Invalid AUDIO_HARDWARE value: ${AUDIO_HARDWARE}. Must be AUX, DAC, AMP4, or BEOCREATE4")
endif()

# Display build configuration
message(STATUS "=== TazzariAudio Dashboard Build Configuration ===")
if(DEPLOYMENT_BUILD)
    message(STATUS "Build Mode: DEPLOYMENT - Fullscreen mode enabled")
    add_compile_definitions(DEPLOYMENT_BUILD)
else()
    message(STATUS "Build Mode: DEVELOPMENT - Windowed mode enabled")
endif()

message(STATUS "Audio Hardware: ${AUDIO_HARDWARE}")
if(AUDIO_HARDWARE STREQUAL "AUX")
    add_compile_definitions(AUDIO_HARDWARE_AUX)
    message(STATUS "  - Built-in 3.5mm jack with PulseAudio")
    message(STATUS "  - Software volume control")
    message(STATUS "  - Software EQ (alsaeq)")
elseif(AUDIO_HARDWARE STREQUAL "DAC")
    add_compile_definitions(AUDIO_HARDWARE_DAC)
    message(STATUS "  - HiFiBerry DAC+ (external amplifier required)")
    message(STATUS "  - Hardware volume control via ALSA")
    message(STATUS "  - Software EQ (alsaeq)")
elseif(AUDIO_HARDWARE STREQUAL "AMP4")
    add_compile_definitions(AUDIO_HARDWARE_AMP4)
    message(STATUS "  - HiFiBerry AMP4 (built-in amplifier)")
    message(STATUS "  - Hardware volume control via ALSA")
    message(STATUS "  - Software EQ (alsaeq)")
elseif(AUDIO_HARDWARE STREQUAL "BEOCREATE4")
    add_compile_definitions(AUDIO_HARDWARE_BEOCREATE4)
    message(STATUS "  - HiFiBerry BeoCreate 4 DSP (4-channel amplifier)")
    message(STATUS "  - Hardware volume control via REST API")
    message(STATUS "  - Hardware EQ via DSP biquad filters")
endif()

if(ENABLE_DEBUG_OUTPUT)
    message(STATUS "Debug output: ENABLED")
    add_compile_definitions(ENABLE_DEBUG_OUTPUT)
else()
    message(STATUS "Debug output: DISABLED")
endif()
message(STATUS "==================================================")

# Find required packages (common)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

# Audio hardware specific dependencies
if(AUDIO_HARDWARE STREQUAL "AUX")
    # PulseAudio for built-in audio
    pkg_check_modules(PULSE REQUIRED libpulse)
    set(AUDIO_LIBS ${PULSE_LIBRARIES})
    set(AUDIO_INCLUDE_DIRS ${PULSE_INCLUDE_DIRS})
    
elseif(AUDIO_HARDWARE STREQUAL "DAC" OR AUDIO_HARDWARE STREQUAL "AMP4")
    # ALSA for HiFiBerry DAC+/AMP4
    pkg_check_modules(ALSA REQUIRED alsa)
    set(AUDIO_LIBS ${ALSA_LIBRARIES})
    set(AUDIO_INCLUDE_DIRS ${ALSA_INCLUDE_DIRS})
    
elseif(AUDIO_HARDWARE STREQUAL "BEOCREATE4")
    # cURL for BeoCreate 4 REST API
    pkg_check_modules(CURL REQUIRED libcurl)
    set(AUDIO_LIBS ${CURL_LIBRARIES})
    set(AUDIO_INCLUDE_DIRS ${CURL_INCLUDE_DIRS})
endif()

# LVGL setup
set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/include/lv_conf.h CACHE STRING "" FORCE)
add_subdirectory(lvgl)

# Include directories
include_directories(
    include/
    ui/
    ${SDL2_INCLUDE_DIRS}
    ${AUDIO_INCLUDE_DIRS}
)

# Source files
file(GLOB_RECURSE UI_SOURCES "ui/*.c" "ui/*.cpp")

set(SOURCES
    src/main.cpp
    src/SerialCommunication.cpp
    src/MultiAudioManager.cpp  # New unified audio manager
)

# Add UI sources
list(APPEND SOURCES ${UI_SOURCES})

# Executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    lvgl
    pthread
    ${SDL2_LIBRARIES}
    ${AUDIO_LIBS}
)

target_compile_options(${PROJECT_NAME} PRIVATE 
    ${SDL2_CFLAGS_OTHER}
)

# Set output names based on build type and audio hardware
if(DEPLOYMENT_BUILD)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "LVGLDashboard_${AUDIO_HARDWARE}_deployment")
else()
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "LVGLDashboard_${AUDIO_HARDWARE}_dev")
endif()

# Install target (optional)
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# Print final summary
message(STATUS "Build will create: ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}")
message(STATUS "Audio dependencies: ${AUDIO_LIBS}")
if(AUDIO_HARDWARE STREQUAL "AUX")
    message(STATUS "Required packages: sudo apt install libpulse-dev libasound2-plugin-equal")
elseif(AUDIO_HARDWARE STREQUAL "DAC" OR AUDIO_HARDWARE STREQUAL "AMP4") 
    message(STATUS "Required packages: sudo apt install libasound2-dev libasound2-plugin-equal")
elseif(AUDIO_HARDWARE STREQUAL "BEOCREATE4")
    message(STATUS "Required packages: sudo apt install libcurl4-openssl-dev hifiberry-dsp")
endif()