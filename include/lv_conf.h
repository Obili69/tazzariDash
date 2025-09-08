#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 32
#define LV_COLOR_CHROMA_KEY lv_color_hex(0x00ff00)

// Memory settings - optimized for deployment
#ifdef DEPLOYMENT_BUILD
    #define LV_MEM_SIZE (128 * 1024U)  // Smaller memory pool for deployment
#else
    #define LV_MEM_SIZE (256 * 1024U)  // Larger memory pool for development
#endif

// Display settings
#define LV_DPI_DEF 100

// Performance monitoring - only in development
#ifdef DEPLOYMENT_BUILD
    #define LV_USE_PERF_MONITOR 0    // Disable performance monitor in deployment
    #define LV_USE_MEM_MONITOR 0     // Disable memory monitor in deployment
#else
    #define LV_USE_PERF_MONITOR 1    // Enable in development
    #define LV_USE_MEM_MONITOR 1     // Enable in development
#endif

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

// SDL2 Configuration - NUTS FAST optimizations
#ifdef DEPLOYMENT_BUILD
    #define LV_SDL_RESOLUTION_H 0    // 0 = use desktop resolution
    #define LV_SDL_RESOLUTION_V 0    // 0 = use desktop resolution
    #define LV_SDL_FULLSCREEN 1      // Enable fullscreen
    #define LV_SDL_DIRECT_EXIT 1     // Exit on window close
    #define LV_SDL_MOUSE_CURSOR 0    // Hide mouse cursor
    #define LV_SDL_ACCELERATED 1     // Enable hardware acceleration
#else
    #define LV_SDL_RESOLUTION_H 1024 // Fixed window size for development
    #define LV_SDL_RESOLUTION_V 600
    #define LV_SDL_FULLSCREEN 0      // Windowed mode
    #define LV_SDL_DIRECT_EXIT 1     // Exit on window close
    #define LV_SDL_MOUSE_CURSOR 1    // Show mouse cursor for development
    #define LV_SDL_ACCELERATED 1     // Enable hardware acceleration
#endif

// Logging - optimized for deployment
#define LV_USE_LOG 1
#ifdef DEPLOYMENT_BUILD
    #define LV_LOG_LEVEL LV_LOG_LEVEL_ERROR  // Only errors in deployment
#else
    #define LV_LOG_LEVEL LV_LOG_LEVEL_INFO   // More verbose in development
#endif

// Threading
#define LV_USE_OS LV_OS_PTHREAD

// Animation settings - NUTS FAST optimizations
#ifdef DEPLOYMENT_BUILD
    #define LV_USE_DRAW_MASKS 0      // Disable complex drawing for speed
    #define LV_DRAW_COMPLEX 0        // Disable complex drawing effects
    #define LV_USE_GPU 1             // Enable GPU acceleration if available
    #define LV_DISP_DEF_REFR_PERIOD 33  // 30 FPS refresh rate (smooth but not wasteful)
#else
    #define LV_USE_DRAW_MASKS 1      // Enable in development for full features
    #define LV_DRAW_COMPLEX 1        // Enable complex drawing in development
    #define LV_USE_GPU 1             // Enable GPU acceleration
    #define LV_DISP_DEF_REFR_PERIOD 16  // 60 FPS refresh rate in development
#endif

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

// NUTS FAST: Disable unnecessary features in deployment
#ifdef DEPLOYMENT_BUILD
    #define LV_USE_ASSERT 0          // Disable assertions
    #define LV_USE_DEBUG 0           // Disable debug features
    #define LV_USE_PROFILER 0        // Disable profiler
    #define LV_TICK_CUSTOM 0         // Use standard ticking
    
    // Disable complex widgets not used in automotive dashboard
    #define LV_USE_CALENDAR 0
    #define LV_USE_COLORWHEEL 0
    #define LV_USE_IMGBTN 0
    #define LV_USE_LED 0
    #define LV_USE_LIST 0
    #define LV_USE_MENU 0
    #define LV_USE_METER 0
    #define LV_USE_MSGBOX 0
    #define LV_USE_SPINBOX 0
    #define LV_USE_SPINNER 0
    #define LV_USE_TABVIEW 0
    #define LV_USE_TILEVIEW 0
    #define LV_USE_WIN 0
    
    // Optimize theme for speed
    #define LV_THEME_DEFAULT_TRANSITION_TIME 0  // No transitions
    #define LV_USE_THEME_BASIC 1
    #define LV_USE_THEME_MONO 0
#else
    // Enable all features in development
    #define LV_USE_ASSERT 1
    #define LV_USE_DEBUG 1
    #define LV_USE_PROFILER 1
    
    // Enable additional widgets for development/testing
    #define LV_USE_CALENDAR 1
    #define LV_USE_COLORWHEEL 1
    #define LV_USE_IMGBTN 1
    #define LV_USE_LED 1
    #define LV_USE_LIST 1
    #define LV_USE_MENU 1
    #define LV_USE_METER 1
    #define LV_USE_MSGBOX 1
    #define LV_USE_SPINBOX 1
    #define LV_USE_SPINNER 1
    #define LV_USE_TABVIEW 1
    #define LV_USE_TILEVIEW 1
    #define LV_USE_WIN 1
    
    #define LV_THEME_DEFAULT_TRANSITION_TIME 200
    #define LV_USE_THEME_BASIC 1
    #define LV_USE_THEME_MONO 1
#endif

#endif