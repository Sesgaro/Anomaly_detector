/*
 * Interfaz de la pantalla (LVGL 8.3, 480x320).
 *
 *  - Barra superior: nivel de alerta y estado del enlace.
 *  - Estado:   ángulo DOA, puntaje acústico, vibración, corriente del motor y
 *              botón para reconocer la alarma.
 *  - Umbrales: ajuste de los umbrales (se envían a la STM32H745).
 *  - Ajustes:  micrófonos, modo DOA, ángulo de enfoque, brillo, calibración.
 *
 * Las fuentes integradas solo tienen ASCII y "°", por eso los textos van sin
 * tildes.
 */
#include "ui.h"
#include "link.h"
#include "hw.h"
#include "settings.h"
#include "calib.h"
#include "lcd.h"
#include "lvgl.h"
#include <stddef.h>
#include <string.h>

#define UPDATE_MS 100u

#define COL_NORMAL   lv_color_hex(0x2E7D32)
#define COL_WARN     lv_color_hex(0xF9A825)
#define COL_ANOM     lv_color_hex(0xEF6C00)
#define COL_FAIL     lv_color_hex(0xC62828)
#define COL_NOLINK   lv_color_hex(0x546E7A)

/* ---------------------------------------------------------------- estado */

static lv_obj_t *top_bar, *lbl_level, *lbl_link;
static lv_obj_t *tab_state, *meter, *lbl_doa;
static lv_meter_indicator_t *needle;
static bool meter_four_mics;

typedef struct {
    lv_obj_t *bar;
    lv_obj_t *value;
} gauge_t;

static gauge_t g_audio, g_vib, g_curr;
static lv_obj_t *btn_ack;

/* ---------------------------------------------------------------- umbrales */

typedef struct {
    const char *name;
    const char *unit;
    uint16_t    max;
    uint16_t    step;
    uint8_t     decimals;   /* 3 = valor ×1000 */
    size_t      offset;     /* campo en dl_thresholds_t */
    lv_obj_t   *slider;
    lv_obj_t   *value;
} thr_row_t;

static thr_row_t rows[] = {
    { "AE advert.",   "",   5000, 10, 3, offsetof(dl_thresholds_t, audio_warn_m)    },
    { "AE anomalia",  "",   5000, 10, 3, offsetof(dl_thresholds_t, audio_anom_m)    },
    { "Vib. advert.", "mg", 4000, 10, 0, offsetof(dl_thresholds_t, vib_warn_mg)     },
    { "Vib. anomalia","mg", 4000, 10, 0, offsetof(dl_thresholds_t, vib_anom_mg)     },
    { "I advert.",    "mA", 8000, 50, 0, offsetof(dl_thresholds_t, current_warn_ma) },
    { "I critica",    "mA", 8000, 50, 0, offsetof(dl_thresholds_t, current_crit_ma) },
};
#define N_ROWS (sizeof(rows) / sizeof(rows[0]))

static dl_thresholds_t thr_edit, thr_remote;
static bool thr_loaded, thr_dirty;
static lv_obj_t *lbl_thr_status, *btn_thr_send, *btn_thr_reload;

/* ---------------------------------------------------------------- ajustes */

static dl_config_t cfg_edit;
static bool cfg_loaded;
static lv_obj_t *bm_mics, *bm_mode, *sl_focus, *lbl_focus, *lbl_cfg_status;
static lv_obj_t *sl_bright, *lbl_info;
static uint16_t lcd_id_shown;

static uint8_t acked_level;

/* ================================================================ helpers */

static void fmt_value(char *buf, size_t n, uint16_t v, uint8_t decimals, const char *unit)
{
    if (decimals == 3)
        lv_snprintf(buf, n, "%u.%03u%s", v / 1000u, v % 1000u, unit);
    else
        lv_snprintf(buf, n, "%u %s", v, unit);
}

static lv_color_t level_color(uint8_t level)
{
    switch (level) {
    case DL_LEVEL_NORMAL:      return COL_NORMAL;
    case DL_LEVEL_ADVERTENCIA: return COL_WARN;
    case DL_LEVEL_ANOMALIA:    return COL_ANOM;
    default:                   return COL_FAIL;
    }
}

