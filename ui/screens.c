#include <string.h>

#include "screens.h"
#include "images.h"
#include "fonts.h"
#include "actions.h"
#include "vars.h"
#include "styles.h"
#include "ui.h"

#include <string.h>

objects_t objects;
lv_obj_t *tick_value_change_obj;

void create_screen_main() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.main = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1024, 600);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_img_create(parent_obj);
            lv_obj_set_pos(obj, 454, 238);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_tazzari);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 551, 67);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "km/h");
        }
        {
            // lblOdo
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_odo = obj;
            lv_obj_set_pos(obj, 64, 248);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "999999");
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_LEFT_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 20, 543);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "ODO");
        }
        {
            // imgHighbeam
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.img_highbeam = obj;
            lv_obj_set_pos(obj, 322, 263);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_vollicht);
        }
        {
            // imgLowbeam
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.img_lowbeam = obj;
            lv_obj_set_pos(obj, 369, 263);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_abblendlicht);
        }
        {
            // imgDrl
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.img_drl = obj;
            lv_obj_set_pos(obj, 391, 260);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_drl);
        }
        {
            // imgRearlight
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.img_rearlight = obj;
            lv_obj_set_pos(obj, 912, 258);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_ruecklicht);
        }
        {
            // imgReverselight
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.img_reverselight = obj;
            lv_obj_set_pos(obj, 912, 260);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_rueckfahrlicht);
        }
        {
            // imgFogrear
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.img_fogrear = obj;
            lv_obj_set_pos(obj, 912, 199);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_nebellicht);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 20, 567);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "TRIP");
        }
        {
            // lblTrip
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_trip = obj;
            lv_obj_set_pos(obj, 64, 562);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "9999");
            lv_obj_add_event_cb(obj, action_set_global_eez_event, LV_EVENT_PRESSED, (void *)0);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 9, 59);
            lv_obj_set_size(obj, 50, 50);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // imgIconIndLeft
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.img_icon_ind_left = obj;
            lv_obj_set_pos(obj, 379, 27);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_icon_blinker);
        }
        {
            // imgIconIndRight
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.img_icon_ind_right = obj;
            lv_obj_set_pos(obj, 612, 27);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_icon_blinker);
            lv_img_set_angle(obj, 1800);
        }
        {
            // imgIconLowbeam
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.img_icon_lowbeam = obj;
            lv_obj_set_pos(obj, 25, 84);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_icon_abblendlicht_cor);
        }
        {
            // imgIconHighbeam
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.img_icon_highbeam = obj;
            lv_obj_set_pos(obj, 25, 20);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_icon_vollicht);
        }
        {
            // imgIconLight
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.img_icon_light = obj;
            lv_obj_set_pos(obj, 90, 89);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_icon_abblendlicht);
        }
        {
            // imgIconFogRear
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.img_icon_fog_rear = obj;
            lv_obj_set_pos(obj, 954, 16);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_icon_nebellicht_hinten);
        }
        {
            // imgIconPark
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.img_icon_park = obj;
            lv_obj_set_pos(obj, 954, 81);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_icon_handbremse);
        }
        {
            // imgIconDrl
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.img_icon_drl = obj;
            lv_obj_set_pos(obj, 90, 20);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_icon_drl);
        }
        {
            // lblGearD
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_gear_d = obj;
            lv_obj_set_pos(obj, 493, 104);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "D");
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_opa(obj, 70, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // chtPwusage
            lv_obj_t *obj = lv_chart_create(parent_obj);
            objects.cht_pwusage = obj;
            lv_obj_set_pos(obj, 368, 493);
            lv_obj_set_size(obj, 288, 100);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0xff15171a), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(0xff15171a), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // barSoc
            lv_obj_t *obj = lv_bar_create(parent_obj);
            objects.bar_soc = obj;
            lv_obj_set_pos(obj, 597, 459);
            lv_obj_set_size(obj, 200, 27);
            lv_bar_set_value(obj, 25, LV_ANIM_OFF);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0xff00ff3d), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0xff44925e), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        }
        {
            // lblSoc
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_soc = obj;
            lv_obj_set_pos(obj, 672, 459);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "20%");
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // lblSpeed
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_speed = obj;
            lv_obj_set_pos(obj, 0, -238);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "102");
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // imgIconBreak
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.img_icon_break = obj;
            lv_obj_set_pos(obj, 889, 81);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_icon_creak);
        }
        {
            // lblVoltMinMax
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_volt_min_max = obj;
            lv_obj_set_pos(obj, 906, 538);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "2.31V/3.02V");
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // lblTempMinMax
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_temp_min_max = obj;
            lv_obj_set_pos(obj, 906, 564);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "2.31V/3.02V");
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // ImgIconBat
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.img_icon_bat = obj;
            lv_obj_set_pos(obj, 889, 11);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_bat);
        }
        {
            // lblGearN
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_gear_n = obj;
            lv_obj_set_pos(obj, 494, 156);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "N");
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // lblGearR
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_gear_r = obj;
            lv_obj_set_pos(obj, 496, 208);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "R");
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_opa(obj, 70, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            lv_obj_t *obj = lv_tabview_create(parent_obj);
            lv_tabview_set_tab_bar_position(obj, LV_DIR_TOP);
            lv_tabview_set_tab_bar_size(obj, 32);
            lv_obj_set_pos(obj, 25, 188);
            lv_obj_set_size(obj, 284, 339);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // Play
                    lv_obj_t *obj = lv_tabview_add_tab(parent_obj, "PLAY");
                    objects.play = obj;
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // imgAlbum
                            lv_obj_t *obj = lv_img_create(parent_obj);
                            objects.img_album = obj;
                            lv_obj_set_pos(obj, 49, 35);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_img_set_src(obj, &img_ext);
                            lv_img_set_zoom(obj, 255);
                        }
                        {
                            // arcVolume
                            lv_obj_t *obj = lv_arc_create(parent_obj);
                            objects.arc_volume = obj;
                            lv_obj_set_pos(obj, -4, -10);
                            lv_obj_set_size(obj, 247, 243);
                            lv_arc_set_value(obj, 25);
                            lv_arc_set_bg_end_angle(obj, 60);
                            lv_obj_add_event_cb(obj, action_set_global_eez_event, LV_EVENT_VALUE_CHANGED, (void *)0);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(0xff44925e), LV_PART_KNOB | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(0xff44925e), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_arc_color(obj, lv_color_hex(0xff44925e), LV_PART_INDICATOR | LV_STATE_DEFAULT);
                        }
                        {
                            // btnPlay
                            lv_obj_t *obj = lv_img_create(parent_obj);
                            objects.btn_play = obj;
                            lv_obj_set_pos(obj, 110, 234);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_img_set_src(obj, &img_play);
                            lv_obj_add_event_cb(obj, action_set_global_eez_event, LV_EVENT_PRESSED, (void *)0);
                            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                        }
                        {
                            // btnSkip
                            lv_obj_t *obj = lv_img_create(parent_obj);
                            objects.btn_skip = obj;
                            lv_obj_set_pos(obj, 174, 230);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_img_set_src(obj, &img_skip);
                            lv_obj_add_event_cb(obj, action_set_global_eez_event, LV_EVENT_PRESSED, (void *)0);
                            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                        }
                        {
                            // btnBack
                            lv_obj_t *obj = lv_img_create(parent_obj);
                            objects.btn_back = obj;
                            lv_obj_set_pos(obj, 33, 230);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_img_set_src(obj, &img_skip);
                            lv_img_set_angle(obj, 1800);
                            lv_obj_add_event_cb(obj, action_set_global_eez_event, LV_EVENT_PRESSED, (void *)0);
                            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_tabview_add_tab(parent_obj, "EQ");
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // sldBase
                            lv_obj_t *obj = lv_slider_create(parent_obj);
                            objects.sld_base = obj;
                            lv_obj_set_pos(obj, 31, 28);
                            lv_obj_set_size(obj, 183, 15);
                            lv_slider_set_value(obj, 25, LV_ANIM_OFF);
                            lv_obj_add_event_cb(obj, action_set_global_eez_event, LV_EVENT_VALUE_CHANGED, (void *)0);
                        }
                        {
                            // BASS
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.bass = obj;
                            lv_obj_set_pos(obj, 32, -6);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_label_set_text(obj, "Bass");
                            lv_obj_set_style_text_font(obj, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        {
                            // MID
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.mid = obj;
                            lv_obj_set_pos(obj, 32, 60);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_label_set_text(obj, "Mitte");
                            lv_obj_set_style_text_font(obj, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        {
                            // sldMid
                            lv_obj_t *obj = lv_slider_create(parent_obj);
                            objects.sld_mid = obj;
                            lv_obj_set_pos(obj, 31, 91);
                            lv_obj_set_size(obj, 183, 15);
                            lv_slider_set_value(obj, 25, LV_ANIM_OFF);
                            lv_obj_add_event_cb(obj, action_set_global_eez_event, LV_EVENT_VALUE_CHANGED, (void *)0);
                        }
                        {
                            // HIGH
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.high = obj;
                            lv_obj_set_pos(obj, 33, 125);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_label_set_text(obj, "Hoch");
                            lv_obj_set_style_text_font(obj, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        {
                            // sldHigh
                            lv_obj_t *obj = lv_slider_create(parent_obj);
                            objects.sld_high = obj;
                            lv_obj_set_pos(obj, 31, 154);
                            lv_obj_set_size(obj, 183, 15);
                            lv_slider_set_value(obj, 25, LV_ANIM_OFF);
                            lv_obj_add_event_cb(obj, action_set_global_eez_event, LV_EVENT_VALUE_CHANGED, (void *)0);
                        }
                    }
                }
            }
        }
    }
}

void tick_screen_main() {
}


void create_screens() {
    lv_disp_t *dispp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), true, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);
    
    create_screen_main();
}

typedef void (*tick_screen_func_t)();

tick_screen_func_t tick_screen_funcs[] = {
    tick_screen_main,
};

void tick_screen(int screen_index) {
    tick_screen_funcs[screen_index]();
}
