#include "ui_dial.h"
#include "theme.h"
#include <math.h>
#include <string.h>

#define SWEEP_DEG     270.0f
#define START_DEG     135.0f      /* 0° = derecha, sentido horario (como LVGL) */
#define TRACK_W       8
#define WAVE_AMP      3.0f        /* px */
#define WAVE_LEN      26.0f       /* px por onda (como la app) */
#define WAVE_STEPS    120
#define N_TICKS       24
#define COOKIE_LOBES  9
#define COOKIE_DEPTH  0.09f       /* profundidad de los lóbulos */
#define PI_F          3.14159265f

typedef struct {
    int32_t    min, max, value;
    lv_color_t accent, cookie;
    lv_obj_t  *lbl_value, *lbl_caption;
} dial_t;

/* Máscara de la galleta (alfa de 4 bits, con antialias), calculada una vez
 * y compartida por todos los diales del mismo tamaño. */
#define COOKIE_MAX_D  120
static uint8_t     cookie_buf[(COOKIE_MAX_D * COOKIE_MAX_D + 1) / 2];
static lv_img_dsc_t cookie_img;
static int          cookie_r;

static void cookie_build(int r)
{
    int d = 2 * r + 2;
    if (r == cookie_r || d > COOKIE_MAX_D)
        return;
    cookie_r = r;
    memset(cookie_buf, 0, sizeof(cookie_buf));
    float c = d / 2.0f;
    for (int y = 0; y < d; y++) {
        for (int x = 0; x < d; x++) {
            float dx = x + 0.5f - c, dy = y + 0.5f - c;
            float dist = sqrtf(dx * dx + dy * dy);
            float t = atan2f(dy, dx);
            float rb = r * (1.0f - COOKIE_DEPTH / 2 + COOKIE_DEPTH / 2 * cosf(COOKIE_LOBES * t));
            float a = rb - dist + 0.5f;
            int v = a <= 0.0f ? 0 : (a >= 1.0f ? 15 : (int)(a * 15.0f + 0.5f));
            int i = y * d + x;
            cookie_buf[i / 2] |= (uint8_t)(v << ((i & 1) ? 0 : 4));
        }
    }
    cookie_img.header.cf = LV_IMG_CF_ALPHA_4BIT;
    cookie_img.header.w = (uint32_t)d;
    cookie_img.header.h = (uint32_t)d;
    cookie_img.data_size = (uint32_t)((d * d + 1) / 2);
    cookie_img.data = cookie_buf;
}

static float val_to_deg(const dial_t *d, float v)
{
    if (d->max <= d->min)
        return START_DEG;
    float f = (v - d->min) / (float)(d->max - d->min);
    f = f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
    return START_DEG + SWEEP_DEG * f;
}

static lv_point_t polar(lv_coord_t cx, lv_coord_t cy, float r, float deg)
{
    float a = deg * PI_F / 180.0f;
    lv_point_t p = { (lv_coord_t)lroundf(cx + r * cosf(a)), (lv_coord_t)lroundf(cy + r * sinf(a)) };
    return p;
}

static void circle(lv_draw_ctx_t *ctx, lv_draw_rect_dsc_t *rd, lv_point_t c, int r, lv_color_t col)
{
    rd->bg_color = col;
    lv_area_t a = { (lv_coord_t)(c.x - r), (lv_coord_t)(c.y - r),
                    (lv_coord_t)(c.x + r), (lv_coord_t)(c.y + r) };
    lv_draw_rect(ctx, rd, &a);
}

