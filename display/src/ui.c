/*
 * Interfaz de la pantalla, estilo Material 3 Expressive (modo oscuro, azul).
 *
 * Riel de navegación con 4 destinos:
 *   Estado   nivel de alerta, dial con el ángulo DOA y tarjetas de audio,
 *            vibración y corriente.
 *   Umbral   ajuste de los 6 umbrales (se envían a la STM32H745).
 *   Enfoque  arreglo de micrófonos, modo DOA y ángulo de enfoque.
 *   Sistema  brillo, LEDs de la rueda, calibración e información.
 *
 * Rueda: girar ajusta el valor activo de la página, clic confirma o pasa al
 * siguiente, mantener presionado cambia de página.
 */
#include "ui.h"
#include "ui_dial.h"
#include "theme.h"
#include "link.h"
#include "hw.h"
#include "knob.h"
#include "settings.h"
#include "calib.h"
#include "lcd.h"
#include "fwupdate.h"
#include "w25q.h"
#include <stddef.h>
#include <string.h>

#define UPDATE_MS   100u
#define RAIL_W      72
#define CONTENT_X   (RAIL_W + 4)
#define CONTENT_W   (LV_HOR_RES - CONTENT_X - 4)
#define N_PAGES     4

/* ---------------------------------------------------------------- general */

static lv_obj_t *pages[N_PAGES];
static lv_obj_t *rail_ind[N_PAGES], *rail_icon[N_PAGES];
static int cur_page;
static uint16_t lcd_id_shown;
static uint8_t acked_level;

/* ---------------------------------------------------------------- estado */

static lv_obj_t *st_card, *st_icon, *st_title, *st_sub, *st_ack;
static lv_obj_t *dial_doa, *lbl_doa_info;

typedef struct {
    lv_obj_t *card, *value, *bar;
} metric_t;
static metric_t m_audio, m_vib, m_curr;

/* ---------------------------------------------------------------- umbrales */

typedef struct {
    const char *name;
    const char *unit;
    uint16_t    max;
    uint16_t    step;
    uint8_t     decimals;   /* 3 = valor ×1000 */
    size_t      offset;
    lv_obj_t   *row, *value;
} thr_row_t;

static thr_row_t rows[] = {
    { "Audio · aviso",    "",    5000, 10, 3, offsetof(dl_thresholds_t, audio_warn_m)    },
    { "Audio · anomalía", "",    5000, 10, 3, offsetof(dl_thresholds_t, audio_anom_m)    },
    { "Vibración · aviso","mg",  4000, 10, 0, offsetof(dl_thresholds_t, vib_warn_mg)     },
    { "Vibración · anom.","mg",  4000, 10, 0, offsetof(dl_thresholds_t, vib_anom_mg)     },
    { "Corriente · aviso","mA",  8000, 50, 0, offsetof(dl_thresholds_t, current_warn_ma) },
    { "Corriente · crít.","mA",  8000, 50, 0, offsetof(dl_thresholds_t, current_crit_ma) },
};
#define N_ROWS (int)(sizeof(rows) / sizeof(rows[0]))

static dl_thresholds_t thr_edit, thr_remote;
static bool thr_loaded, thr_dirty;
static int thr_sel;
static lv_obj_t *dial_thr, *lbl_thr_status, *btn_thr_send, *btn_thr_reset;

/* ---------------------------------------------------------------- enfoque */

static dl_config_t cfg_edit;
static bool cfg_loaded;
static lv_obj_t *dial_focus, *seg_mics, *seg_mode, *lbl_cfg_status;

/* ---------------------------------------------------------------- sistema */

static lv_obj_t *sl_bright, *seg_led, *lbl_info;

/* ================================================================ helpers */

static void fmt_value(char *buf, size_t n, uint16_t v, uint8_t decimals, const char *unit)
{
    if (decimals == 3)
        lv_snprintf(buf, n, "%u.%03u%s%s", v / 1000u, v % 1000u, *unit ? " " : "", unit);
    else
        lv_snprintf(buf, n, "%u%s%s", v, *unit ? " " : "", unit);
}

static void level_colors(bool alive, uint8_t level, lv_color_t *cont, lv_color_t *on)
{
    if (!alive) {
        *cont = ST_OFF_CONT; *on = ST_OFF_ON;
        return;
    }
    switch (level) {
    case DL_LEVEL_NORMAL:      *cont = ST_OK_CONT;   *on = ST_OK_ON;   break;
    case DL_LEVEL_ADVERTENCIA: *cont = ST_WARN_CONT; *on = ST_WARN_ON; break;
    case DL_LEVEL_ANOMALIA:    *cont = ST_ANOM_CONT; *on = ST_ANOM_ON; break;
    default:                   *cont = ST_FAIL_CONT; *on = ST_FAIL_ON; break;
    }
}