static const char *level_text(uint8_t level)
{
    switch (level) {
    case DL_LEVEL_NORMAL:      return "NORMAL";
    case DL_LEVEL_ADVERTENCIA: return "ADVERTENCIA";
    case DL_LEVEL_ANOMALIA:    return "ANOMALIA";
    default:                   return "FALLA";
    }
}

static lv_obj_t *flex_row(lv_obj_t *parent)
{
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(r, 6, 0);
    lv_obj_set_style_pad_ver(r, 2, 0);
    return r;
}

static lv_obj_t *small_btn(lv_obj_t *parent, const char *txt, int w)
{
    lv_obj_t *b = lv_btn_create(parent);
    lv_obj_set_size(b, w, 32);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_center(l);
    return b;
}

/* ================================================================ estado */

static void build_meter(bool four_mics)
{
    if (meter)
        lv_obj_del(meter);

    meter = lv_meter_create(tab_state);
    lv_obj_set_size(meter, 170, 170);
    lv_obj_align(meter, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_font(meter, &lv_font_montserrat_14, 0);

    lv_meter_scale_t *sc = lv_meter_add_scale(meter);
    if (four_mics) {
        /* -180..180 en círculo completo; 0° arriba */
        lv_meter_set_scale_ticks(meter, sc, 25, 2, 8, lv_palette_main(LV_PALETTE_GREY));
        lv_meter_set_scale_major_ticks(meter, sc, 6, 3, 14, lv_color_black(), 12);
        lv_meter_set_scale_range(meter, sc, -180, 180, 360, 90);
    } else {
        /* -90..90 en semicírculo; 0° arriba */
        lv_meter_set_scale_ticks(meter, sc, 19, 2, 8, lv_palette_main(LV_PALETTE_GREY));
        lv_meter_set_scale_major_ticks(meter, sc, 3, 3, 14, lv_color_black(), 12);
        lv_meter_set_scale_range(meter, sc, -90, 90, 180, 180);
    }
    needle = lv_meter_add_needle_line(meter, sc, 4, lv_palette_main(LV_PALETTE_RED), -12);
    meter_four_mics = four_mics;

    if (lbl_doa)
        lv_obj_align_to(lbl_doa, meter, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);
}

static gauge_t make_gauge(lv_obj_t *parent, const char *name, int y)
{
    gauge_t g;
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, name);
    lv_obj_set_pos(l, 185, y);

    g.value = lv_label_create(parent);
    lv_label_set_text(g.value, "--");
    lv_obj_set_width(g.value, 110);
    lv_obj_set_style_text_align(g.value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(g.value, 330, y);

    g.bar = lv_bar_create(parent);
    lv_obj_set_size(g.bar, 255, 14);
    lv_obj_set_pos(g.bar, 185, y + 20);
    lv_bar_set_range(g.bar, 0, 1000);
    return g;
}

static void ack_cb(lv_event_t *e)
{
    (void)e;
    acked_level = link_status()->level;
    hw_buzzer_set(false);
    link_send_ack_alarm();
}

static void build_state_tab(lv_obj_t *tab)
{
    tab_state = tab;
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

    lbl_doa = lv_label_create(tab);
    lv_obj_set_style_text_font(lbl_doa, &lv_font_montserrat_20, 0);
    lv_label_set_text(lbl_doa, "DOA: --");
    build_meter(false);

    g_audio = make_gauge(tab, "Audio (error AE)", 0);
    g_vib   = make_gauge(tab, "Vibracion RMS", 46);
    g_curr  = make_gauge(tab, "Corriente motor", 92);

    btn_ack = lv_btn_create(tab);
    lv_obj_set_size(btn_ack, 255, 40);
    lv_obj_set_pos(btn_ack, 185, 150);
    lv_obj_t *l = lv_label_create(btn_ack);
    lv_label_set_text(l, LV_SYMBOL_BELL " Reconocer alarma");
    lv_obj_center(l);
    lv_obj_add_event_cb(btn_ack, ack_cb, LV_EVENT_CLICKED, NULL);
}

static void gauge_set(gauge_t *g, uint16_t v, uint16_t warn, uint16_t anom,
                      uint8_t decimals, const char *unit)
{
    char buf[24];
    uint32_t max = anom ? (uint32_t)anom * 3u / 2u : 1000u;
    if (max < 1u)
        max = 1u;

    lv_bar_set_range(g->bar, 0, (int32_t)max);
    lv_bar_set_value(g->bar, v > max ? (int32_t)max : v, LV_ANIM_OFF);

    lv_color_t c = COL_NORMAL;
    if (anom && v >= anom)
        c = COL_FAIL;
    else if (warn && v >= warn)
        c = COL_WARN;
    lv_obj_set_style_bg_color(g->bar, c, LV_PART_INDICATOR);

    fmt_value(buf, sizeof(buf), v, decimals, unit);
    lv_label_set_text(g->value, buf);
}

/* ================================================================ umbrales */

static uint16_t *thr_field(dl_thresholds_t *t, const thr_row_t *r)
{
    return (uint16_t *)((uint8_t *)t + r->offset);
}

static void thr_set_status(const char *txt, lv_color_t c)
{
    lv_label_set_text(lbl_thr_status, txt);
    lv_obj_set_style_text_color(lbl_thr_status, c, 0);
}

static void thr_row_refresh(thr_row_t *r)
{
    char buf[24];
    uint16_t v = *thr_field(&thr_edit, r);
    lv_slider_set_value(r->slider, v, LV_ANIM_OFF);
    fmt_value(buf, sizeof(buf), v, r->decimals, r->unit);
    lv_label_set_text(r->value, buf);
}

static void thr_changed(thr_row_t *r, int32_t v)
{
    if (v < 0) v = 0;
    if (v > r->max) v = r->max;
    *thr_field(&thr_edit, r) = (uint16_t)v;
    thr_row_refresh(r);
    thr_dirty = memcmp(&thr_edit, &thr_remote, sizeof(thr_edit)) != 0;
    thr_set_status(thr_dirty ? "Cambios sin enviar" : "Sin cambios",
                   thr_dirty ? COL_ANOM : lv_palette_main(LV_PALETTE_GREY));
}

static void slider_cb(lv_event_t *e)
{
    thr_row_t *r = lv_event_get_user_data(e);
    int32_t v = lv_slider_get_value(lv_event_get_target(e));
    v = (v / r->step) * r->step;
    thr_changed(r, v);
}

static void minus_cb(lv_event_t *e)
{
    thr_row_t *r = lv_event_get_user_data(e);
    thr_changed(r, (int32_t)*thr_field(&thr_edit, r) - r->step);
}

static void plus_cb(lv_event_t *e)
{
    thr_row_t *r = lv_event_get_user_data(e);
    thr_changed(r, (int32_t)*thr_field(&thr_edit, r) + r->step);
}

static void thr_send_cb(lv_event_t *e)
{
    (void)e;
    if (thr_edit.audio_warn_m >= thr_edit.audio_anom_m ||
        thr_edit.vib_warn_mg >= thr_edit.vib_anom_mg ||
        thr_edit.current_warn_ma >= thr_edit.current_crit_ma) {
        thr_set_status("Advertencia debe ser menor", COL_FAIL);
        return;
    }
    link_send_thresholds(&thr_edit);
    thr_set_status("Enviado, esperando confirmacion", COL_WARN);
}

static void thr_reload_cb(lv_event_t *e)
{
    (void)e;
    thr_edit = thr_remote;
    for (size_t i = 0; i < N_ROWS; i++)
        thr_row_refresh(&rows[i]);
    thr_dirty = false;
    thr_set_status("Sin cambios", lv_palette_main(LV_PALETTE_GREY));
}

static void build_thr_tab(lv_obj_t *tab)
{
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tab, 2, 0);

    for (size_t i = 0; i < N_ROWS; i++) {
        thr_row_t *r = &rows[i];
        lv_obj_t *row = flex_row(tab);

        lv_obj_t *l = lv_label_create(row);
        lv_label_set_text(l, r->name);
        lv_obj_set_width(l, 100);

        lv_obj_t *bm = small_btn(row, LV_SYMBOL_MINUS, 34);
        lv_obj_add_event_cb(bm, minus_cb, LV_EVENT_CLICKED, r);

        r->slider = lv_slider_create(row);
        lv_obj_set_flex_grow(r->slider, 1);
        lv_slider_set_range(r->slider, 0, r->max);
        lv_obj_add_event_cb(r->slider, slider_cb, LV_EVENT_VALUE_CHANGED, r);

        lv_obj_t *bp = small_btn(row, LV_SYMBOL_PLUS, 34);
        lv_obj_add_event_cb(bp, plus_cb, LV_EVENT_CLICKED, r);

        r->value = lv_label_create(row);
        lv_obj_set_width(r->value, 70);
        lv_obj_set_style_text_align(r->value, LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_text(r->value, "--");
    }

    lv_obj_t *row = flex_row(tab);
    btn_thr_send = small_btn(row, LV_SYMBOL_UPLOAD " Enviar", 120);
    lv_obj_add_event_cb(btn_thr_send, thr_send_cb, LV_EVENT_CLICKED, NULL);
    btn_thr_reload = small_btn(row, LV_SYMBOL_REFRESH " Descartar", 130);
    lv_obj_add_event_cb(btn_thr_reload, thr_reload_cb, LV_EVENT_CLICKED, NULL);
    lbl_thr_status = lv_label_create(row);
    lv_obj_set_flex_grow(lbl_thr_status, 1);
    lv_label_set_long_mode(lbl_thr_status, LV_LABEL_LONG_WRAP);
    thr_set_status("Esperando umbrales de la H745", lv_palette_main(LV_PALETTE_GREY));

    lv_obj_add_state(btn_thr_send, LV_STATE_DISABLED);
    lv_obj_add_state(btn_thr_reload, LV_STATE_DISABLED);
}

/* ================================================================ ajustes */

static const char *map_mics[] = { "2 microfonos", "4 microfonos", "" };
static const char *map_mode[] = { "Barrido", "Enfoque fijo", "" };

static void focus_label(void)
{
    int16_t d = cfg_edit.focus_ddeg;
    lv_label_set_text_fmt(lbl_focus, "%d°", d / 10);
}

static void cfg_set_status(const char *txt, lv_color_t c)
{
    lv_label_set_text(lbl_cfg_status, txt);
    lv_obj_set_style_text_color(lbl_cfg_status, c, 0);
}

static void cfg_changed_cb(lv_event_t *e)
{
    (void)e;
    cfg_edit.n_mics = lv_btnmatrix_has_btn_ctrl(bm_mics, 1, LV_BTNMATRIX_CTRL_CHECKED) ? 4 : 2;
    cfg_edit.doa_mode = lv_btnmatrix_has_btn_ctrl(bm_mode, 1, LV_BTNMATRIX_CTRL_CHECKED)
                        ? DL_DOA_FIXED : DL_DOA_SCAN;
    cfg_edit.focus_ddeg = (int16_t)(lv_slider_get_value(sl_focus) * 10);
    focus_label();
    cfg_set_status("Cambios sin enviar", COL_ANOM);
}

static void cfg_send_cb(lv_event_t *e)
{
    (void)e;
    link_send_config(&cfg_edit);
    cfg_set_status("Enviado", COL_WARN);
}

static void bright_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    settings()->brightness = (uint8_t)lv_slider_get_value(sl_bright);
    hw_backlight_set(settings()->brightness);
    if (code == LV_EVENT_RELEASED)
        settings_save();
}

