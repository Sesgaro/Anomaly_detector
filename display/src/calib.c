#include "calib.h"
#include "touch.h"
#include "settings.h"
#include "hw.h"
#include "board.h"
#include "lvgl.h"

#define N_POINTS     3
#define N_SAMPLES    16
#define RELEASE_MS   150u
#define CROSS_SIZE   30

static const int16_t px[N_POINTS] = { 40, LCD_WIDTH - 40, LCD_WIDTH / 2 };
static const int16_t py[N_POINTS] = { 40, LCD_HEIGHT / 2, LCD_HEIGHT - 40 };

enum { ST_IDLE, ST_WAIT_PRESS, ST_SAMPLING, ST_WAIT_RELEASE };

static int state = ST_IDLE;
static int point;
static uint32_t acc_x, acc_y;
static int n_acc;
static uint32_t release_t0;
static uint16_t raw_x[N_POINTS], raw_y[N_POINTS];

static lv_obj_t *scr, *prev_scr, *cross_h, *cross_v, *msg;

static void show_point(void)
{
    lv_obj_set_pos(cross_h, px[point] - CROSS_SIZE / 2, py[point] - 1);
    lv_obj_set_pos(cross_v, px[point] - 1, py[point] - CROSS_SIZE / 2);
    lv_label_set_text_fmt(msg, "Calibracion del tactil\nToque el centro de la cruz (%d/%d)",
                          point + 1, N_POINTS);
}

static lv_obj_t *make_line(lv_obj_t *parent, int w, int h)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    return o;
}

void calib_start(void)
{
    if (state != ST_IDLE)
        return;

    prev_scr = lv_scr_act();
    scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    msg = lv_label_create(scr);
    lv_obj_set_style_text_color(msg, lv_color_white(), 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, -20);

    cross_h = make_line(scr, CROSS_SIZE, 3);
    cross_v = make_line(scr, 3, CROSS_SIZE);

    point = 0;
    show_point();
    lv_scr_load(scr);
    state = ST_WAIT_PRESS;
}

static void finish(void)
{
    touch_cal_t cal;
    settings_t *s = settings();

    if (touch_compute_cal(px, py, raw_x, raw_y, &cal)) {
        touch_set_cal(&cal);
        s->cal = cal;
        s->cal_valid = 1;
        settings_save();
        hw_buzzer_beep(150);
    } else {
        /* Puntos degenerados: repetir */
        point = 0;
        show_point();
        lv_label_set_text(msg, "Calibracion invalida, repetir.\nToque el centro de la cruz (1/3)");
        state = ST_WAIT_PRESS;
        return;
    }

    lv_scr_load(prev_scr);
    lv_obj_del(scr);
    state = ST_IDLE;
}

void calib_poll(void)
{
    uint16_t rx, ry;
    bool down;

    if (state == ST_IDLE)
        return;

    down = touch_read_raw(&rx, &ry);

    switch (state) {
    case ST_WAIT_PRESS:
        if (down) {
            acc_x = acc_y = 0;
            n_acc = 0;
            state = ST_SAMPLING;
        }
        break;

    case ST_SAMPLING:
        if (!down) {
            state = ST_WAIT_PRESS;   /* se soltó antes de tiempo */
            break;
        }
        acc_x += rx;
        acc_y += ry;
        if (++n_acc >= N_SAMPLES) {
            raw_x[point] = (uint16_t)(acc_x / N_SAMPLES);
            raw_y[point] = (uint16_t)(acc_y / N_SAMPLES);
            hw_buzzer_beep(40);
            release_t0 = HAL_GetTick();
            state = ST_WAIT_RELEASE;
        }
        break;

    case ST_WAIT_RELEASE:
        if (down) {
            release_t0 = HAL_GetTick();
        } else if (HAL_GetTick() - release_t0 >= RELEASE_MS) {
            if (++point >= N_POINTS) {
                finish();
            } else {
                show_point();
                state = ST_WAIT_PRESS;
            }
        }
        break;
    }
}

bool calib_active(void)
{
    return state != ST_IDLE;
}
