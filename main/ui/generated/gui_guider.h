/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#ifndef GUI_GUIDER_H
#define GUI_GUIDER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"


typedef struct
{
  
	lv_obj_t *general;
	bool general_del;
	lv_obj_t *general_arc_rpm;
	lv_obj_t *general_arc_speed;
	lv_obj_t *general_arc_oil;
	lv_obj_t *general_arc_energy;
	lv_obj_t *general_cont_1;
	lv_obj_t *general_label_rpm_number;
	lv_obj_t *general_label_time;
	lv_obj_t *general_label_temp;
	lv_obj_t *general_slider_energy;
	lv_obj_t *general_label_speed_number;
	lv_obj_t *general_label_energy_number;
	lv_obj_t *general_label_energy_number_2;
	lv_obj_t *general_label_oil_number;
	lv_obj_t *general_label_trip;
	lv_obj_t *general_img_temp;
	lv_obj_t *general_label_1;
	lv_obj_t *general_label_2;
	lv_obj_t *general_label_3;
	lv_obj_t *general_label_4;
	lv_obj_t *general_label_5;
	lv_obj_t *general_label_6;
	lv_obj_t *general_label_unit_kw;
	lv_obj_t *race;
	bool race_del;
	lv_obj_t *race_img_1;
	lv_obj_t *race_label_top;
	lv_obj_t *race_label_down;
	lv_obj_t *race_label_left;
	lv_obj_t *race_label_right;
	lv_obj_t *race_arc_4;
	lv_obj_t *race_arc_5;
	lv_obj_t *race_arc_6;
	lv_obj_t *race_arc_7;
	lv_obj_t *race_label_speed_number;
	lv_obj_t *race_label_kw;
	lv_obj_t *race_label_energy_number;
	lv_obj_t *race_label_time;
	lv_obj_t *race_img_2;
	lv_obj_t *race_img_3;
	lv_obj_t *race_img_4;
	lv_obj_t *race_img_5;
	lv_obj_t *race_label_1;
	lv_obj_t *race_label_2;
	lv_obj_t *setting;
	bool setting_del;
	lv_obj_t *setting_label_1;
	lv_obj_t *setting_btn_2;
	lv_obj_t *setting_btn_2_label;
	lv_obj_t *setting_btn_3;
	lv_obj_t *setting_btn_3_label;
	lv_obj_t *setting_cont_1;
	lv_obj_t *bluetooth;
	bool bluetooth_del;
	lv_obj_t *bluetooth_bt_label_title;
	lv_obj_t *bluetooth_bt_list_devices;
	lv_obj_t *bluetooth_bt_list_devices_item0;
	lv_obj_t *bluetooth_bt_btn_scan;
	lv_obj_t *bluetooth_bt_btn_scan_label;
	lv_obj_t *bluetooth_btn_back;
	lv_obj_t *bluetooth_btn_back_label;
	lv_obj_t *bluetooth_bt_sw_enable;
}lv_ui;

typedef void (*ui_setup_scr_t)(lv_ui * ui);

void ui_init_style(lv_style_t * style);

void ui_load_scr_animation(lv_ui *ui, lv_obj_t ** new_scr, bool new_scr_del, bool * old_scr_del, ui_setup_scr_t setup_scr,
                           lv_screen_load_anim_t anim_type, uint32_t time, uint32_t delay, bool is_clean, bool auto_del);

void ui_animation(void * var, uint32_t duration, int32_t delay, int32_t start_value, int32_t end_value, lv_anim_path_cb_t path_cb,
                  uint32_t repeat_cnt, uint32_t repeat_delay, uint32_t playback_time, uint32_t playback_delay,
                  lv_anim_exec_xcb_t exec_cb, lv_anim_start_cb_t start_cb, lv_anim_completed_cb_t ready_cb, lv_anim_deleted_cb_t deleted_cb);


void init_scr_del_flag(lv_ui *ui);

void setup_bottom_layer(void);

void setup_ui(lv_ui *ui);

void video_play(lv_ui *ui);

void init_keyboard(lv_ui *ui);

extern lv_ui guider_ui;


void setup_scr_general(lv_ui *ui);
void setup_scr_race(lv_ui *ui);
void setup_scr_setting(lv_ui *ui);
void setup_scr_bluetooth(lv_ui *ui);
LV_IMAGE_DECLARE(_tempunit_RGB565A8_20x19);
LV_IMAGE_DECLARE(_Gforce_RGB565A8_320x320);
LV_IMAGE_DECLARE(_o_RGB565A8_20x18);
LV_IMAGE_DECLARE(_e_RGB565A8_20x20);
LV_IMAGE_DECLARE(_tempunit_RGB565A8_16x14);

LV_FONT_DECLARE(lv_font_montserratMedium_26)
LV_FONT_DECLARE(lv_font_montserratMedium_32)
LV_FONT_DECLARE(lv_font_montserratMedium_24)
LV_FONT_DECLARE(lv_font_montserratMedium_72)
LV_FONT_DECLARE(lv_font_montserratMedium_68)
LV_FONT_DECLARE(lv_font_montserratMedium_20)
LV_FONT_DECLARE(lv_font_montserratMedium_18)
LV_FONT_DECLARE(lv_font_montserratMedium_36)
LV_FONT_DECLARE(lv_font_montserratMedium_16)


#ifdef __cplusplus
}
#endif
#endif
