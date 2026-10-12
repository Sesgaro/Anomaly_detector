/*
 * Tema Material 3 (Expressive), modo oscuro, paleta Monet a partir de un azul.
 */
#ifndef THEME_H
#define THEME_H

#include "lvgl.h"

/* Roles de color (esquema oscuro) */
#define M3_PRIMARY              lv_color_hex(0xA8C8FF)
#define M3_ON_PRIMARY           lv_color_hex(0x0A305F)
#define M3_PRIMARY_CONTAINER    lv_color_hex(0x284777)
#define M3_ON_PRIMARY_CONTAINER lv_color_hex(0xD6E3FF)
#define M3_SECONDARY_CONTAINER  lv_color_hex(0x3E4759)
#define M3_ON_SECONDARY_CONT    lv_color_hex(0xDAE2F9)
#define M3_TERTIARY             lv_color_hex(0xDCBCE0)
#define M3_SURFACE              lv_color_hex(0x111318)
#define M3_SURFACE_CONT_LOW     lv_color_hex(0x191C20)
#define M3_SURFACE_CONT         lv_color_hex(0x1D2024)
#define M3_SURFACE_CONT_HIGH    lv_color_hex(0x282A2F)
#define M3_SURFACE_CONT_HIGHEST lv_color_hex(0x33353A)
#define M3_ON_SURFACE           lv_color_hex(0xE2E2E9)
#define M3_ON_SURFACE_VARIANT   lv_color_hex(0xC4C6D0)
#define M3_OUTLINE              lv_color_hex(0x8E9099)
#define M3_OUTLINE_VARIANT      lv_color_hex(0x44474E)

/* Estados del detector (contenedor / texto sobre el contenedor / acento) */
#define ST_OK_CONT     lv_color_hex(0x284777)
#define ST_OK_ON       lv_color_hex(0xD6E3FF)
#define ST_WARN_CONT   lv_color_hex(0x5B4300)
#define ST_WARN_ON     lv_color_hex(0xFFDF9E)
#define ST_WARN        lv_color_hex(0xF5BF48)
#define ST_ANOM_CONT   lv_color_hex(0x6E3900)
#define ST_ANOM_ON     lv_color_hex(0xFFDCC4)
#define ST_ANOM        lv_color_hex(0xFFB783)
#define ST_FAIL_CONT   lv_color_hex(0x93000A)
#define ST_FAIL_ON     lv_color_hex(0xFFDAD6)
#define ST_FAIL        lv_color_hex(0xFFB4AB)
#define ST_OFF_CONT    lv_color_hex(0x33353A)
#define ST_OFF_ON      lv_color_hex(0xC4C6D0)

LV_FONT_DECLARE(font_body);
LV_FONT_DECLARE(font_title);
LV_FONT_DECLARE(font_display);
LV_FONT_DECLARE(font_icons);

/* Íconos (FontAwesome, mismos códigos que LV_SYMBOL_*) */
#define IC_HOME     "\xEF\x80\x95"
#define IC_EDIT     "\xEF\x8C\x84"
#define IC_SETTINGS "\xEF\x80\x93"
#define IC_BELL     "\xEF\x83\xB3"
#define IC_OK       "\xEF\x80\x8C"
#define IC_CLOSE    "\xEF\x80\x8D"
#define IC_WARNING  "\xEF\x81\xB1"
#define IC_UPLOAD   "\xEF\x82\x93"
#define IC_REFRESH  "\xEF\x80\xA1"
#define IC_GPS      "\xEF\x84\xA4"
#define IC_CHARGE   "\xEF\x83\xA7"
#define IC_AUDIO    "\xEF\x80\x81"
#define IC_WIFI     "\xEF\x87\xAB"
#define IC_MINUS    "\xEF\x81\xA8"
#define IC_PLUS     "\xEF\x81\xA7"
#define IC_EYE      "\xEF\x81\xAE"
#define IC_LINK     "\xEF\x83\x81"

void theme_init(void);

/* Contenedores y botones con la forma de Material 3 */
lv_obj_t *m3_card(lv_obj_t *parent, lv_color_t bg, lv_coord_t radius);
lv_obj_t *m3_button(lv_obj_t *parent, const char *txt, lv_color_t bg, lv_color_t fg,
                    lv_coord_t w, lv_coord_t h);
lv_obj_t *m3_label(lv_obj_t *parent, const char *txt, const lv_font_t *font, lv_color_t c);

/* Grupo segmentado de botones (Material 3 "button group"): n botones de
 * ancho w; el seleccionado toma la forma de píldora. */
lv_obj_t *m3_segmented(lv_obj_t *parent, const char *const *labels, int n,
                       lv_coord_t w, lv_coord_t h, lv_event_cb_t cb);
void m3_segmented_select(lv_obj_t *seg, int idx);
int m3_segmented_selected(lv_obj_t *seg);

#endif /* THEME_H */
