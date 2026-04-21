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
#include "ui/ui_main_menu.h"
#include "ui/ui_style.h"

#include "page_analog_scanner.h"

#define SCANNER_CHANNELS 48

static const char *ch_names[SCANNER_CHANNELS] = {
    "A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8",
    "B1", "B2", "B3", "B4", "B5", "B6", "B7", "B8",
    "E1", "E2", "E3", "E4", "E5", "E6", "E7", "E8",
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8",
    "R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8",
    "L1", "L2", "L3", "L4", "L5", "L6", "L7", "L8"
};

static const uint16_t ch_freqs[SCANNER_CHANNELS] = {
    5865, 5845, 5825, 5805, 5785, 5765, 5745, 5725, // Band A
    5733, 5752, 5771, 5790, 5809, 5828, 5847, 5866, // Band B
    5705, 5685, 5665, 5645, 5885, 5905, 5925, 5945, // Band E
    5740, 5760, 5780, 5800, 5820, 5840, 5860, 5880, // Band F
    5658, 5695, 5732, 5769, 5806, 5843, 5880, 5917, // Band R
    5362, 5399, 5436, 5473, 5510, 5547, 5584, 5621  // Band L
};

static lv_obj_t *chart;
static lv_chart_series_t *ser_rssi;
static lv_chart_cursor_t *cursor;
static lv_timer_t *scan_timer = NULL;
static lv_obj_t *label_info;
static lv_obj_t *btn_scan;
static lv_obj_t *label_btn;
static lv_obj_t *label_blix;

static int current_scan_ch = 0;
static int selected_ch = 0;
static bool is_scanning = false;

static void update_info() {
    char buf[64];
    snprintf(buf, sizeof(buf), "Channel: %s  |  Freq: %d MHz", ch_names[selected_ch], ch_freqs[selected_ch]);
    lv_label_set_text(label_info, buf);
    
    // Highlight the selected point in chart if not scanning
    if (!is_scanning && cursor) {
        lv_chart_set_cursor_point(chart, cursor, ser_rssi, selected_ch);
    }
}

static void scan_timer_cb(lv_timer_t * timer) {
    if (!is_scanning || chart == NULL) return;

    RTC6715_SetCH(current_scan_ch);
    usleep(40 * 1000); // 40ms to stabilize

    int rssi = 0;
    for (int i = 0; i < 8; i++) {
        rssi += RTC6715_GetRssi();
    }
    rssi /= 8;

    lv_chart_set_value_by_id(chart, ser_rssi, current_scan_ch, rssi);
    
    current_scan_ch++;
    if (current_scan_ch >= SCANNER_CHANNELS) {
        current_scan_ch = 0;
    }
}

static void btn_scan_cb(lv_event_t * e) {
    is_scanning = !is_scanning;
    if (is_scanning) {
        lv_label_set_text(label_btn, "STOP SCAN");
        lv_obj_set_style_bg_color(btn_scan, lv_palette_main(LV_PALETTE_RED), 0);
    } else {
        lv_label_set_text(label_btn, "START SCAN");
        lv_obj_set_style_bg_color(btn_scan, lv_palette_main(LV_PALETTE_GREEN), 0);
        selected_ch = current_scan_ch;
        update_info();
    }
}

static lv_obj_t *page_analog_scanner_create(lv_obj_t *parent, panel_arr_t *arr) {
    lv_obj_t *page = lv_menu_page_create(parent, NULL);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(page, 1053, 900);
    lv_obj_add_style(page, &style_subpage, LV_PART_MAIN);
    lv_obj_set_style_pad_top(page, 60, 0);

    lv_obj_t *section = lv_menu_section_create(page);
    lv_obj_add_style(section, &style_submenu, LV_PART_MAIN);
    lv_obj_set_size(section, 1053, 894);

    create_text(NULL, section, false, "Analog Scanner V2", LV_MENU_ITEM_BUILDER_VARIANT_2);

    lv_obj_t *cont = lv_obj_create(section);
    lv_obj_set_size(cont, 1000, 750);
    lv_obj_add_style(cont, &style_context, LV_PART_MAIN);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    // Info label at top
    label_info = lv_label_create(cont);
    lv_label_set_text(label_info, "Select a channel or Start Scan");
    lv_obj_set_style_text_font(label_info, &lv_font_montserrat_28, 0);
    lv_obj_align(label_info, LV_ALIGN_TOP_MID, 0, 10);

    // Chart
    chart = lv_chart_create(cont);
    lv_obj_set_size(chart, 900, 450);
    lv_obj_align(chart, LV_ALIGN_TOP_MID, 0, 70);
    lv_chart_set_type(chart, LV_CHART_TYPE_BAR);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 3300);
    lv_chart_set_point_count(chart, SCANNER_CHANNELS);
    lv_chart_set_div_line_count(chart, 5, 12);
    
    lv_obj_set_style_bg_color(chart, lv_color_make(20, 20, 20), 0);
    lv_obj_set_style_line_color(chart, lv_color_make(60, 60, 60), 0);
    
    ser_rssi = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
    
    // Add a cursor
    cursor = lv_chart_add_cursor(chart, lv_palette_main(LV_PALETTE_YELLOW), LV_DIR_VER);

    // Buttons at bottom
    btn_scan = lv_btn_create(cont);
    lv_obj_set_size(btn_scan, 200, 60);
    lv_obj_align(btn_scan, LV_ALIGN_BOTTOM_MID, 0, -60);
    lv_obj_set_style_bg_color(btn_scan, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_add_event_cb(btn_scan, btn_scan_cb, LV_EVENT_CLICKED, NULL);

    label_btn = lv_label_create(btn_scan);
    lv_label_set_text(label_btn, "START SCAN");
    lv_obj_center(label_btn);

    // Made by Blix
    label_blix = lv_label_create(cont);
    lv_label_set_text(label_blix, "Made by Blix 🇨🇭");
    lv_obj_set_style_text_font(label_blix, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_blix, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(label_blix, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

    return page;
}

static void on_enter() {
    RTC6715_Open(1, 0);
    is_scanning = false;
    current_scan_ch = 0;
    if (scan_timer == NULL) {
        scan_timer = lv_timer_create(scan_timer_cb, 60, NULL);
    }
}

static void on_exit() {
    is_scanning = false;
    if (scan_timer != NULL) {
        lv_timer_del(scan_timer);
        scan_timer = NULL;
    }
    RTC6715_Open(0, 0);
}

static void on_roller(uint8_t key) {
    if (is_scanning) return;
    
    if (key == DIAL_KEY_UP) {
        selected_ch = (selected_ch + 1) % SCANNER_CHANNELS;
    } else if (key == DIAL_KEY_DOWN) {
        selected_ch = (selected_ch - 1 + SCANNER_CHANNELS) % SCANNER_CHANNELS;
    }
    update_info();
}

static void on_click(uint8_t key, int sel) {
    if (is_scanning) {
        // Stop scan on click
        is_scanning = false;
        lv_label_set_text(label_btn, "START SCAN");
        lv_obj_set_style_bg_color(btn_scan, lv_palette_main(LV_PALETTE_GREEN), 0);
        update_info();
    } else {
        // Tune to selected channel
        g_setting.source.analog_channel = (uint8_t)selected_ch;
        RTC6715_SetCH(selected_ch);
        LOGI("Scanner tuned to %s (%d MHz)", ch_names[selected_ch], ch_freqs[selected_ch]);
        // Visual feedback
        lv_label_set_text(label_info, "TUNED! Press Back to exit");
    }
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
    .on_roller = on_roller,
    .on_click = on_click,
    .on_right_button = NULL,
};
