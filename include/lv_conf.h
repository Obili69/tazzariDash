#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 32
#define LV_COLOR_CHROMA_KEY lv_color_hex(0x00ff00)

// Memory settings
#define LV_MEM_SIZE (256 * 1024U)  // 256KB for Ubuntu

// Display settings
#define LV_DPI_DEF 100
#define LV_USE_PERF_MONITOR 1
#define LV_USE_MEM_MONITOR 1

// Enable features your UI uses
#define LV_USE_LABEL 1
#define LV_USE_IMG 1
#define LV_USE_BAR 1
#define LV_USE_CHART 1
#define LV_USE_SLIDER 1
#define LV_USE_BUTTON 1
#define LV_USE_BUTTONMATRIX 1
#define LV_USE_ARC 1

// File system (for images/assets)
#define LV_USE_FS_STDIO 1

// Enable SDL2 driver
#define LV_USE_SDL 1

// SDL2 Configuration - conditional based on build type
#ifdef DEPLOYMENT_BUILD
    #define LV_SDL_RESOLUTION_H 0    // 0 = use desktop resolution
    #define LV_SDL_RESOLUTION_V 0    // 0 = use desktop resolution
    #define LV_SDL_FULLSCREEN 1      // Enable fullscreen
    #define LV_SDL_DIRECT_EXIT 1     // Exit on window close
#else
    #define LV_SDL_RESOLUTION_H 1024 // Fixed window size for development
    #define LV_SDL_RESOLUTION_V 600
    #define LV_SDL_FULLSCREEN 0      // Windowed mode
    #define LV_SDL_DIRECT_EXIT 1     // Exit on window close
#endif

// Logging
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_INFO

// Threading
#define LV_USE_OS LV_OS_PTHREAD

// FONT CONFIGURATION - Enable all Montserrat fonts your UI needs
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 0
#define LV_FONT_MONTSERRAT_14 1  // Default (already enabled)
#define LV_FONT_MONTSERRAT_16 1  // Enable 16pt
#define LV_FONT_MONTSERRAT_18 1  // Enable 18pt
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 1  // Enable 24pt
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 1  // Enable 48pt

#endif