static void draw_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    dial_t *d = lv_obj_get_user_data(obj);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);

    lv_coord_t size = lv_area_get_width(&a);
    lv_coord_t cx = (lv_coord_t)(a.x1 + size / 2);
    lv_coord_t cy = (lv_coord_t)(a.y1 + lv_area_get_height(&a) / 2);
    float track_r = size / 2.0f - 10.0f;
    int body_r = (int)(track_r - 26.0f);
    lv_point_t c = { cx, cy };

    /* 1. Pista inactiva: arco completo del recorrido */
    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init(&arc);
    arc.color = M3_SURFACE_CONT_HIGHEST;
    arc.width = TRACK_W;
    arc.rounded = 1;
    lv_draw_arc(ctx, &arc, &c, (uint16_t)(track_r + TRACK_W / 2), (uint16_t)START_DEG,
                (uint16_t)((int)(START_DEG + SWEEP_DEG) % 360));

    /* 2. Pista activa ondulada: desde 0 si el rango lo incluye, si no desde
     *    el mínimo; la onda nace y muere plana */
    float origin = (d->min <= 0 && d->max >= 0) ? 0.0f : (float)d->min;
    float a0 = val_to_deg(d, origin), a1 = val_to_deg(d, (float)d->value);
    if (fabsf(a1 - a0) > 0.5f) {
        lv_draw_line_dsc_t ln;
        lv_draw_line_dsc_init(&ln);
        ln.color = d->accent;
        ln.width = TRACK_W;
        ln.round_start = 1;
        ln.round_end = 1;
        float waves = roundf(2.0f * PI_F * track_r / WAVE_LEN);
        int steps = (int)(fabsf(a1 - a0) / 270.0f * WAVE_STEPS) + 4;
        lv_point_t prev = polar(cx, cy, track_r, a0);
        for (int i = 1; i <= steps; i++) {
            float t = a0 + (a1 - a0) * i / steps;
            float edge = fminf(1.0f, fminf((float)i, (float)(steps - i)) / 6.0f);
            float r = track_r + WAVE_AMP * edge * sinf(t * PI_F / 180.0f * waves);
            lv_point_t p = polar(cx, cy, r, t);
            lv_draw_line(ctx, &ln, &prev, &p);
            prev = p;
        }
    }

    /* 3. Marcas: cada 1/24 del recorrido, las principales más largas */
    lv_draw_line_dsc_t tk;
    lv_draw_line_dsc_init(&tk);
    tk.color = M3_OUTLINE_VARIANT;
    tk.round_start = tk.round_end = 1;
    for (int i = 0; i <= N_TICKS; i++) {
        float t = START_DEG + SWEEP_DEG * i / N_TICKS;
        bool major = (i % 6) == 0;
        float r0 = track_r - 10.0f, r1 = r0 - (major ? 8.0f : 4.0f);
        tk.width = major ? 3 : 2;
        lv_point_t p0 = polar(cx, cy, r0, t), p1 = polar(cx, cy, r1, t);
        lv_draw_line(ctx, &tk, &p0, &p1);
    }

    /* 4. Cuerpo de galleta (9 lóbulos suaves) */
    cookie_build(body_r);
    if (cookie_r == body_r) {
        lv_draw_img_dsc_t id;
        lv_draw_img_dsc_init(&id);
        id.recolor = d->cookie;
        id.recolor_opa = LV_OPA_COVER;
        lv_area_t ia = { (lv_coord_t)(cx - body_r - 1), (lv_coord_t)(cy - body_r - 1),
                         (lv_coord_t)(cx + body_r), (lv_coord_t)(cy + body_r) };
        lv_draw_img(ctx, &id, &ia, &cookie_img);
    }

    lv_draw_rect_dsc_t rd;
    lv_draw_rect_dsc_init(&rd);
    rd.radius = LV_RADIUS_CIRCLE;

    /* 5. Indicador: pastilla en el borde de la galleta apuntando al valor */
    circle(ctx, &rd, polar(cx, cy, body_r - 7.0f, a1), 4, d->accent);

    /* 6. Punta en la pista exterior */
    lv_point_t hp = polar(cx, cy, track_r, a1);
    circle(ctx, &rd, hp, 9, d->accent);
    circle(ctx, &rd, hp, 3, M3_ON_PRIMARY);
}

static void delete_cb(lv_event_t *e)
{
    lv_mem_free(lv_obj_get_user_data(lv_event_get_target(e)));
}

lv_obj_t *ui_dial_create(lv_obj_t *parent, lv_coord_t size)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, size, size);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    dial_t *d = lv_mem_alloc(sizeof(*d));
    memset(d, 0, sizeof(*d));
    d->max = 100;
    d->accent = M3_PRIMARY;
    d->cookie = M3_PRIMARY_CONTAINER;
    lv_obj_set_user_data(obj, d);

    d->lbl_value = m3_label(obj, "", &font_display, M3_ON_PRIMARY_CONTAINER);
    lv_obj_align(d->lbl_value, LV_ALIGN_CENTER, 0, -6);
    d->lbl_caption = m3_label(obj, "", &font_body, M3_ON_PRIMARY_CONTAINER);
    lv_obj_align(d->lbl_caption, LV_ALIGN_CENTER, 0, 22);

    lv_obj_add_event_cb(obj, draw_cb, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(obj, delete_cb, LV_EVENT_DELETE, NULL);
    return obj;
}

void ui_dial_set_range(lv_obj_t *obj, int32_t min, int32_t max)
{
    dial_t *d = lv_obj_get_user_data(obj);
    if (d->min == min && d->max == max)
        return;
    d->min = min;
    d->max = max;
    lv_obj_invalidate(obj);
}

void ui_dial_set_value(lv_obj_t *obj, int32_t value)
{
    dial_t *d = lv_obj_get_user_data(obj);
    if (d->value == value)
        return;
    d->value = value;
    lv_obj_invalidate(obj);
}

void ui_dial_set_text(lv_obj_t *obj, const char *value_txt, const char *caption)
{
    dial_t *d = lv_obj_get_user_data(obj);
    if (strcmp(lv_label_get_text(d->lbl_value), value_txt) != 0)
        lv_label_set_text(d->lbl_value, value_txt);
    if (caption && strcmp(lv_label_get_text(d->lbl_caption), caption) != 0)
        lv_label_set_text(d->lbl_caption, caption);
}

void ui_dial_set_accent(lv_obj_t *obj, lv_color_t accent, lv_color_t cookie)
{
    dial_t *d = lv_obj_get_user_data(obj);
    if (d->accent.full == accent.full && d->cookie.full == cookie.full)
        return;
    d->accent = accent;
    d->cookie = cookie;
    lv_obj_invalidate(obj);
}
