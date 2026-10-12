#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>
#include <stdbool.h>
#include "touch.h"

/* Ajustes locales de la pantalla, guardados en el último sector de la W25Qxx.
 * Los umbrales NO se guardan aquí: viven en la STM32H745. */
typedef struct {
    uint8_t     brightness;   /* 10–100 % */
    uint8_t     cal_valid;
    touch_cal_t cal;
    uint8_t     led_on;       /* LEDs de la rueda encendidos (v2) */
} settings_t;

/* Devuelve false si no había ajustes válidos (se cargan los de fábrica). */
bool settings_load(void);
bool settings_save(void);
settings_t *settings(void);

#endif /* SETTINGS_H */
