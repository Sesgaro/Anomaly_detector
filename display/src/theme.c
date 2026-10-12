#include "theme.h"

void theme_init(void)
{
    lv_disp_t *d = lv_disp_get_default();
    lv_theme_t *th = lv_theme_default_init(d, M3_PRIMARY, M3_SECONDARY_CONTAINER,
                                           true, &font_body);
    lv_disp_set_theme(d, th);

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, M3_SURFACE, 0);
    lv_obj_set_style_text_color(scr, M3_ON_SURFACE, 0);
    lv_obj_set_style_text_font(scr, &font_body, 0);
}

lv_obj_t *m3_card(lv_obj_t *parent, lv_color_t bg, lv_coord_t radius)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_set_style_bg_color(c, bg, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c, radius, 0);
    lv_obj_set_style_pad_all(c, 12, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
}

lv_obj_t *m3_label(lv_obj_t *parent, const char *txt, const lv_font_t *font, lv_color_t c)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, c, 0);
    return l;
}

lv_obj_t *m3_button(lv_obj_t *parent, const char *txt, lv_color_t bg, lv_color_t fg,
                    lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *b = lv_btn_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, bg, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    /* Al presionar: la forma se "aplana" (Expressive) y se aclara */
    lv_obj_set_style_radius(b, 12, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(b, lv_color_lighten(bg, LV_OPA_20), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(b, LV_OPA_40, LV_STATE_DISABLED);

    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, fg, 0);
    lv_obj_set_style_text_font(l, &font_body, 0);
    lv_obj_center(l);
    return b;
}

/* --------------------------------------------------------- grupo segmentado */

static void seg_restyle(lv_obj_t *seg)
{
    int sel = (int)(intptr_t)lv_obj_get_user_data(seg);
    uint32_t n = lv_obj_get_child_cnt(seg);

    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *b = lv_obj_get_child(seg, (int32_t)i);
        bool on = (int)i == sel;
        bool first = i == 0, last = i == n - 1;
        lv_obj_set_style_bg_color(b, on ? M3_PRIMARY : M3_SECONDARY_CONTAINER, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(b, 0),
                                    on ? M3_ON_PRIMARY : M3_ON_SECONDARY_CONT, 0);
        /* Extremos redondeados y el seleccionado en píldora completa */
        lv_obj_set_style_radius(b, (on || first || last) ? LV_RADIUS_CIRCLE : 8, 0);
    }
}

static void seg_btn_cb(lv_event_t *e)
{
    lv_obj_t *b = lv_event_get_target(e);
    lv_obj_t *seg = lv_obj_get_parent(b);
    m3_segmented_select(seg, (int)lv_obj_get_index(b));
    lv_event_send(seg, LV_EVENT_VALUE_CHANGED, NULL);
}

lv_obj_t *m3_segmented(lv_obj_t *parent, const char *const *labels, int n,
                       lv_coord_t w, lv_coord_t h, lv_event_cb_t cb)
{
    lv_obj_t *seg = lv_obj_create(parent);
    lv_obj_remove_style_all(seg);
    lv_obj_set_size(seg, LV_SIZE_CONTENT, h);
    lv_obj_set_flex_flow(seg, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(seg, 3, 0);
    lv_obj_clear_flag(seg, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < n; i++) {
        lv_obj_t *b = m3_button(seg, labels[i], M3_SECONDARY_CONTAINER,
                                M3_ON_SECONDARY_CONT, w, h);
        lv_obj_add_event_cb(b, seg_btn_cb, LV_EVENT_CLICKED, NULL);
    }
    lv_obj_set_user_data(seg, (void *)(intptr_t)-1);
    seg_restyle(seg);
    if (cb)
        lv_obj_add_event_cb(seg, cb, LV_EVENT_VALUE_CHANGED, NULL);
    return seg;
}

void m3_segmented_select(lv_obj_t *seg, int idx)
{
    lv_obj_set_user_data(seg, (void *)(intptr_t)idx);
    seg_restyle(seg);
}

int m3_segmented_selected(lv_obj_t *seg)
{
    return (int)(intptr_t)lv_obj_get_user_data(seg);
}
