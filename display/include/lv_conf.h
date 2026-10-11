/*
 * Configuración de LVGL 8.3 para la TFT35 B1. Solo se listan los valores que
 * cambian respecto a los predeterminados (lv_conf_internal.h).
 */
#if 1 /* habilitar contenido */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* Color: RGB565, sin intercambio de bytes (el bus FSMC es de 16 bits) */
#define LV_COLOR_DEPTH          16
#define LV_COLOR_16_SWAP        0

/* Memoria interna de LVGL */
#define LV_MEM_CUSTOM           0
#define LV_MEM_SIZE             (36U * 1024U)

/* Tick a partir de HAL_GetTick() */
#define LV_TICK_CUSTOM          1
#define LV_TICK_CUSTOM_INCLUDE  "stm32f2xx_hal.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (HAL_GetTick())

#define LV_DISP_DEF_REFR_PERIOD 30
#define LV_INDEV_DEF_READ_PERIOD 20
#define LV_DPI_DEF              130

/* Fuentes (14 y 20 px; solo ASCII + "°"; por eso los textos de la interfaz van sin tildes) */
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_20   1
#define LV_FONT_DEFAULT         &lv_font_montserrat_14

/* Widgets que no se usan (ahorran flash) */
#define LV_USE_CALENDAR         0
#define LV_USE_COLORWHEEL       0
#define LV_USE_IMGBTN           0
#define LV_USE_KEYBOARD         0
#define LV_USE_LIST             0
#define LV_USE_MENU             0
#define LV_USE_SPAN             0
#define LV_USE_SPINBOX          0
#define LV_USE_TILEVIEW         0
#define LV_USE_WIN              0
#define LV_USE_ANIMIMG          0
#define LV_USE_TEXTAREA         0
#define LV_USE_ROLLER           0
#define LV_USE_TABLE            0
#define LV_USE_CANVAS           0
#define LV_USE_IMG              1
#define LV_USE_CHART            0
#define LV_USE_LED              0
#define LV_USE_MSGBOX           0
#define LV_USE_SPINNER          0
#define LV_USE_ARC              0
#define LV_USE_CHECKBOX         0
#define LV_USE_DROPDOWN         0
#define LV_USE_SWITCH           0
#define LV_USE_LINE             0
#define LV_USE_GRID             0

#define LV_USE_LOG              0
#define LV_USE_ASSERT_NULL      1
#define LV_USE_ASSERT_MALLOC    1

#define LV_BUILD_EXAMPLES       0

#endif /* LV_CONF_H */
#endif