static lv_obj_t *page_create(void)
{
    lv_obj_t *p = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(p);
    lv_obj_set_pos(p, CONTENT_X, 6);
    lv_obj_set_size(p, CONTENT_W, LV_VER_RES - 12);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(p, LV_OBJ_FLAG_HIDDEN);
    return p;
}

/* ================================================================ navegación */

static void show_page(int idx)
{
    cur_page = (idx + N_PAGES) % N_PAGES;
    for (int i = 0; i < N_PAGES; i++) {
        bool on = i == cur_page;
        if (on)
            lv_obj_clear_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_opa(rail_ind[i], on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(rail_icon[i], on ? M3_ON_SECONDARY_CONT : M3_ON_SURFACE_VARIANT, 0);
    }
}

static void rail_cb(lv_event_t *e)
{
    show_page((int)(intptr_t)lv_event_get_user_data(e));
}

static void build_rail(void)
{
    static const char *icons[N_PAGES] = { IC_HOME, IC_EDIT, IC_GPS, IC_SETTINGS };
    static const char *names[N_PAGES] = { "Estado", "Umbral", "Enfoque", "Sistema" };

    for (int i = 0; i < N_PAGES; i++) {
        lv_obj_t *item = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(item);
        lv_obj_set_size(item, RAIL_W, 64);
        lv_obj_set_pos(item, 0, 22 + i * 72);
        lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(item, rail_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        rail_ind[i] = lv_obj_create(item);
        lv_obj_remove_style_all(rail_ind[i]);
        lv_obj_set_size(rail_ind[i], 56, 32);
        lv_obj_align(rail_ind[i], LV_ALIGN_TOP_MID, 0, 4);
        lv_obj_set_style_radius(rail_ind[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(rail_ind[i], M3_SECONDARY_CONTAINER, 0);
        lv_obj_clear_flag(rail_ind[i], LV_OBJ_FLAG_CLICKABLE);

        rail_icon[i] = m3_label(rail_ind[i], icons[i], &font_icons, M3_ON_SURFACE_VARIANT);
        lv_obj_center(rail_icon[i]);

        lv_obj_t *l = m3_label(item, names[i], &font_body, M3_ON_SURFACE_VARIANT);
        lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 40);
    }
}

/* ================================================================ estado */

static void ack_cb(lv_event_t *e)
{
    (void)e;
    acked_level = link_status()->level;
    hw_buzzer_set(false);
    link_send_ack_alarm();
}

static metric_t make_metric(lv_obj_t *parent, const char *icon, const char *name, lv_coord_t y)
{
    metric_t m;
    m.card = m3_card(parent, M3_SURFACE_CONT, 20);
    lv_obj_set_style_pad_ver(m.card, 9, 0);
    lv_obj_set_pos(m.card, 200, y);
    lv_obj_set_size(m.card, 200, 72);

    lv_obj_t *l = m3_label(m.card, icon, &font_icons, M3_PRIMARY);
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, 0, 0);
    l = m3_label(m.card, name, &font_body, M3_ON_SURFACE_VARIANT);
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, 28, 2);

    m.value = m3_label(m.card, "--", &font_title, M3_ON_SURFACE);
    lv_obj_align(m.value, LV_ALIGN_LEFT_MID, 0, 3);

    m.bar = lv_bar_create(m.card);
    lv_obj_remove_style_all(m.bar);
    lv_obj_set_size(m.bar, 176, 6);
    lv_obj_align(m.bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(m.bar, M3_SURFACE_CONT_HIGHEST, 0);
    lv_obj_set_style_bg_opa(m.bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(m.bar, 3, 0);
    lv_obj_set_style_bg_color(m.bar, M3_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(m.bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(m.bar, 3, LV_PART_INDICATOR);
    lv_bar_set_range(m.bar, 0, 1000);
    return m;
}

static void build_state(lv_obj_t *p)
{
    st_card = m3_card(p, ST_OFF_CONT, 24);
    lv_obj_set_size(st_card, CONTENT_W, 64);
    lv_obj_set_style_pad_hor(st_card, 16, 0);

    st_icon = m3_label(st_card, IC_LINK, &font_icons, ST_OFF_ON);
    lv_obj_align(st_icon, LV_ALIGN_LEFT_MID, 0, 0);
    st_title = m3_label(st_card, "Sin enlace", &font_title, ST_OFF_ON);
    lv_obj_align(st_title, LV_ALIGN_TOP_LEFT, 34, -4);
    st_sub = m3_label(st_card, "Esperando datos de la STM32H745", &font_body, ST_OFF_ON);
    lv_obj_align(st_sub, LV_ALIGN_BOTTOM_LEFT, 34, 4);
    lv_obj_set_width(st_sub, CONTENT_W - 32 - 34);
    lv_label_set_long_mode(st_sub, LV_LABEL_LONG_DOT);

    st_ack = m3_button(st_card, "Reconocer", M3_PRIMARY, M3_ON_PRIMARY, 112, 40);
    lv_obj_align(st_ack, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(st_ack, ack_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(st_ack, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *dc = m3_card(p, M3_SURFACE_CONT, 28);
    lv_obj_set_pos(dc, 0, 72);
    lv_obj_set_size(dc, 192, 236);
    lv_obj_set_style_pad_all(dc, 4, 0);
    dial_doa = ui_dial_create(dc, 184);
    lv_obj_align(dial_doa, LV_ALIGN_TOP_MID, 0, 0);
    ui_dial_set_range(dial_doa, -90, 90);
    ui_dial_set_text(dial_doa, "--", "Ángulo DOA");
    lbl_doa_info = m3_label(dc, "2 micrófonos · barrido", &font_body, M3_ON_SURFACE_VARIANT);
    lv_obj_align(lbl_doa_info, LV_ALIGN_BOTTOM_MID, 0, -10);

    m_audio = make_metric(p, IC_AUDIO, "Audio (error AE)", 72);
    m_vib   = make_metric(p, IC_WIFI, "Vibración RMS", 154);
    m_curr  = make_metric(p, IC_CHARGE, "Corriente motor", 236);
}

static void metric_set(metric_t *m, uint16_t v, uint16_t warn, uint16_t anom,
                       uint8_t decimals, const char *unit)
{
    char buf[24];
    uint32_t max = anom ? (uint32_t)anom * 3u / 2u : 1000u;

    lv_bar_set_range(m->bar, 0, (int32_t)max);
    lv_bar_set_value(m->bar, v > max ? (int32_t)max : v, LV_ANIM_OFF);

    lv_color_t c = M3_PRIMARY;
    if (anom && v >= anom)
        c = ST_FAIL;
    else if (warn && v >= warn)
        c = ST_WARN;
    lv_obj_set_style_bg_color(m->bar, c, LV_PART_INDICATOR);

    fmt_value(buf, sizeof(buf), v, decimals, unit);
    lv_label_set_text(m->value, buf);
}

/* ================================================================ umbrales */

static uint16_t *thr_field(dl_thresholds_t *t, const thr_row_t *r)
{
    return (uint16_t *)((uint8_t *)t + r->offset);
}

static void thr_status(const char *txt, lv_color_t c)
{
    lv_label_set_text(lbl_thr_status, txt);
    lv_obj_set_style_text_color(lbl_thr_status, c, 0);
}

static void thr_refresh(void)
{
    char buf[24];

    for (int i = 0; i < N_ROWS; i++) {
        thr_row_t *r = &rows[i];
        bool sel = i == thr_sel;
        fmt_value(buf, sizeof(buf), *thr_field(&thr_edit, r), r->decimals, r->unit);
        lv_label_set_text(r->value, thr_loaded ? buf : "--");
        lv_obj_set_style_bg_opa(r->row, sel ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }

    thr_row_t *r = &rows[thr_sel];
    uint16_t v = *thr_field(&thr_edit, r);
    if (r->decimals == 3)
        lv_snprintf(buf, sizeof(buf), "%u.%02u", v / 1000u, (v % 1000u) / 10u);
    else
        lv_snprintf(buf, sizeof(buf), "%u", v);
    ui_dial_set_range(dial_thr, 0, r->max);
    ui_dial_set_value(dial_thr, v);
    ui_dial_set_text(dial_thr, thr_loaded ? buf : "--", r->name);
}

static void thr_change(int32_t delta_steps)
{
    if (!thr_loaded)
        return;
    thr_row_t *r = &rows[thr_sel];
    int32_t v = (int32_t)*thr_field(&thr_edit, r) + delta_steps * r->step;
    if (v < 0) v = 0;
    if (v > r->max) v = r->max;
    *thr_field(&thr_edit, r) = (uint16_t)v;
    thr_dirty = memcmp(&thr_edit, &thr_remote, sizeof(thr_edit)) != 0;
    thr_status(thr_dirty ? "Cambios sin enviar" : "Sin cambios",
               thr_dirty ? ST_ANOM : M3_ON_SURFACE_VARIANT);
    thr_refresh();
}

static void thr_row_cb(lv_event_t *e)
{
    thr_sel = (int)(intptr_t)lv_event_get_user_data(e);
    thr_refresh();
}

static void thr_step_cb(lv_event_t *e)
{
    static const int8_t steps[] = { -10, -1, 1, 10 };
    lv_obj_t *seg = lv_event_get_target(e);
    int i = m3_segmented_selected(seg);
    if (i >= 0 && i < 4)
        thr_change(steps[i]);
    m3_segmented_select(seg, -1);   /* los botones de paso no quedan marcados */
}

static void thr_send(void)
{
    if (!thr_loaded)
        return;
    if (thr_edit.audio_warn_m >= thr_edit.audio_anom_m ||
        thr_edit.vib_warn_mg >= thr_edit.vib_anom_mg ||
        thr_edit.current_warn_ma >= thr_edit.current_crit_ma) {
        thr_status("El aviso debe ser menor que la anomalía", ST_FAIL);
        return;
    }
    link_send_thresholds(&thr_edit);
    thr_status("Enviado, esperando confirmación", ST_WARN);
}

static void thr_send_cb(lv_event_t *e) { (void)e; thr_send(); }

static void thr_reset_cb(lv_event_t *e)
{
    (void)e;
    thr_edit = thr_remote;
    thr_dirty = false;
    thr_status("Sin cambios", M3_ON_SURFACE_VARIANT);
    thr_refresh();
}

static void build_thresholds(lv_obj_t *p)
{
    static const char *steps[] = { "-10", "-1", "+1", "+10" };

    lv_obj_t *dc = m3_card(p, M3_SURFACE_CONT, 28);
    lv_obj_set_size(dc, 192, 308);
    lv_obj_set_style_pad_all(dc, 4, 0);
    dial_thr = ui_dial_create(dc, 184);
    lv_obj_align(dial_thr, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *seg = m3_segmented(dc, steps, 4, 42, 40, thr_step_cb);
    lv_obj_align(seg, LV_ALIGN_TOP_MID, 0, 192);

    lbl_thr_status = m3_label(dc, "", &font_body, M3_ON_SURFACE_VARIANT);
    lv_obj_set_width(lbl_thr_status, 176);
    lv_label_set_long_mode(lbl_thr_status, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(lbl_thr_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl_thr_status, LV_ALIGN_TOP_MID, 0, 244);

    lv_obj_t *lc = m3_card(p, M3_SURFACE_CONT, 28);
    lv_obj_set_pos(lc, 200, 0);
    lv_obj_set_size(lc, 200, 254);
    lv_obj_set_style_pad_all(lc, 8, 0);
    lv_obj_set_flex_flow(lc, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(lc, 2, 0);

    for (int i = 0; i < N_ROWS; i++) {
        thr_row_t *r = &rows[i];
        r->row = lv_obj_create(lc);
        lv_obj_remove_style_all(r->row);
        lv_obj_set_size(r->row, 184, 37);
        lv_obj_set_style_radius(r->row, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(r->row, M3_SECONDARY_CONTAINER, 0);
        lv_obj_set_style_pad_hor(r->row, 12, 0);
        lv_obj_add_flag(r->row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(r->row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(r->row, thr_row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *n = m3_label(r->row, r->name, &font_body, M3_ON_SURFACE);
        lv_obj_align(n, LV_ALIGN_LEFT_MID, 0, -7);
        lv_obj_set_style_text_color(n, M3_ON_SURFACE_VARIANT, 0);
        r->value = m3_label(r->row, "--", &font_body, M3_ON_SURFACE);
        lv_obj_align(r->value, LV_ALIGN_LEFT_MID, 0, 9);
    }

    btn_thr_send = m3_button(p, IC_UPLOAD "  Enviar", M3_PRIMARY, M3_ON_PRIMARY, 104, 46);
    lv_obj_set_pos(btn_thr_send, 200, 262);
    lv_obj_add_event_cb(btn_thr_send, thr_send_cb, LV_EVENT_CLICKED, NULL);
    btn_thr_reset = m3_button(p, "Descartar", M3_SECONDARY_CONTAINER, M3_ON_SECONDARY_CONT, 90, 46);
    lv_obj_set_pos(btn_thr_reset, 310, 262);
    lv_obj_add_event_cb(btn_thr_reset, thr_reset_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_add_state(btn_thr_send, LV_STATE_DISABLED);
    lv_obj_add_state(btn_thr_reset, LV_STATE_DISABLED);
    thr_status("Esperando umbrales de la H745", M3_ON_SURFACE_VARIANT);
    thr_refresh();
}

/* ================================================================ enfoque */

static void cfg_status(const char *txt, lv_color_t c)
{
    lv_label_set_text(lbl_cfg_status, txt);
    lv_obj_set_style_text_color(lbl_cfg_status, c, 0);
}

static void cfg_refresh(void)
{
    char buf[12];
    int lim = cfg_edit.n_mics == 4 ? 180 : 90;
    int deg = cfg_edit.focus_ddeg / 10;

    if (deg > lim) deg = lim;
    if (deg < -lim) deg = -lim;
    cfg_edit.focus_ddeg = (int16_t)(deg * 10);

    lv_snprintf(buf, sizeof(buf), "%d°", deg);
    ui_dial_set_range(dial_focus, -lim, lim);
    ui_dial_set_value(dial_focus, deg);
    ui_dial_set_text(dial_focus, buf, cfg_edit.doa_mode == DL_DOA_FIXED ? "Enfoque fijo" : "Barrido");
    m3_segmented_select(seg_mics, cfg_edit.n_mics == 4 ? 1 : 0);
    m3_segmented_select(seg_mode, cfg_edit.doa_mode == DL_DOA_FIXED ? 1 : 0);
}

static void cfg_changed(void)
{
    cfg_refresh();
    cfg_status("Cambios sin enviar", ST_ANOM);
}

static void focus_change(int deg)
{
    cfg_edit.focus_ddeg = (int16_t)(cfg_edit.focus_ddeg + deg * 10);
    cfg_changed();
}

static void focus_step_cb(lv_event_t *e)
{
    static const int8_t steps[] = { -10, -1, 1, 10 };
    lv_obj_t *seg = lv_event_get_target(e);
    int i = m3_segmented_selected(seg);
    if (i >= 0 && i < 4)
        focus_change(steps[i]);
    m3_segmented_select(seg, -1);
}

static void mics_cb(lv_event_t *e)
{
    cfg_edit.n_mics = m3_segmented_selected(lv_event_get_target(e)) == 1 ? 4 : 2;
    cfg_changed();
}

static void mode_cb(lv_event_t *e)
{
    cfg_edit.doa_mode = m3_segmented_selected(lv_event_get_target(e)) == 1 ? DL_DOA_FIXED : DL_DOA_SCAN;
    cfg_changed();
}

static void preset_cb(lv_event_t *e)
{
    cfg_edit.focus_ddeg = (int16_t)((intptr_t)lv_event_get_user_data(e) * 10);
    cfg_edit.doa_mode = DL_DOA_FIXED;
    cfg_changed();
}

static void cfg_send(void)
{
    link_send_config(&cfg_edit);
    cfg_status("Enviado", ST_WARN);
}

static void cfg_send_cb(lv_event_t *e) { (void)e; cfg_send(); }

static lv_obj_t *section_title(lv_obj_t *parent, const char *txt)
{
    lv_obj_t *l = m3_label(parent, txt, &font_body, M3_PRIMARY);
    return l;
}

static void build_focus(lv_obj_t *p)
{
    static const char *steps[] = { "-10", "-1", "+1", "+10" };
    static const char *mics[] = { "2 mic", "4 mic" };
    static const char *mode[] = { "Barrido", "Fijo" };
    static const int presets[] = { -90, -45, 0, 45, 90 };

    lv_obj_t *dc = m3_card(p, M3_SURFACE_CONT, 28);
    lv_obj_set_size(dc, 192, 308);
    lv_obj_set_style_pad_all(dc, 4, 0);
    dial_focus = ui_dial_create(dc, 184);
    lv_obj_align(dial_focus, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_t *seg = m3_segmented(dc, steps, 4, 42, 40, focus_step_cb);
    lv_obj_align(seg, LV_ALIGN_TOP_MID, 0, 192);
    lbl_cfg_status = m3_label(dc, "Esperando config. de la H745", &font_body, M3_ON_SURFACE_VARIANT);
    lv_obj_set_width(lbl_cfg_status, 176);
    lv_label_set_long_mode(lbl_cfg_status, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(lbl_cfg_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl_cfg_status, LV_ALIGN_TOP_MID, 0, 244);

    lv_obj_t *rc = m3_card(p, M3_SURFACE_CONT, 28);
    lv_obj_set_pos(rc, 200, 0);
    lv_obj_set_size(rc, 200, 254);
    lv_obj_set_style_pad_all(rc, 14, 0);
    lv_obj_set_flex_flow(rc, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(rc, 6, 0);

    section_title(rc, "Arreglo");
    seg_mics = m3_segmented(rc, mics, 2, 84, 38, mics_cb);
    section_title(rc, "Modo DOA");
    seg_mode = m3_segmented(rc, mode, 2, 84, 38, mode_cb);
    section_title(rc, "Atajos");
    lv_obj_t *row = lv_obj_create(rc);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, 172, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(row, 4, 0);
    lv_obj_set_style_pad_row(row, 4, 0);
    for (int i = 0; i < 5; i++) {
        char t[8];
        lv_snprintf(t, sizeof(t), "%d°", presets[i]);
        lv_obj_t *b = m3_button(row, t, M3_SURFACE_CONT_HIGHEST, M3_ON_SURFACE, i < 3 ? 54 : 83, 32);
        lv_obj_add_event_cb(b, preset_cb, LV_EVENT_CLICKED, (void *)(intptr_t)presets[i]);
    }

    lv_obj_t *b = m3_button(p, IC_UPLOAD "  Enviar configuración", M3_PRIMARY, M3_ON_PRIMARY, 200, 46);
    lv_obj_set_pos(b, 200, 262);
    lv_obj_add_event_cb(b, cfg_send_cb, LV_EVENT_CLICKED, NULL);

    cfg_edit.n_mics = 2;
    cfg_edit.doa_mode = DL_DOA_SCAN;
    cfg_refresh();
}

/* ================================================================ sistema */

static void bright_cb(lv_event_t *e)
{
    settings()->brightness = (uint8_t)lv_slider_get_value(sl_bright);
    hw_backlight_set(settings()->brightness);
    if (lv_event_get_code(e) == LV_EVENT_RELEASED)
        settings_save();
}

static void led_cb(lv_event_t *e)
{
    settings()->led_on = m3_segmented_selected(lv_event_get_target(e)) == 0;
    settings_save();
}

static void calib_cb(lv_event_t *e)
{
    (void)e;
    calib_start();
}

static void build_system(lv_obj_t *p)
{
    static const char *led[] = { "Encendidos", "Apagados" };

    lv_obj_t *c = m3_card(p, M3_SURFACE_CONT, 28);
    lv_obj_set_size(c, CONTENT_W, 308);
    lv_obj_set_style_pad_all(c, 18, 0);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c, 8, 0);

    section_title(c, "Brillo de la pantalla");
    sl_bright = lv_slider_create(c);
    lv_obj_set_size(sl_bright, 340, 16);
    lv_slider_set_range(sl_bright, 10, 100);
    lv_slider_set_value(sl_bright, settings()->brightness, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(sl_bright, M3_SECONDARY_CONTAINER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sl_bright, M3_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sl_bright, M3_PRIMARY, LV_PART_KNOB);
    lv_obj_set_style_pad_hor(sl_bright, 2, LV_PART_KNOB);
    lv_obj_set_style_pad_ver(sl_bright, 10, LV_PART_KNOB);
    lv_obj_set_style_radius(sl_bright, 2, LV_PART_KNOB);
    lv_obj_add_event_cb(sl_bright, bright_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(sl_bright, bright_cb, LV_EVENT_RELEASED, NULL);

    section_title(c, "LEDs de la rueda (D1 y D2)");
    seg_led = m3_segmented(c, led, 2, 120, 38, led_cb);
    m3_segmented_select(seg_led, settings()->led_on ? 0 : 1);

    section_title(c, "Pantalla táctil");
    lv_obj_t *b = m3_button(c, "Calibrar", M3_SECONDARY_CONTAINER, M3_ON_SECONDARY_CONT, 120, 40);
    lv_obj_add_event_cb(b, calib_cb, LV_EVENT_CLICKED, NULL);

    section_title(c, "Información");
    lbl_info = m3_label(c, "", &font_body, M3_ON_SURFACE_VARIANT);
}

/* ================================================================ rueda */

static void knob_handle(void)
{
    int16_t s = knob_take_steps();
    knob_event_t ev = knob_poll_button();

    if (ev == KNOB_EV_LONG) {
        show_page(cur_page + 1);
        hw_buzzer_beep(20);
        return;
    }

    switch (cur_page) {
    case 0:
        if (ev == KNOB_EV_CLICK && !lv_obj_has_flag(st_ack, LV_OBJ_FLAG_HIDDEN))
            ack_cb(NULL);
        break;
    case 1:
        if (s)
            thr_change(s);
        if (ev == KNOB_EV_CLICK) {
            thr_sel = (thr_sel + 1) % N_ROWS;
            thr_refresh();
        }
        break;
    case 2:
        if (s)
            focus_change(s);
        if (ev == KNOB_EV_CLICK)
            cfg_send();
        break;
    case 3:
        if (s) {
            int v = (int)settings()->brightness + s * 5;
            v = v < 10 ? 10 : (v > 100 ? 100 : v);
            settings()->brightness = (uint8_t)v;
            lv_slider_set_value(sl_bright, v, LV_ANIM_OFF);
            hw_backlight_set((uint8_t)v);
        }
        if (ev == KNOB_EV_CLICK) {
            settings()->led_on = !settings()->led_on;
            m3_segmented_select(seg_led, settings()->led_on ? 0 : 1);
            settings_save();
        }
        break;
    }
}

/* LEDs de la rueda según el nivel de alerta */
static uint32_t scale_rgb(uint32_t rgb, unsigned pct)
{
    uint32_t r = ((rgb >> 16) & 0xFF) * pct / 100u;
    uint32_t g = ((rgb >> 8) & 0xFF) * pct / 100u;
    uint32_t b = (rgb & 0xFF) * pct / 100u;
    return (r << 16) | (g << 8) | b;
}

static void leds_update(bool alive, uint8_t level)
{
    uint32_t c;
    unsigned pct = 25;

    if (!settings()->led_on || calib_active()) {
        knob_set_leds(0, 0);
        return;
    }
    if (fwupdate_active()) {
        c = 0x7F4FFF;                           /* violeta: actualizando */
    } else if (!alive) {
        c = 0x2050C0;
        pct = 8;
    } else {
        switch (level) {
        case DL_LEVEL_NORMAL:      c = 0x3070FF; break;
        case DL_LEVEL_ADVERTENCIA: c = 0xFFA000; break;
        case DL_LEVEL_ANOMALIA:    c = 0xFF4000; break;
        default:
            c = 0xFF0000;
            if ((lv_tick_get() / 250u) & 1u)    /* FALLA: parpadeo */
                pct = 0;
            break;
        }
    }
    uint32_t v = scale_rgb(c, pct);
    knob_set_leds(v, v);
}

/* ================================================================ público */

void ui_init(uint16_t lcd_id)
{
    lcd_id_shown = lcd_id;
    theme_init();
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);

    build_rail();
    for (int i = 0; i < N_PAGES; i++)
        pages[i] = page_create();
    build_state(pages[0]);
    build_thresholds(pages[1]);
    build_focus(pages[2]);
    build_system(pages[3]);
    show_page(0);
}

static void update_state(bool alive, const dl_status_t *st)
{
    lv_color_t cont, on;
    char buf[48];

    level_colors(alive, st->level, &cont, &on);
    lv_obj_set_style_bg_color(st_card, cont, 0);
    lv_obj_set_style_text_color(st_icon, on, 0);
    lv_obj_set_style_text_color(st_title, on, 0);
    lv_obj_set_style_text_color(st_sub, on, 0);

    if (!alive) {
        lv_label_set_text(st_icon, IC_LINK);
        lv_label_set_text(st_title, "Sin enlace");
        lv_label_set_text(st_sub, "Esperando datos de la STM32H745");
        lv_obj_add_flag(st_ack, LV_OBJ_FLAG_HIDDEN);
        hw_buzzer_set(false);
        ui_dial_set_accent(dial_doa, M3_OUTLINE, M3_SURFACE_CONT_HIGHEST);
        return;
    }

    static const char *titles[] = { "Normal", "Advertencia", "Anomalía", "Falla" };
    uint8_t lvl = st->level > DL_LEVEL_FALLA ? DL_LEVEL_FALLA : st->level;
    lv_label_set_text(st_icon, lvl == DL_LEVEL_NORMAL ? IC_OK : (lvl == DL_LEVEL_FALLA ? IC_CLOSE : IC_WARNING));
    lv_label_set_text(st_title, titles[lvl]);
    /* Alarma sonora y botón de reconocer en ANOMALIA/FALLA */
    if (st->level < DL_LEVEL_ANOMALIA)
        acked_level = st->level;
    bool alarm = st->level >= DL_LEVEL_ANOMALIA && st->level > acked_level;
    hw_buzzer_set(alarm);
    if (alarm) {
        lv_obj_clear_flag(st_ack, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_width(st_sub, CONTENT_W - 32 - 34 - 118);
        lv_label_set_text(st_sub, "Pulsa la rueda o el botón");
    } else {
        lv_obj_add_flag(st_ack, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_width(st_sub, CONTENT_W - 32 - 34);
        lv_snprintf(buf, sizeof(buf), "H745 en línea · Ethernet %s",
                    (st->flags & DL_FLAG_ETH_LINK) ? "conectado" : "sin enlace");
        lv_label_set_text(st_sub, buf);
    }

    /* Dial DOA */
    bool four = (st->flags & DL_FLAG_FOUR_MICS) != 0;
    ui_dial_set_range(dial_doa, four ? -180 : -90, four ? 180 : 90);
    ui_dial_set_accent(dial_doa, lvl == DL_LEVEL_NORMAL ? M3_PRIMARY : on, M3_PRIMARY_CONTAINER);
    if (st->flags & DL_FLAG_DOA_VALID) {
        ui_dial_set_value(dial_doa, st->doa_ddeg / 10);
        lv_snprintf(buf, sizeof(buf), "%d°", st->doa_ddeg / 10);
        ui_dial_set_text(dial_doa, buf, "Ángulo DOA");
    } else {
        ui_dial_set_text(dial_doa, "--", "Sin fuente");
    }
    lv_label_set_text_fmt(lbl_doa_info, "%d micrófonos · %s", four ? 4 : 2,
                          cfg_edit.doa_mode == DL_DOA_FIXED ? "fijo" : "barrido");

    metric_set(&m_audio, st->audio_score_m, thr_remote.audio_warn_m, thr_remote.audio_anom_m, 3, "");
    metric_set(&m_vib, st->vib_rms_mg, thr_remote.vib_warn_mg, thr_remote.vib_anom_mg, 0, "mg");
    int16_t i_motor = st->current_ma[0];
    metric_set(&m_curr, i_motor < 0 ? 0 : (uint16_t)i_motor,
               thr_remote.current_warn_ma, thr_remote.current_crit_ma, 0, "mA");
}

void ui_update(void)
{
    static uint32_t last;
    uint32_t now = lv_tick_get();
    bool alive = link_alive();
    const dl_status_t *st = link_status();

    if (calib_active())
        return;

    knob_handle();
    leds_update(alive, st->level);

    if (now - last < UPDATE_MS)
        return;
    last = now;

    update_state(alive, st);

    /* Umbrales recibidos de la H745 */
    dl_thresholds_t t;
    if (link_thresholds(&t) && (!thr_loaded || memcmp(&t, &thr_remote, sizeof(t)) != 0)) {
        bool was_loaded = thr_loaded;
        thr_remote = t;
        thr_loaded = true;
        if (!thr_dirty || memcmp(&t, &thr_edit, sizeof(t)) == 0) {
            thr_edit = t;
            thr_dirty = false;
            thr_status(was_loaded ? "Umbrales aplicados" : "Sincronizado", ST_OK_ON);
        }
        lv_obj_clear_state(btn_thr_send, LV_STATE_DISABLED);
        lv_obj_clear_state(btn_thr_reset, LV_STATE_DISABLED);
        thr_refresh();
    }

    /* Configuración recibida de la H745 */
    dl_config_t c;
    if (link_config(&c) && !cfg_loaded) {
        cfg_edit = c;
        cfg_loaded = true;
        cfg_refresh();
        cfg_status("Sincronizado", ST_OK_ON);
    }

    const char *drv = lcd_driver() == LCD_DRV_ST7796S ? "ST7796S"
                    : lcd_driver() == LCD_DRV_ILI9488 ? "ILI9488" : "desconocido";
    lv_label_set_text_fmt(lbl_info,
                          "Firmware %s · CRC %08lX\nLCD %s (0x%04X) · flash %06lX (%lu KB)\n"
                          "Tramas %lu · errores %lu",
                          DISPLAY_FW_VERSION, (unsigned long)fwupdate_running_crc(),
                          drv, lcd_id_shown, (unsigned long)w25q_jedec(),
                          (unsigned long)(w25q_size() / 1024u),
                          (unsigned long)link_rx_frames(), (unsigned long)link_rx_errors());
}
