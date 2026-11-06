#ifndef UI_DIAL_H
#define UI_DIAL_H

#include "lvgl.h"

/*
 * Dial estilo Material 3 Expressive: forma de "galleta" ondulada al centro,
 * marcas, arco de progreso ondulado y un punto que señala el valor.
 * El recorrido se centra arriba: 270° por defecto, 180° (semicírculo) o 360°
 * (vuelta completa).
 */
lv_obj_t *ui_dial_create(lv_obj_t *parent, lv_coord_t size);

void ui_dial_set_range(lv_obj_t *dial, int32_t min, int32_t max);
void ui_dial_set_value(lv_obj_t *dial, int32_t value);

/* Texto grande (font_display) y leyenda bajo el número */
void ui_dial_set_text(lv_obj_t *dial, const char *value_txt, const char *caption);

/* Recorrido visual en grados: 180, 270 o 360 */
void ui_dial_set_sweep(lv_obj_t *dial, int sweep_deg);

/* Arco fino por fuera de la pista entre lo y hi (p. ej. el arco de
 * apertura alrededor del punto de enfoque). */
void ui_dial_set_band(lv_obj_t *dial, bool on, int32_t lo, int32_t hi);

/* Color del arco y del punto (por defecto, M3_PRIMARY) */
void ui_dial_set_accent(lv_obj_t *dial, lv_color_t accent, lv_color_t cookie);

#endif /* UI_DIAL_H */
