#ifndef CALIB_H
#define CALIB_H

#include <stdbool.h>

/* Calibración del táctil con 3 puntos. Mientras está activa, la entrada
 * táctil de LVGL queda deshabilitada. */
void calib_start(void);
void calib_poll(void);
bool calib_active(void);

#endif /* CALIB_H */
