/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"



void setup_scr_race(lv_ui *ui)
{
    //Write codes race
    ui->race = lv_obj_create(NULL);
    lv_obj_set_size(ui->race, 466, 466);
    lv_obj_set_scrollbar_mode(ui->race, LV_SCROLLBAR_MODE_OFF);

    //Write style for race, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->race, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->race, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->race, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes race_img_1
    ui->race_img_1 = lv_image_create(ui->race);
    lv_obj_set_pos(ui->race_img_1, 73, 73);
    lv_obj_set_size(ui->race_img_1, 320, 320);
    lv_obj_add_flag(ui->race_img_1, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(ui->race_img_1, &_Gforce_RGB565A8_320x320);
    lv_image_set_pivot(ui->race_img_1, 50,50);
    lv_image_set_rotation(ui->race_img_1, 0);

    //Write style for race_img_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_image_recolor_opa(ui->race_img_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_image_opa(ui->race_img_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes race_label_top
    ui->race_label_top = lv_label_create(ui->race);
    lv_obj_set_pos(ui->race_label_top, 200, 69);
    lv_obj_set_size(ui->race_label_top, 66, 19);
    lv_label_set_text(ui->race_label_top, "0.00");
    lv_label_set_long_mode(ui->race_label_top, LV_LABEL_LONG_WRAP);

    //Write style for race_label_top, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->race_label_top, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->race_label_top, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->race_label_top, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->race_label_top, &lv_font_OPPOSans_Medium_18, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->race_label_top, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->race_label_top, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->race_label_top, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->race_label_top, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->race_label_top, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->race_label_top, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->race_label_top, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->race_label_top, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->race_label_top, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->race_label_top, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes race_label_down
    ui->race_label_down = lv_label_create(ui->race);
    lv_obj_set_pos(ui->race_label_down, 200, 382);
    lv_obj_set_size(ui->race_label_down, 66, 19);
    lv_label_set_text(ui->race_label_down, "0.00");
    lv_label_set_long_mode(ui->race_label_down, LV_LABEL_LONG_WRAP);

    //Write style for race_label_down, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->race_label_down, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->race_label_down, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->race_label_down, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->race_label_down, &lv_font_OPPOSans_Medium_18, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->race_label_down, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->race_label_down, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->race_label_down, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->race_label_down, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->race_label_down, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->race_label_down, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->race_label_down, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->race_label_down, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->race_label_down, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->race_label_down, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes race_label_left
    ui->race_label_left = lv_label_create(ui->race);
    lv_obj_set_pos(ui->race_label_left, 44, 223);
    lv_obj_set_size(ui->race_label_left, 66, 19);
    lv_label_set_text(ui->race_label_left, "0.00");
    lv_label_set_long_mode(ui->race_label_left, LV_LABEL_LONG_WRAP);

    //Write style for race_label_left, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->race_label_left, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->race_label_left, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->race_label_left, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->race_label_left, &lv_font_OPPOSans_Medium_18, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->race_label_left, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->race_label_left, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->race_label_left, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->race_label_left, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->race_label_left, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->race_label_left, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->race_label_left, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->race_label_left, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->race_label_left, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->race_label_left, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes race_label_right
    ui->race_label_right = lv_label_create(ui->race);
    lv_obj_set_pos(ui->race_label_right, 359, 223);
    lv_obj_set_size(ui->race_label_right, 66, 19);
    lv_label_set_text(ui->race_label_right, "0.00");
    lv_label_set_long_mode(ui->race_label_right, LV_LABEL_LONG_WRAP);

    //Write style for race_label_right, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->race_label_right, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->race_label_right, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->race_label_right, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->race_label_right, &lv_font_OPPOSans_Medium_18, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->race_label_right, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->race_label_right, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->race_label_right, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->race_label_right, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->race_label_right, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->race_label_right, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->race_label_right, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->race_label_right, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->race_label_right, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->race_label_right, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes race_arc_4
    ui->race_arc_4 = lv_arc_create(ui->race);
    lv_obj_set_pos(ui->race_arc_4, 8, 8);
    lv_obj_set_size(ui->race_arc_4, 450, 450);
    lv_arc_set_mode(ui->race_arc_4, LV_ARC_MODE_REVERSE);
    lv_arc_set_range(ui->race_arc_4, 0, 200);
    lv_arc_set_bg_angles(ui->race_arc_4, 130, 180);
    lv_arc_set_value(ui->race_arc_4, 200);
    lv_arc_set_rotation(ui->race_arc_4, 180);

    //Write style for race_arc_4, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->race_arc_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->race_arc_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui->race_arc_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->race_arc_4, 6, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->race_arc_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->race_arc_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->race_arc_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->race_arc_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->race_arc_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for race_arc_4, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_arc_width(ui->race_arc_4, 15, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui->race_arc_4, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(ui->race_arc_4, lv_color_hex(0xff6500), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_rounded(ui->race_arc_4, true, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write style for race_arc_4, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->race_arc_4, 0, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->race_arc_4, 0, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes race_arc_5
    ui->race_arc_5 = lv_arc_create(ui->race);
    lv_obj_set_pos(ui->race_arc_5, 8, 8);
    lv_obj_set_size(ui->race_arc_5, 450, 450);
    lv_arc_set_mode(ui->race_arc_5, LV_ARC_MODE_REVERSE);
    lv_arc_set_range(ui->race_arc_5, 0, 200);
    lv_arc_set_bg_angles(ui->race_arc_5, 130, 180);
    lv_arc_set_value(ui->race_arc_5, 200);
    lv_arc_set_rotation(ui->race_arc_5, 0);

    //Write style for race_arc_5, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->race_arc_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->race_arc_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui->race_arc_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->race_arc_5, 6, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->race_arc_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->race_arc_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->race_arc_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->race_arc_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->race_arc_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for race_arc_5, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_arc_width(ui->race_arc_5, 15, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui->race_arc_5, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(ui->race_arc_5, lv_color_hex(0xff6500), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_rounded(ui->race_arc_5, true, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write style for race_arc_5, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->race_arc_5, 0, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->race_arc_5, 0, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes race_arc_6
    ui->race_arc_6 = lv_arc_create(ui->race);
    lv_obj_set_pos(ui->race_arc_6, 8, 8);
    lv_obj_set_size(ui->race_arc_6, 450, 450);
    lv_arc_set_mode(ui->race_arc_6, LV_ARC_MODE_NORMAL);
    lv_arc_set_range(ui->race_arc_6, 0, 200);
    lv_arc_set_bg_angles(ui->race_arc_6, 180, 230);
    lv_arc_set_value(ui->race_arc_6, 200);
    lv_arc_set_rotation(ui->race_arc_6, 0);

    //Write style for race_arc_6, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->race_arc_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->race_arc_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui->race_arc_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->race_arc_6, 6, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->race_arc_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->race_arc_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->race_arc_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->race_arc_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->race_arc_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for race_arc_6, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_arc_width(ui->race_arc_6, 15, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui->race_arc_6, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(ui->race_arc_6, lv_color_hex(0xff6500), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_rounded(ui->race_arc_6, true, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write style for race_arc_6, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->race_arc_6, 0, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->race_arc_6, 0, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes race_arc_7
    ui->race_arc_7 = lv_arc_create(ui->race);
    lv_obj_set_pos(ui->race_arc_7, 8, 8);
    lv_obj_set_size(ui->race_arc_7, 450, 450);
    lv_arc_set_mode(ui->race_arc_7, LV_ARC_MODE_NORMAL);
    lv_arc_set_range(ui->race_arc_7, 0, 200);
    lv_arc_set_bg_angles(ui->race_arc_7, 180, 230);
    lv_arc_set_value(ui->race_arc_7, 200);
    lv_arc_set_rotation(ui->race_arc_7, 180);

    //Write style for race_arc_7, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->race_arc_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->race_arc_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui->race_arc_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->race_arc_7, 6, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->race_arc_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->race_arc_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->race_arc_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->race_arc_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->race_arc_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for race_arc_7, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_arc_width(ui->race_arc_7, 15, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui->race_arc_7, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(ui->race_arc_7, lv_color_hex(0xff6500), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_rounded(ui->race_arc_7, true, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write style for race_arc_7, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->race_arc_7, 0, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->race_arc_7, 0, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes race_label_speed_number
    ui->race_label_speed_number = lv_label_create(ui->race);
    lv_obj_set_pos(ui->race_label_speed_number, 88, 122);
    lv_obj_set_size(ui->race_label_speed_number, 289, 71);
    lv_label_set_text(ui->race_label_speed_number, "00");
    lv_label_set_long_mode(ui->race_label_speed_number, LV_LABEL_LONG_WRAP);

    //Write style for race_label_speed_number, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->race_label_speed_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->race_label_speed_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->race_label_speed_number, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->race_label_speed_number, &lv_font_OPPOSans_Bold_72, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->race_label_speed_number, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->race_label_speed_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->race_label_speed_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->race_label_speed_number, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->race_label_speed_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->race_label_speed_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->race_label_speed_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->race_label_speed_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->race_label_speed_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->race_label_speed_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes race_label_kw
    ui->race_label_kw = lv_label_create(ui->race);
    lv_obj_set_pos(ui->race_label_kw, 213, 323);
    lv_obj_set_size(ui->race_label_kw, 40, 18);
    lv_label_set_text(ui->race_label_kw, "kw");
    lv_label_set_long_mode(ui->race_label_kw, LV_LABEL_LONG_WRAP);

    //Write style for race_label_kw, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->race_label_kw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->race_label_kw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->race_label_kw, lv_color_hex(0x999999), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->race_label_kw, &lv_font_OPPOSans_Medium_18, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->race_label_kw, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->race_label_kw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->race_label_kw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->race_label_kw, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->race_label_kw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->race_label_kw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->race_label_kw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->race_label_kw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->race_label_kw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->race_label_kw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes race_label_energy_number
    ui->race_label_energy_number = lv_label_create(ui->race);
    lv_obj_set_pos(ui->race_label_energy_number, 120, 260);
    lv_obj_set_size(ui->race_label_energy_number, 225, 72);
    lv_label_set_text(ui->race_label_energy_number, "-16.8");
    lv_label_set_long_mode(ui->race_label_energy_number, LV_LABEL_LONG_WRAP);

    //Write style for race_label_energy_number, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->race_label_energy_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->race_label_energy_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->race_label_energy_number, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->race_label_energy_number, &lv_font_OPPOSans_Bold_68, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->race_label_energy_number, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->race_label_energy_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->race_label_energy_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->race_label_energy_number, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->race_label_energy_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->race_label_energy_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->race_label_energy_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->race_label_energy_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->race_label_energy_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->race_label_energy_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes race_label_time
    ui->race_label_time = lv_label_create(ui->race);
    lv_obj_set_pos(ui->race_label_time, 173, 17);
    lv_obj_set_size(ui->race_label_time, 120, 32);
    lv_label_set_text(ui->race_label_time, "00:00");
    lv_label_set_long_mode(ui->race_label_time, LV_LABEL_LONG_WRAP);

    //Write style for race_label_time, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->race_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->race_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->race_label_time, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->race_label_time, &lv_font_OPPOSans_Medium_32, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->race_label_time, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->race_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->race_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->race_label_time, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->race_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->race_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->race_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->race_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->race_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->race_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes race_img_2
    ui->race_img_2 = lv_image_create(ui->race);
    lv_obj_set_pos(ui->race_img_2, 139, 421);
    lv_obj_set_size(ui->race_img_2, 20, 18);
    lv_obj_add_flag(ui->race_img_2, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(ui->race_img_2, &_o_RGB565A8_20x18);
    lv_image_set_pivot(ui->race_img_2, 50,50);
    lv_image_set_rotation(ui->race_img_2, 0);

    //Write style for race_img_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_image_recolor_opa(ui->race_img_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_image_opa(ui->race_img_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes race_img_3
    ui->race_img_3 = lv_image_create(ui->race);
    lv_obj_set_pos(ui->race_img_3, 240, 421);
    lv_obj_set_size(ui->race_img_3, 20, 20);
    lv_obj_add_flag(ui->race_img_3, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(ui->race_img_3, &_e_RGB565A8_20x20);
    lv_image_set_pivot(ui->race_img_3, 50,50);
    lv_image_set_rotation(ui->race_img_3, 0);

    //Write style for race_img_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_image_recolor_opa(ui->race_img_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_image_opa(ui->race_img_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes race_img_4
    ui->race_img_4 = lv_image_create(ui->race);
    lv_obj_set_pos(ui->race_img_4, 208, 423);
    lv_obj_set_size(ui->race_img_4, 16, 14);
    lv_obj_add_flag(ui->race_img_4, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(ui->race_img_4, &_tempunit_RGB565A8_16x14);
    lv_image_set_pivot(ui->race_img_4, 50,50);
    lv_image_set_rotation(ui->race_img_4, 0);

    //Write style for race_img_4, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_image_recolor_opa(ui->race_img_4, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_image_recolor(ui->race_img_4, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_image_opa(ui->race_img_4, 255, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes race_img_5
    ui->race_img_5 = lv_image_create(ui->race);
    lv_obj_set_pos(ui->race_img_5, 310, 422);
    lv_obj_set_size(ui->race_img_5, 16, 14);
    lv_obj_add_flag(ui->race_img_5, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(ui->race_img_5, &_tempunit_RGB565A8_16x14);
    lv_image_set_pivot(ui->race_img_5, 50,50);
    lv_image_set_rotation(ui->race_img_5, 0);

    //Write style for race_img_5, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_image_recolor_opa(ui->race_img_5, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_image_recolor(ui->race_img_5, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_image_opa(ui->race_img_5, 255, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes race_label_1
    ui->race_label_1 = lv_label_create(ui->race);
    lv_obj_set_pos(ui->race_label_1, 147, 422);
    lv_obj_set_size(ui->race_label_1, 55, 18);
    lv_label_set_text(ui->race_label_1, "16.5");
    lv_label_set_long_mode(ui->race_label_1, LV_LABEL_LONG_WRAP);

    //Write style for race_label_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->race_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->race_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->race_label_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->race_label_1, &lv_font_OPPOSans_Medium_18, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->race_label_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->race_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->race_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->race_label_1, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->race_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->race_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->race_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->race_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->race_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->race_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes race_label_2
    ui->race_label_2 = lv_label_create(ui->race);
    lv_obj_set_pos(ui->race_label_2, 248, 422);
    lv_obj_set_size(ui->race_label_2, 55, 18);
    lv_label_set_text(ui->race_label_2, "16.5");
    lv_label_set_long_mode(ui->race_label_2, LV_LABEL_LONG_WRAP);

    //Write style for race_label_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->race_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->race_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->race_label_2, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->race_label_2, &lv_font_OPPOSans_Medium_18, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->race_label_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->race_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->race_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->race_label_2, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->race_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->race_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->race_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->race_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->race_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->race_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes race_label_G_piont
    ui->race_label_G_piont = lv_label_create(ui->race);
    lv_obj_set_pos(ui->race_label_G_piont, 223, 223);
    lv_obj_set_size(ui->race_label_G_piont, 20, 20);
    lv_label_set_text(ui->race_label_G_piont, "");
    lv_label_set_long_mode(ui->race_label_G_piont, LV_LABEL_LONG_WRAP);

    //Write style for race_label_G_piont, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->race_label_G_piont, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->race_label_G_piont, 20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->race_label_G_piont, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->race_label_G_piont, &lv_font_OPPOSans_Medium_18, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->race_label_G_piont, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->race_label_G_piont, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->race_label_G_piont, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->race_label_G_piont, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->race_label_G_piont, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->race_label_G_piont, lv_color_hex(0xff6500), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->race_label_G_piont, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->race_label_G_piont, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->race_label_G_piont, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->race_label_G_piont, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->race_label_G_piont, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->race_label_G_piont, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //The custom code of race.


    //Update current screen layout.
    lv_obj_update_layout(ui->race);

}
