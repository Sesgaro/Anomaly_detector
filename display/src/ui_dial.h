#ifndef UI_DIAL_H
#define UI_DIAL_H

#include "lvgl.h"

/*
 * Dial estilo Material 3 Expressive: forma de "galleta" ondulada al centro,
 * marcas, arco de progreso ondulado y un punto que señala el valor.
 * El barrido es de 270° (de abajo a la izquierda a abajo a la derecha).
 */
lv_obj_t *ui_dial_create(lv_obj_t *parent, lv_coord_t size);

void ui_dial_set_range(lv_obj_t *dial, int32_t min, int32_t max);
void ui_dial_set_value(lv_obj_t *dial, int32_t value);

/* Texto grande (font_display) y leyenda bajo el número */
void ui_dial_set_text(lv_obj_t *dial, const char *value_txt, const char *caption);

/* Color del arco y del punto (por defecto, M3_PRIMARY) */
void ui_dial_set_accent(lv_obj_t *dial, lv_color_t accent, lv_color_t cookie);

#endif /* UI_DIAL_H */
