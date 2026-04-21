#include <stdio.h>
#include <unistd.h>

#include <log/log.h>
#include <minIni.h>

#include "core/app_state.h"
#include "core/battery.h"
#include "core/common.hh"
#include "core/dvr.h"
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
static lv_obj_t *label_status;
static lv_obj_t *label_blix;

static int current_scan_ch = 0;
static int selected_ch = 0;
static bool is_scanning = false;

static void update_info() {
    char buf[128];
    if (is_scanning) {
        snprintf(buf, sizeof(buf), "#FF0000 SCANNING...#\nClick to Pause");
    } else {
        snprintf(buf, sizeof(buf), "Channel: #FFFF00 %s#  (%d MHz)\n#00FF00 Long Press to FLY#", ch_names[selected_ch], ch_freqs[selected_ch]);
    }
    lv_label_set_text(label_info, buf);
    
    if (!is_scanning && cursor) {
        lv_chart_set_cursor_point(chart, cursor, ser_rssi, selected_ch);
    }
}

static void scan_timer_cb(lv_timer_t * timer) {
    if (!is_scanning || chart == NULL) return;

    RTC6715_SetCH(current_scan_ch);
    usleep(40 * 1000); 

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
    lv_label_set_recolor(label_info, true);
    lv_obj_set_style_text_font(label_info, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_align(label_info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label_info, LV_ALIGN_TOP_MID, 0, 20);

    // Chart
    chart = lv_chart_create(cont);
    lv_obj_set_size(chart, 920, 480);
    lv_obj_align(chart, LV_ALIGN_TOP_MID, 0, 140);
    lv_chart_set_type(chart, LV_CHART_TYPE_BAR);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 3300);
    lv_chart_set_point_count(chart, SCANNER_CHANNELS);
    lv_chart_set_div_line_count(chart, 5, 12);
    
    lv_obj_set_style_bg_color(chart, lv_color_make(20, 20, 20), 0);
    lv_obj_set_style_line_color(chart, lv_color_make(60, 60, 60), 0);
    
    ser_rssi = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
    cursor = lv_chart_add_cursor(chart, lv_palette_main(LV_PALETTE_YELLOW), LV_DIR_VER);

    // Instructions at bottom
    label_status = lv_label_create(cont);
    lv_label_set_text(label_status, "Dial: Select Channel  |  Click: Play/Pause  |  Long Press: FLY");
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(label_status, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(label_status, LV_ALIGN_BOTTOM_MID, 0, -40);

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
    is_scanning = true; // Start scanning by default
    current_scan_ch = 0;
    if (scan_timer == NULL) {
        scan_timer = lv_timer_create(scan_timer_cb, 60, NULL);
    }
    update_info();
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
    if (key == DIAL_KEY_CLICK) {
        is_scanning = !is_scanning;
        if (!is_scanning) {
            selected_ch = (current_scan_ch > 0) ? (current_scan_ch - 1) : (SCANNER_CHANNELS - 1);
        }
        update_info();
    } else if (key == DIAL_KEY_PRESS) {
        // Long press: Tune and Fly!
        is_scanning = false;
        
        // Update settings
        g_setting.source.analog_channel = (uint8_t)(selected_ch + 1);
        ini_putl("source", "analog_channel", g_setting.source.analog_channel, SETTING_INI);
        
        LOGI("Scanner tuned to %s, launching Analog mode", ch_names[selected_ch]);
        
        // Execute source change
        app_switch_to_analog();
        g_source_info.source = SOURCE_ANALOG;
        app_state_push(APP_STATE_VIDEO);
        
        // Clean up and close menu
        main_menu_show(false);
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