static void calib_cb(lv_event_t *e)
{
    (void)e;
    calib_start();
}

static lv_obj_t *make_bm(lv_obj_t *parent, const char **map)
{
    lv_obj_t *bm = lv_btnmatrix_create(parent);
    lv_btnmatrix_set_map(bm, map);
    lv_btnmatrix_set_btn_ctrl_all(bm, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_one_checked(bm, true);
    lv_btnmatrix_set_btn_ctrl(bm, 0, LV_BTNMATRIX_CTRL_CHECKED);
    lv_obj_set_size(bm, 300, 44);
    lv_obj_set_style_pad_all(bm, 4, 0);
    lv_obj_add_event_cb(bm, cfg_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    return bm;
}

static void build_cfg_tab(lv_obj_t *tab)
{
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tab, 4, 0);

    lv_obj_t *row = flex_row(tab);
    lv_obj_t *l = lv_label_create(row);
    lv_label_set_text(l, "Arreglo");
    lv_obj_set_width(l, 110);
    bm_mics = make_bm(row, map_mics);

    row = flex_row(tab);
    l = lv_label_create(row);
    lv_label_set_text(l, "Modo DOA");
    lv_obj_set_width(l, 110);
    bm_mode = make_bm(row, map_mode);

    row = flex_row(tab);
    l = lv_label_create(row);
    lv_label_set_text(l, "Enfoque");
    lv_obj_set_width(l, 110);
    sl_focus = lv_slider_create(row);
    lv_obj_set_flex_grow(sl_focus, 1);
    lv_slider_set_range(sl_focus, -180, 180);
    lv_obj_add_event_cb(sl_focus, cfg_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lbl_focus = lv_label_create(row);
    lv_obj_set_width(lbl_focus, 60);
    lv_obj_set_style_text_align(lbl_focus, LV_TEXT_ALIGN_RIGHT, 0);

    row = flex_row(tab);
    lv_obj_t *b = small_btn(row, LV_SYMBOL_UPLOAD " Enviar config.", 170);
    lv_obj_add_event_cb(b, cfg_send_cb, LV_EVENT_CLICKED, NULL);
    lbl_cfg_status = lv_label_create(row);
    cfg_set_status("Esperando config. de la H745", lv_palette_main(LV_PALETTE_GREY));

    row = flex_row(tab);
    l = lv_label_create(row);
    lv_label_set_text(l, "Brillo");
    lv_obj_set_width(l, 110);
    sl_bright = lv_slider_create(row);
    lv_obj_set_flex_grow(sl_bright, 1);
    lv_slider_set_range(sl_bright, 10, 100);
    lv_slider_set_value(sl_bright, settings()->brightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(sl_bright, bright_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(sl_bright, bright_cb, LV_EVENT_RELEASED, NULL);
    b = small_btn(row, "Calibrar tactil", 150);
    lv_obj_add_event_cb(b, calib_cb, LV_EVENT_CLICKED, NULL);

    lbl_info = lv_label_create(tab);
    lv_obj_set_style_text_color(lbl_info, lv_palette_main(LV_PALETTE_GREY), 0);

    cfg_edit.n_mics = 2;
    cfg_edit.doa_mode = DL_DOA_SCAN;
    cfg_edit.focus_ddeg = 0;
    focus_label();
}

static void cfg_apply_to_widgets(void)
{
    lv_btnmatrix_set_btn_ctrl(bm_mics, cfg_edit.n_mics == 4 ? 1 : 0, LV_BTNMATRIX_CTRL_CHECKED);
    lv_btnmatrix_set_btn_ctrl(bm_mode, cfg_edit.doa_mode == DL_DOA_FIXED ? 1 : 0,
                              LV_BTNMATRIX_CTRL_CHECKED);
    lv_slider_set_value(sl_focus, cfg_edit.focus_ddeg / 10, LV_ANIM_OFF);
    focus_label();
}

/* ================================================================ público */

void ui_init(uint16_t lcd_id)
{
    lcd_id_shown = lcd_id;

    lv_obj_t *scr = lv_scr_act();
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    top_bar = lv_obj_create(scr);
    lv_obj_remove_style_all(top_bar);
    lv_obj_set_size(top_bar, LV_HOR_RES, 44);
    lv_obj_set_style_bg_opa(top_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(top_bar, COL_NOLINK, 0);

    lbl_level = lv_label_create(top_bar);
    lv_obj_set_style_text_font(lbl_level, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_level, lv_color_white(), 0);
    lv_obj_align(lbl_level, LV_ALIGN_LEFT_MID, 10, 0);
    lv_label_set_text(lbl_level, "SIN ENLACE");

    lbl_link = lv_label_create(top_bar);
    lv_obj_set_style_text_color(lbl_link, lv_color_white(), 0);
    lv_obj_align(lbl_link, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_label_set_text(lbl_link, LV_SYMBOL_CLOSE " H745");

    lv_obj_t *tv = lv_tabview_create(scr, LV_DIR_TOP, 36);
    lv_obj_set_size(tv, LV_HOR_RES, LV_VER_RES - 44);
    lv_obj_set_pos(tv, 0, 44);

    build_state_tab(lv_tabview_add_tab(tv, "Estado"));
    build_thr_tab(lv_tabview_add_tab(tv, "Umbrales"));
    build_cfg_tab(lv_tabview_add_tab(tv, "Ajustes"));
}

static void update_top_and_alarm(bool alive, const dl_status_t *st)
{
    if (!alive) {
        lv_obj_set_style_bg_color(top_bar, COL_NOLINK, 0);
        lv_label_set_text(lbl_level, "SIN ENLACE");
        lv_label_set_text(lbl_link, LV_SYMBOL_CLOSE " H745");
        hw_buzzer_set(false);
        return;
    }

    lv_obj_set_style_bg_color(top_bar, level_color(st->level), 0);
    lv_label_set_text(lbl_level, level_text(st->level));
    lv_label_set_text_fmt(lbl_link, "%s ETH  " LV_SYMBOL_OK " H745",
                          (st->flags & DL_FLAG_ETH_LINK) ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE);

    /* Alarma sonora en ANOMALIA/FALLA hasta que se reconozca. Si el nivel
     * baja, se rearma para el siguiente aumento. */
    if (st->level < DL_LEVEL_ANOMALIA)
        acked_level = st->level;
    hw_buzzer_set(st->level >= DL_LEVEL_ANOMALIA && st->level > acked_level);
}

void ui_update(void)
{
    static uint32_t last;
    uint32_t now = lv_tick_get();

    if (calib_active() || now - last < UPDATE_MS)
        return;
    last = now;

    bool alive = link_alive();
    const dl_status_t *st = link_status();
    update_top_and_alarm(alive, st);

    /* Umbrales recibidos de la H745 */
    dl_thresholds_t t;
    if (link_thresholds(&t) && memcmp(&t, &thr_remote, sizeof(t)) != 0) {
        bool was_loaded = thr_loaded;
        thr_remote = t;
        thr_loaded = true;
        if (!thr_dirty || !was_loaded || memcmp(&t, &thr_edit, sizeof(t)) == 0) {
            thr_edit = t;
            for (size_t i = 0; i < N_ROWS; i++)
                thr_row_refresh(&rows[i]);
            thr_dirty = false;
            thr_set_status(was_loaded ? "Umbrales aplicados" : "Sin cambios", COL_NORMAL);
        }
        lv_obj_clear_state(btn_thr_send, LV_STATE_DISABLED);
        lv_obj_clear_state(btn_thr_reload, LV_STATE_DISABLED);
    }

    /* Configuración recibida de la H745 */
    dl_config_t c;
    if (link_config(&c) && !cfg_loaded) {
        cfg_edit = c;
        cfg_loaded = true;
        cfg_apply_to_widgets();
        cfg_set_status("Sincronizado", COL_NORMAL);
    }

    if (alive) {
        bool four = (st->flags & DL_FLAG_FOUR_MICS) != 0;
        if (four != meter_four_mics)
            build_meter(four);

        if (st->flags & DL_FLAG_DOA_VALID) {
            lv_meter_set_indicator_value(meter, needle, st->doa_ddeg / 10);
            int16_t d = st->doa_ddeg;
            int16_t ad = d < 0 ? (int16_t)-d : d;
            lv_label_set_text_fmt(lbl_doa, "DOA: %s%d.%d°", d < 0 ? "-" : "", ad / 10, ad % 10);
        } else {
            lv_label_set_text(lbl_doa, "DOA: sin fuente");
        }
        lv_obj_align_to(lbl_doa, meter, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);

        gauge_set(&g_audio, st->audio_score_m, thr_remote.audio_warn_m, thr_remote.audio_anom_m, 3, "");
        gauge_set(&g_vib, st->vib_rms_mg, thr_remote.vib_warn_mg, thr_remote.vib_anom_mg, 0, "mg");
        int16_t i_motor = st->current_ma[0];
        gauge_set(&g_curr, i_motor < 0 ? 0 : (uint16_t)i_motor,
                  thr_remote.current_warn_ma, thr_remote.current_crit_ma, 0, "mA");
    }

    const char *drv = lcd_driver() == LCD_DRV_ST7796S ? "ST7796S"
                    : lcd_driver() == LCD_DRV_ILI9488 ? "ILI9488" : "desconocido";
    lv_label_set_text_fmt(lbl_info, "LCD %s (0x%04X)  tramas %lu  errores %lu",
                          drv, lcd_id_shown,
                          (unsigned long)link_rx_frames(), (unsigned long)link_rx_errors());
}
