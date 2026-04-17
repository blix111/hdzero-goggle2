#include <stdio.h>
#include <unistd.h>

#include <log/log.h>
#include <minIni.h>

#include "core/app_state.h"
#include "core/battery.h"
#include "core/common.hh"
#include "driver/dm5680.h"
#include "driver/hardware.h"
#include "driver/mcp3021.h"
#include "driver/rtc6715.h"
#include "lang/language.h"
#include "page_common.h"
#include "ui/ui_style.h"

// NOTRE FICHIER EST INCLUS ICI, APRÈS LES FONDATIONS !
#include "page_analog_scanner.h"

#define SCANNER_CHANNELS 40

static lv_obj_t *chart;
static lv_chart_series_t *ser_rssi;
static lv_timer_t *scan_timer = NULL;
static int current_scan_ch = 0;

static void scan_timer_cb(lv_timer_t * timer) {
    if (chart == NULL) return;

    RTC6715_SetCH(current_scan_ch);
    usleep(20 * 1000); 

    int volt_mv = 0;
    for (int i = 0; i < 4; i++) {
        volt_mv += RTC6715_GetRssi();
    }
    volt_mv = volt_mv / 4;

    lv_chart_set_next_value(chart, ser_rssi, volt_mv);

    current_scan_ch++;
    if (current_scan_ch >= SCANNER_CHANNELS) {
        current_scan_ch = 0;
    }
}

static lv_obj_t *page_analog_scanner_create(lv_obj_t *parent, panel_arr_t *arr) {
    lv_obj_t *page = lv_menu_page_create(parent, NULL);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(page, 1053, 900);
    lv_obj_add_style(page, &style_subpage, LV_PART_MAIN);
    lv_obj_set_style_pad_top(page, 94, 0);

    lv_obj_t *section = lv_menu_section_create(page);
    lv_obj_add_style(section, &style_submenu, LV_PART_MAIN);
    lv_obj_set_size(section, 1053, 894);

    create_text(NULL, section, false, "Analog Scanner", LV_MENU_ITEM_BUILDER_VARIANT_2);

    lv_obj_t *cont = lv_obj_create(section);
    lv_obj_set_size(cont, 960, 800);
    lv_obj_set_pos(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(cont, &style_context, LV_PART_MAIN);

    chart = lv_chart_create(cont);
    lv_obj_set_size(chart, 900, 600);
    lv_obj_center(chart);
    lv_chart_set_type(chart, LV_CHART_TYPE_BAR);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 3300);
    lv_chart_set_point_count(chart, SCANNER_CHANNELS);
    
    lv_chart_set_div_line_count(chart, 5, 8);
    lv_obj_set_style_bg_color(chart, lv_color_make(30, 30, 30), 0);
    lv_obj_set_style_line_color(chart, lv_color_make(100, 100, 100), 0);

    ser_rssi = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);

    for(int i = 0; i < SCANNER_CHANNELS; i++) {
        lv_chart_set_next_value(chart, ser_rssi, 0);
    }

    return page;
}

static void on_enter() {
    RTC6715_Open(1, 0);
    current_scan_ch = 0;
    if (scan_timer == NULL) {
        scan_timer = lv_timer_create(scan_timer_cb, 50, NULL);
    }
}

static void on_exit() {
    if (scan_timer != NULL) {
        lv_timer_del(scan_timer);
        scan_timer = NULL;
    }
    RTC6715_Open(0, 0);
}

page_pack_t pp_analog_scanner = {
    .p_arr = {
        .cur = 0,
        .max = 0,
    },
    .name = "Analog Scanner",
    .create = page_analog_scanner_create,
    .enter = on_enter,
    .exit = on_exit,
    .on_created = NULL,
    .on_update = NULL,
    .on_roller = NULL,
    .on_click = NULL,
    .on_right_button = NULL,
};
