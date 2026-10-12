/*
 * Simulador de la interfaz de la pantalla en la PC: renderiza ui.c con LVGL
 * en memoria y guarda capturas (PPM) de cada página con datos de ejemplo.
 */
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "ui.h"
#include "link.h"
#include "knob.h"
#include "settings.h"
#include "lcd.h"

uint32_t sim_tick;
static uint16_t fb[480 * 320];

/* ---- stubs del hardware ---- */
static dl_status_t st;
static dl_thresholds_t thr = { 800, 1500, 300, 700, 1500, 2500 };
static dl_config_t cfg = { 2, DL_DOA_SCAN, 0 };
static settings_t set = { .brightness = 80, .cal_valid = 1, .led_on = 1 };
static knob_event_t pending_ev;
static int alive = 1;

bool link_alive(void) { return alive; }
const dl_status_t *link_status(void) { return &st; }
bool link_thresholds(dl_thresholds_t *o) { *o = thr; return true; }
bool link_config(dl_config_t *o) { *o = cfg; return true; }
void link_send(uint8_t t, const void *p, uint8_t l) { (void)t; (void)p; (void)l; }
void link_send_thresholds(const dl_thresholds_t *t) { (void)t; }
void link_send_config(const dl_config_t *c) { (void)c; }
void link_send_ack_alarm(void) {}
uint32_t link_rx_frames(void) { return 1234; }
uint32_t link_rx_errors(void) { return 0; }
void hw_backlight_set(uint8_t p) { (void)p; }
void hw_buzzer_set(bool on) { (void)on; }
void hw_buzzer_beep(uint16_t ms) { (void)ms; }
int16_t knob_take_steps(void) { return 0; }
knob_event_t knob_poll_button(void) { knob_event_t e = pending_ev; pending_ev = KNOB_EV_NONE; return e; }
void knob_set_leds(uint32_t a, uint32_t b) { (void)a; (void)b; }
settings_t *settings(void) { return &set; }
bool settings_save(void) { return true; }
void calib_start(void) {}
bool calib_active(void) { return false; }
int lcd_driver(void) { return LCD_DRV_ST7796S; }
bool fwupdate_active(void) { return false; }
uint32_t fwupdate_running_crc(void) { return 0x1A2B3C4D; }
uint32_t w25q_jedec(void) { return 0xEF4017; }
uint32_t w25q_size(void) { return 8u << 20; }

static void flush_cb(lv_disp_drv_t *d, const lv_area_t *a, lv_color_t *px)
{
    for (int y = a->y1; y <= a->y2; y++)
        for (int x = a->x1; x <= a->x2; x++)
            fb[y * 480 + x] = (px++)->full;
    lv_disp_flush_ready(d);
}

static void run(int ms)
{
    for (int i = 0; i < ms; i += 5) {
        sim_tick += 5;
        ui_update();
        lv_timer_handler();
    }
}

static void save(const char *path)
{
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n480 320\n255\n");
    for (int i = 0; i < 480 * 320; i++) {
        uint16_t c = fb[i];
        unsigned char rgb[3] = { (unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 0x3F) << 2),
                                 (unsigned char)((c & 0x1F) << 3) };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("%s\n", path);
}

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : ".";
    char path[256];
    static lv_color_t buf[480 * 40];
    static lv_disp_draw_buf_t db;
    static lv_disp_drv_t dd;

    lv_init();
    lv_disp_draw_buf_init(&db, buf, NULL, 480 * 40);
    lv_disp_drv_init(&dd);
    dd.hor_res = 480;
    dd.ver_res = 320;
    dd.flush_cb = flush_cb;
    dd.draw_buf = &db;
    lv_disp_drv_register(&dd);

    ui_init(0x7796);

    st.level = DL_LEVEL_NORMAL;
    st.flags = DL_FLAG_DETECTOR_OK | DL_FLAG_ETH_LINK | DL_FLAG_DOA_VALID;
    st.doa_ddeg = 375;
    st.audio_score_m = 520;
    st.vib_rms_mg = 140;
    st.current_ma[0] = 1100;
    run(300);
    snprintf(path, sizeof path, "%s/0_estado_normal.ppm", out); save(path);

    st.level = DL_LEVEL_ANOMALIA;
    st.doa_ddeg = -520;
    st.audio_score_m = 1720;
    st.vib_rms_mg = 760;
    st.current_ma[0] = 2300;
    run(300);
    snprintf(path, sizeof path, "%s/1_estado_anomalia.ppm", out); save(path);

    for (int p = 1; p < 4; p++) {
        pending_ev = KNOB_EV_LONG;
        run(200);
        snprintf(path, sizeof path, "%s/%d_pagina%d.ppm", out, p + 1, p); save(path);
    }

    pending_ev = KNOB_EV_LONG;   /* volver a Estado, sin enlace */
    alive = 0;
    run(300);
    snprintf(path, sizeof path, "%s/5_sin_enlace.ppm", out); save(path);

    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    printf("Memoria LVGL usada (máx.): %lu bytes (punteros de 64 bits)\n",
           (unsigned long)mon.max_used);
    return 0;
}
