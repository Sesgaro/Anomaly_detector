#ifndef TOUCH_H
#define TOUCH_H

#include <stdint.h>
#include <stdbool.h>

/* Calibración afín: x = a*rx + b*ry + c ; y = d*rx + e*ry + f */
typedef struct {
    float a, b, c;
    float d, e, f;
} touch_cal_t;

void touch_init(void);

/* Lectura cruda del XPT2046 (12 bits). Devuelve false si no hay toque. */
bool touch_read_raw(uint16_t *rx, uint16_t *ry);

/* Lectura en píxeles con la calibración vigente. */
bool touch_read(int16_t *x, int16_t *y);

void touch_set_cal(const touch_cal_t *cal);

/* Calcula la calibración a partir de 3 puntos no colineales. */
bool touch_compute_cal(const int16_t sx[3], const int16_t sy[3],
                       const uint16_t rx[3], const uint16_t ry[3],
                       touch_cal_t *out);

#endif /* TOUCH_H */
