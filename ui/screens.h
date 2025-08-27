#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *arc_volume;
    lv_obj_t *bar_soc;
    lv_obj_t *bass;
    lv_obj_t *btn_back;
    lv_obj_t *btn_play;
    lv_obj_t *btn_skip;
    lv_obj_t *cht_pwusage;
    lv_obj_t *high;
    lv_obj_t *img_album;
    lv_obj_t *img_drl;
    lv_obj_t *img_fogrear;
    lv_obj_t *img_highbeam;
    lv_obj_t *img_icon_bat;
    lv_obj_t *img_icon_break;
    lv_obj_t *img_icon_drl;
    lv_obj_t *img_icon_fog_rear;
    lv_obj_t *img_icon_highbeam;
    lv_obj_t *img_icon_ind_left;
    lv_obj_t *img_icon_ind_right;
    lv_obj_t *img_icon_light;
    lv_obj_t *img_icon_lowbeam;
    lv_obj_t *img_icon_park;
    lv_obj_t *img_lowbeam;
    lv_obj_t *img_rearlight;
    lv_obj_t *img_reverselight;
    lv_obj_t *lbl_gear_d;
    lv_obj_t *lbl_gear_n;
    lv_obj_t *lbl_gear_r;
    lv_obj_t *lbl_odo;
    lv_obj_t *lbl_soc;
    lv_obj_t *lbl_speed;
    lv_obj_t *lbl_temp_min_max;
    lv_obj_t *lbl_trip;
    lv_obj_t *lbl_volt_min_max;
    lv_obj_t *mid;
    lv_obj_t *play;
    lv_obj_t *sld_base;
    lv_obj_t *sld_high;
    lv_obj_t *sld_mid;
} objects_t;

extern objects_t objects;

enum ScreensEnum {
    SCREEN_ID_MAIN = 1,
};

void create_screen_main();
void tick_screen_main();

void create_screens();
void tick_screen(int screen_index);


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/