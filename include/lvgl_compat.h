#ifndef LVGL_COMPAT_H
#define LVGL_COMPAT_H

#include <lvgl.h>

// LVGL v8 → v9 API compatibility layer
#define lv_scr_load_anim lv_screen_load_anim

// Add other v8→v9 mappings as needed:
// #define lv_obj_set_style_local_text_font lv_obj_set_style_text_font
// #define lv_task_handler lv_timer_handler
// #define LV_EVENT_APPLY LV_EVENT_READY

#endif
