#ifndef EEZ_LVGL_UI_IMAGES_H
#define EEZ_LVGL_UI_IMAGES_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_img_dsc_t img_tazzari;
extern const lv_img_dsc_t img_vollicht;
extern const lv_img_dsc_t img_abblendlicht;
extern const lv_img_dsc_t img_drl;
extern const lv_img_dsc_t img_rueckfahrlicht;
extern const lv_img_dsc_t img_ruecklicht;
extern const lv_img_dsc_t img_nebellicht;
extern const lv_img_dsc_t img_icon_vollicht;
extern const lv_img_dsc_t img_icon_abblendlicht;
extern const lv_img_dsc_t img_icon_drl;
extern const lv_img_dsc_t img_icon_blinker;
extern const lv_img_dsc_t img_icon_nebellicht_hinten;
extern const lv_img_dsc_t img_icon_nebellicht_vorne;
extern const lv_img_dsc_t img_icon_handbremse;
extern const lv_img_dsc_t img_icon_creak;
extern const lv_img_dsc_t img_icon_abblendlicht_cor;
extern const lv_img_dsc_t img_bat;
extern const lv_img_dsc_t img_stop;
extern const lv_img_dsc_t img_play;
extern const lv_img_dsc_t img_skip;
extern const lv_img_dsc_t img_ext;

#ifndef EXT_IMG_DESC_T
#define EXT_IMG_DESC_T
typedef struct _ext_img_desc_t {
    const char *name;
    const lv_img_dsc_t *img_dsc;
} ext_img_desc_t;
#endif

extern const ext_img_desc_t images[21];


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_IMAGES_H*/