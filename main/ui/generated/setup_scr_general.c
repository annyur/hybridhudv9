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



void setup_scr_general(lv_ui *ui)
{
    //Write codes general
    ui->general = lv_obj_create(NULL);
    lv_obj_set_size(ui->general, 466, 466);
    lv_obj_set_scrollbar_mode(ui->general, LV_SCROLLBAR_MODE_OFF);

    //Write style for general, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->general, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->general, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->general, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes general_arc_rpm
    ui->general_arc_rpm = lv_arc_create(ui->general);
    lv_obj_set_pos(ui->general_arc_rpm, 30, 30);
    lv_obj_set_size(ui->general_arc_rpm, 405, 405);
    lv_arc_set_mode(ui->general_arc_rpm, LV_ARC_MODE_NORMAL);
    lv_arc_set_range(ui->general_arc_rpm, 0, 8000);
    lv_arc_set_bg_angles(ui->general_arc_rpm, 140, 315);
    lv_arc_set_value(ui->general_arc_rpm, 0);
    lv_arc_set_rotation(ui->general_arc_rpm, 0);

    //Write style for general_arc_rpm, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->general_arc_rpm, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->general_arc_rpm, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui->general_arc_rpm, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->general_arc_rpm, 330, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->general_arc_rpm, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->general_arc_rpm, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->general_arc_rpm, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->general_arc_rpm, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->general_arc_rpm, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for general_arc_rpm, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_arc_width(ui->general_arc_rpm, 10, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui->general_arc_rpm, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(ui->general_arc_rpm, lv_color_hex(0xff6500), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_rounded(ui->general_arc_rpm, true, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write style for general_arc_rpm, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->general_arc_rpm, 0, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->general_arc_rpm, 5, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes general_arc_speed
    ui->general_arc_speed = lv_arc_create(ui->general);
    lv_obj_set_pos(ui->general_arc_speed, 8, 8);
    lv_obj_set_size(ui->general_arc_speed, 450, 450);
    lv_arc_set_mode(ui->general_arc_speed, LV_ARC_MODE_NORMAL);
    lv_arc_set_range(ui->general_arc_speed, 0, 200);
    lv_arc_set_bg_angles(ui->general_arc_speed, 140, 315);
    lv_arc_set_value(ui->general_arc_speed, 0);
    lv_arc_set_rotation(ui->general_arc_speed, 0);

    //Write style for general_arc_speed, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->general_arc_speed, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->general_arc_speed, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(ui->general_arc_speed, 15, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui->general_arc_speed, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(ui->general_arc_speed, lv_color_hex(0x111111), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_rounded(ui->general_arc_speed, true, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->general_arc_speed, 450, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->general_arc_speed, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->general_arc_speed, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->general_arc_speed, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->general_arc_speed, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->general_arc_speed, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for general_arc_speed, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_arc_width(ui->general_arc_speed, 15, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui->general_arc_speed, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(ui->general_arc_speed, lv_color_hex(0x2195f6), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_rounded(ui->general_arc_speed, true, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write style for general_arc_speed, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->general_arc_speed, 0, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->general_arc_speed, 0, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes general_arc_oil
    ui->general_arc_oil = lv_arc_create(ui->general);
    lv_obj_set_pos(ui->general_arc_oil, 8, 8);
    lv_obj_set_size(ui->general_arc_oil, 450, 450);
    lv_arc_set_mode(ui->general_arc_oil, LV_ARC_MODE_NORMAL);
    lv_arc_set_range(ui->general_arc_oil, 0, 100);
    lv_arc_set_bg_angles(ui->general_arc_oil, 3, 40);
    lv_arc_set_value(ui->general_arc_oil, 0);
    lv_arc_set_rotation(ui->general_arc_oil, -2);

    //Write style for general_arc_oil, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->general_arc_oil, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->general_arc_oil, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(ui->general_arc_oil, 15, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui->general_arc_oil, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(ui->general_arc_oil, lv_color_hex(0x111111), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_rounded(ui->general_arc_oil, true, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->general_arc_oil, 450, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->general_arc_oil, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->general_arc_oil, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->general_arc_oil, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->general_arc_oil, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->general_arc_oil, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for general_arc_oil, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_arc_width(ui->general_arc_oil, 15, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui->general_arc_oil, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(ui->general_arc_oil, lv_color_hex(0x00ff00), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_rounded(ui->general_arc_oil, true, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write style for general_arc_oil, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->general_arc_oil, 0, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->general_arc_oil, 0, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes general_arc_energy
    ui->general_arc_energy = lv_arc_create(ui->general);
    lv_obj_set_pos(ui->general_arc_energy, 8, 8);
    lv_obj_set_size(ui->general_arc_energy, 450, 450);
    lv_arc_set_mode(ui->general_arc_energy, LV_ARC_MODE_REVERSE);
    lv_arc_set_range(ui->general_arc_energy, 0, 100);
    lv_arc_set_bg_angles(ui->general_arc_energy, 320, 356);
    lv_arc_set_value(ui->general_arc_energy, 0);
    lv_arc_set_rotation(ui->general_arc_energy, 0);

    //Write style for general_arc_energy, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->general_arc_energy, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->general_arc_energy, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(ui->general_arc_energy, 15, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui->general_arc_energy, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(ui->general_arc_energy, lv_color_hex(0x111111), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_rounded(ui->general_arc_energy, true, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->general_arc_energy, 450, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->general_arc_energy, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->general_arc_energy, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->general_arc_energy, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->general_arc_energy, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->general_arc_energy, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for general_arc_energy, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_arc_width(ui->general_arc_energy, 15, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui->general_arc_energy, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(ui->general_arc_energy, lv_color_hex(0x00ff00), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_rounded(ui->general_arc_energy, true, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write style for general_arc_energy, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->general_arc_energy, 0, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->general_arc_energy, 0, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes general_cont_1
    ui->general_cont_1 = lv_obj_create(ui->general);
    lv_obj_set_pos(ui->general_cont_1, 208, 208);
    lv_obj_set_size(ui->general_cont_1, 50, 50);
    lv_obj_set_scrollbar_mode(ui->general_cont_1, LV_SCROLLBAR_MODE_OFF);

    //Write style for general_cont_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->general_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->general_cont_1, 150, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->general_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->general_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->general_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->general_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->general_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->general_cont_1, 30, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui->general_cont_1, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui->general_cont_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui->general_cont_1, 40, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_x(ui->general_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_y(ui->general_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes general_cont_2
    ui->general_cont_2 = lv_obj_create(ui->general);
    lv_obj_set_pos(ui->general_cont_2, 193, 193);
    lv_obj_set_size(ui->general_cont_2, 80, 80);
    lv_obj_set_scrollbar_mode(ui->general_cont_2, LV_SCROLLBAR_MODE_OFF);

    //Write style for general_cont_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->general_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->general_cont_2, 100, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->general_cont_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->general_cont_2, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->general_cont_2, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->general_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->general_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->general_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->general_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->general_cont_2, 50, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui->general_cont_2, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui->general_cont_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui->general_cont_2, 80, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_x(ui->general_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_y(ui->general_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes general_label_rpm_number
    ui->general_label_rpm_number = lv_label_create(ui->general);
    lv_obj_set_pos(ui->general_label_rpm_number, 48, 219);
    lv_obj_set_size(ui->general_label_rpm_number, 104, 27);
    lv_label_set_text(ui->general_label_rpm_number, "0");
    lv_label_set_long_mode(ui->general_label_rpm_number, LV_LABEL_LONG_WRAP);

    //Write style for general_label_rpm_number, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->general_label_rpm_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->general_label_rpm_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->general_label_rpm_number, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->general_label_rpm_number, &lv_font_montserratMedium_26, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->general_label_rpm_number, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->general_label_rpm_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->general_label_rpm_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->general_label_rpm_number, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->general_label_rpm_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->general_label_rpm_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->general_label_rpm_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->general_label_rpm_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->general_label_rpm_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->general_label_rpm_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes general_label_time
    ui->general_label_time = lv_label_create(ui->general);
    lv_obj_set_pos(ui->general_label_time, 173, 46);
    lv_obj_set_size(ui->general_label_time, 120, 32);
    lv_label_set_text(ui->general_label_time, "00:00");
    lv_label_set_long_mode(ui->general_label_time, LV_LABEL_LONG_WRAP);

    //Write style for general_label_time, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->general_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->general_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->general_label_time, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->general_label_time, &lv_font_montserratMedium_32, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->general_label_time, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->general_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->general_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->general_label_time, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->general_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->general_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->general_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->general_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->general_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->general_label_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes general_label_temp
    ui->general_label_temp = lv_label_create(ui->general);
    lv_obj_set_pos(ui->general_label_temp, 175, 82);
    lv_obj_set_size(ui->general_label_temp, 100, 32);
    lv_label_set_text(ui->general_label_temp, "16.5");
    lv_label_set_long_mode(ui->general_label_temp, LV_LABEL_LONG_WRAP);

    //Write style for general_label_temp, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->general_label_temp, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->general_label_temp, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->general_label_temp, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->general_label_temp, &lv_font_montserratMedium_24, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->general_label_temp, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->general_label_temp, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->general_label_temp, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->general_label_temp, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->general_label_temp, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->general_label_temp, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->general_label_temp, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->general_label_temp, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->general_label_temp, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->general_label_temp, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes general_slider_energy
    ui->general_slider_energy = lv_slider_create(ui->general);
    lv_obj_set_pos(ui->general_slider_energy, 132, 230);
    lv_obj_set_size(ui->general_slider_energy, 200, 5);
    lv_slider_set_range(ui->general_slider_energy, 0, 100);
    lv_slider_set_mode(ui->general_slider_energy, LV_SLIDER_MODE_RANGE);
    lv_slider_set_value(ui->general_slider_energy, 50, LV_ANIM_OFF);
    lv_slider_set_left_value(ui->general_slider_energy, 10, LV_ANIM_OFF);

    //Write style for general_slider_energy, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->general_slider_energy, 28, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->general_slider_energy, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->general_slider_energy, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->general_slider_energy, 8, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(ui->general_slider_energy, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->general_slider_energy, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for general_slider_energy, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->general_slider_energy, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->general_slider_energy, lv_color_hex(0x00ff24), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->general_slider_energy, LV_GRAD_DIR_NONE, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->general_slider_energy, 8, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write style for general_slider_energy, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->general_slider_energy, 0, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->general_slider_energy, 8, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes general_label_speed_number
    ui->general_label_speed_number = lv_label_create(ui->general);
    lv_obj_set_pos(ui->general_label_speed_number, 88, 144);
    lv_obj_set_size(ui->general_label_speed_number, 289, 71);
    lv_label_set_text(ui->general_label_speed_number, "00");
    lv_label_set_long_mode(ui->general_label_speed_number, LV_LABEL_LONG_WRAP);

    //Write style for general_label_speed_number, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->general_label_speed_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->general_label_speed_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->general_label_speed_number, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->general_label_speed_number, &lv_font_montserratMedium_72, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->general_label_speed_number, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->general_label_speed_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->general_label_speed_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->general_label_speed_number, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->general_label_speed_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->general_label_speed_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->general_label_speed_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->general_label_speed_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->general_label_speed_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->general_label_speed_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes general_label_energy_number
    ui->general_label_energy_number = lv_label_create(ui->general);
    lv_obj_set_pos(ui->general_label_energy_number, 72, 246);
    lv_obj_set_size(ui->general_label_energy_number, 225, 72);
    lv_label_set_text(ui->general_label_energy_number, "-16.8");
    lv_label_set_long_mode(ui->general_label_energy_number, LV_LABEL_LONG_WRAP);

    //Write style for general_label_energy_number, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->general_label_energy_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->general_label_energy_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->general_label_energy_number, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->general_label_energy_number, &lv_font_montserratMedium_68, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->general_label_energy_number, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->general_label_energy_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->general_label_energy_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->general_label_energy_number, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->general_label_energy_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->general_label_energy_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->general_label_energy_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->general_label_energy_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->general_label_energy_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->general_label_energy_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes general_label_energy_number_2
    ui->general_label_energy_number_2 = lv_label_create(ui->general);
    lv_obj_set_pos(ui->general_label_energy_number_2, 327, 205);
    lv_obj_set_size(ui->general_label_energy_number_2, 100, 32);
    lv_label_set_text(ui->general_label_energy_number_2, "160");
    lv_label_set_long_mode(ui->general_label_energy_number_2, LV_LABEL_LONG_WRAP);

    //Write style for general_label_energy_number_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->general_label_energy_number_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->general_label_energy_number_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->general_label_energy_number_2, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->general_label_energy_number_2, &lv_font_montserratMedium_24, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->general_label_energy_number_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->general_label_energy_number_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->general_label_energy_number_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->general_label_energy_number_2, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->general_label_energy_number_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->general_label_energy_number_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->general_label_energy_number_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->general_label_energy_number_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->general_label_energy_number_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->general_label_energy_number_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes general_label_oil_number
    ui->general_label_oil_number = lv_label_create(ui->general);
    lv_obj_set_pos(ui->general_label_oil_number, 327, 232);
    lv_obj_set_size(ui->general_label_oil_number, 100, 32);
    lv_label_set_text(ui->general_label_oil_number, "800");
    lv_label_set_long_mode(ui->general_label_oil_number, LV_LABEL_LONG_WRAP);

    //Write style for general_label_oil_number, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->general_label_oil_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->general_label_oil_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->general_label_oil_number, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->general_label_oil_number, &lv_font_montserratMedium_24, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->general_label_oil_number, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->general_label_oil_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->general_label_oil_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->general_label_oil_number, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->general_label_oil_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->general_label_oil_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->general_label_oil_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->general_label_oil_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->general_label_oil_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->general_label_oil_number, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes general_label_trip
    ui->general_label_trip = lv_label_create(ui->general);
    lv_obj_set_pos(ui->general_label_trip, 183, 365);
    lv_obj_set_size(ui->general_label_trip, 100, 32);
    lv_label_set_text(ui->general_label_trip, "trip");
    lv_label_set_long_mode(ui->general_label_trip, LV_LABEL_LONG_WRAP);

    //Write style for general_label_trip, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->general_label_trip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->general_label_trip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->general_label_trip, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->general_label_trip, &lv_font_montserratMedium_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->general_label_trip, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->general_label_trip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->general_label_trip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->general_label_trip, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->general_label_trip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->general_label_trip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->general_label_trip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->general_label_trip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->general_label_trip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->general_label_trip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes general_img_temp
    ui->general_img_temp = lv_image_create(ui->general);
    lv_obj_set_pos(ui->general_img_temp, 255, 84);
    lv_obj_set_size(ui->general_img_temp, 20, 19);
    lv_obj_add_flag(ui->general_img_temp, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(ui->general_img_temp, &_tempunit_RGB565A8_20x19);
    lv_image_set_pivot(ui->general_img_temp, 50,50);
    lv_image_set_rotation(ui->general_img_temp, 0);

    //Write style for general_img_temp, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_image_recolor_opa(ui->general_img_temp, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_image_recolor(ui->general_img_temp, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_image_opa(ui->general_img_temp, 255, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes general_label_1
    ui->general_label_1 = lv_label_create(ui->general);
    lv_obj_set_pos(ui->general_label_1, 162, 426);
    lv_obj_set_size(ui->general_label_1, 100, 32);
    lv_label_set_text(ui->general_label_1, "000000");
    lv_label_set_long_mode(ui->general_label_1, LV_LABEL_LONG_WRAP);

    //Write style for general_label_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->general_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->general_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->general_label_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->general_label_1, &lv_font_montserratMedium_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->general_label_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->general_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->general_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->general_label_1, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->general_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->general_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->general_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->general_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->general_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->general_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes general_label_2
    ui->general_label_2 = lv_label_create(ui->general);
    lv_obj_set_pos(ui->general_label_2, 107, 395);
    lv_obj_set_size(ui->general_label_2, 62, 21);
    lv_label_set_text(ui->general_label_2, "15.6");
    lv_label_set_long_mode(ui->general_label_2, LV_LABEL_LONG_WRAP);

    //Write style for general_label_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->general_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->general_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->general_label_2, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->general_label_2, &lv_font_montserratMedium_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->general_label_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->general_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->general_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->general_label_2, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->general_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->general_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->general_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->general_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->general_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->general_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes general_label_3
    ui->general_label_3 = lv_label_create(ui->general);
    lv_obj_set_pos(ui->general_label_3, 229, 395);
    lv_obj_set_size(ui->general_label_3, 55, 22);
    lv_label_set_text(ui->general_label_3, "5.6\n");
    lv_label_set_long_mode(ui->general_label_3, LV_LABEL_LONG_WRAP);

    //Write style for general_label_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->general_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->general_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->general_label_3, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->general_label_3, &lv_font_montserratMedium_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->general_label_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->general_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->general_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->general_label_3, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->general_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->general_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->general_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->general_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->general_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->general_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes general_label_4
    ui->general_label_4 = lv_label_create(ui->general);
    lv_obj_set_pos(ui->general_label_4, 180, 397);
    lv_obj_set_size(ui->general_label_4, 55, 22);
    lv_label_set_text(ui->general_label_4, "kw/H");
    lv_label_set_long_mode(ui->general_label_4, LV_LABEL_LONG_WRAP);

    //Write style for general_label_4, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->general_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->general_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->general_label_4, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->general_label_4, &lv_font_montserratMedium_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->general_label_4, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->general_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->general_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->general_label_4, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->general_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->general_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->general_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->general_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->general_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->general_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes general_label_5
    ui->general_label_5 = lv_label_create(ui->general);
    lv_obj_set_pos(ui->general_label_5, 293, 395);
    lv_obj_set_size(ui->general_label_5, 55, 22);
    lv_label_set_text(ui->general_label_5, "L/km");
    lv_label_set_long_mode(ui->general_label_5, LV_LABEL_LONG_WRAP);

    //Write style for general_label_5, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->general_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->general_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->general_label_5, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->general_label_5, &lv_font_montserratMedium_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->general_label_5, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->general_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->general_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->general_label_5, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->general_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->general_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->general_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->general_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->general_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->general_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes general_label_6
    ui->general_label_6 = lv_label_create(ui->general);
    lv_obj_set_pos(ui->general_label_6, 261, 426);
    lv_obj_set_size(ui->general_label_6, 45, 20);
    lv_label_set_text(ui->general_label_6, "km");
    lv_label_set_long_mode(ui->general_label_6, LV_LABEL_LONG_WRAP);

    //Write style for general_label_6, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->general_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->general_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->general_label_6, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->general_label_6, &lv_font_montserratMedium_18, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->general_label_6, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->general_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->general_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->general_label_6, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->general_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->general_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->general_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->general_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->general_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->general_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes general_label_unit_kw
    ui->general_label_unit_kw = lv_label_create(ui->general);
    lv_obj_set_pos(ui->general_label_unit_kw, 297, 287);
    lv_obj_set_size(ui->general_label_unit_kw, 40, 18);
    lv_label_set_text(ui->general_label_unit_kw, "kw");
    lv_label_set_long_mode(ui->general_label_unit_kw, LV_LABEL_LONG_WRAP);

    //Write style for general_label_unit_kw, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->general_label_unit_kw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->general_label_unit_kw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->general_label_unit_kw, lv_color_hex(0x999999), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->general_label_unit_kw, &lv_font_montserratMedium_18, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->general_label_unit_kw, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->general_label_unit_kw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->general_label_unit_kw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->general_label_unit_kw, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->general_label_unit_kw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->general_label_unit_kw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->general_label_unit_kw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->general_label_unit_kw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->general_label_unit_kw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->general_label_unit_kw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //The custom code of general.


    //Update current screen layout.
    lv_obj_update_layout(ui->general);

}
