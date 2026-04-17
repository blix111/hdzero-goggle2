#include "page_analog_scanner.h"

#include <stdio.h>
#include <unistd.h>

#include <log/log.h>
#include <minIni.h>

#include "core/app_state.h"
#include "driver/rtc6715.h"
#include "lang/language.h"
#include "page_common.h"
#include "ui/ui_style.h"

// Le nombre de chaînes analogiques standards (Bandes A, B, E, F, R)
#define SCANNER_CHANNELS 40

static lv_obj_t *chart;
static lv_chart_series_t *ser_rssi;
static lv_timer_t *scan_timer = NULL;
static int current_scan_ch = 0;
static int saved_channel = 0; // Pour mémoriser la chaîne sur laquelle on était avant de scanner

// Fonction appelée régulièrement pour scanner la chaîne suivante
static void scan_timer_cb(lv_timer_t * timer) {
    if (chart == NULL) return;

    // 1. Régler la puce analogique sur la chaîne actuelle
    RTC6715_SetCH(current_scan_ch);
    
    // 2. Attendre très brièvement que le signal se stabilise (sans bloquer toute l'interface)
    usleep(20 * 1000); 

    // 3. Lire le RSSI (On fait une petite moyenne rapide)
    int volt_mv = 0;
    for (int i = 0; i < 4; i++) {
        volt_mv += RTC6715_GetRssi();
    }
    volt_mv = volt_mv / 4;

    // 4. Mettre à jour le graphique (on limite arbitrairement entre 0 et 3300 mV pour l'affichage)
    lv_chart_set_next_value(chart, ser_rssi, volt_mv);

    // 5. Passer à la chaîne suivante
    current_scan_ch++;
    if (current_scan_ch >= SCANNER_CHANNELS) {
        current_scan_ch = 0; // On boucle
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

    create_text(NULL, section, false, _lang("Analog Scanner"), LV_MENU_ITEM_BUILDER_VARIANT_2);

    lv_obj_t *cont = lv_obj_create(section);
    lv_obj_set_size(cont, 960, 800);
    lv_obj_set_pos(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(cont, &style_context, LV_PART_MAIN);

    // --- Création du graphique LVGL ---
    chart = lv_chart_create(cont);
    lv_obj_set_size(chart, 900, 600);
    lv_obj_center(chart);
    lv_chart_set_type(chart, LV_CHART_TYPE_BAR);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 3300); // Plage RSSI max attendue en mV
    lv_chart_set_point_count(chart, SCANNER_CHANNELS);
    
    // Grille de fond
    lv_chart_set_div_line_count(chart, 5, 8);
    lv_obj_set_style_bg_color(chart, lv_color_make(30, 30, 30), 0);
    lv_obj_set_style_line_color(chart, lv_color_make(100, 100, 100), 0);

    // Série de données (les barres)
    ser_rssi = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);

    // Remplir de zéros au début
    for(int i = 0; i < SCANNER_CHANNELS; i++) {
        lv_chart_set_next_value(chart, ser_rssi, 0);
    }

    return page;
}

static void on_enter() {
    // On allume le module analogique
    RTC6715_Open(1, 0);
    
    // On démarre le timer qui va scanner les chaînes toutes les 50ms
    // 50ms * 40 channels = ~2 secondes pour un scan complet. C'est rapide et ça ne fait pas lagger.
    current_scan_ch = 0;
    if (scan_timer == NULL) {
        scan_timer = lv_timer_create(scan_timer_cb, 50, NULL);
    }
}

static void on_exit() {
    // On arrête le timer
    if (scan_timer != NULL) {
        lv_timer_del(scan_timer);
        scan_timer = NULL;
    }
    
    // On remet le module analogique dans son état (ici on le coupe pour simplifier, 
    // l'OSD le rallumera si nécessaire)
    RTC6715_Open(0, 0);
}

page_pack_t pp_analog_scanner = {
    .p_arr = {
        .cur = 0,
        .max = 0, // Pas de boutons cliquables, c'est juste un affichage
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
