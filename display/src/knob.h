#ifndef KNOB_H
#define KNOB_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Rueda (encoder) de la TFT35 B1 y sus dos LEDs RGB (D1 y D2).
 * Pines según el firmware oficial de BTT para la variante B1:
 *   ENCA PA8, ENCB PC9, botón PC8 (activos en bajo, con pull-up),
 *   ENC_EN PC6 (salida hacia la placa madre, se deja en bajo),
 *   LEDs: 2 WS2812 en cadena en PC7.
 */

typedef enum {
    KNOB_EV_NONE = 0,
    KNOB_EV_CLICK,        /* pulsación corta (al soltar)          */
    KNOB_EV_LONG,         /* mantener presionado >= KNOB_LONG_MS   */
} knob_event_t;

#define KNOB_LONG_MS 700u

void knob_init(void);

/* Pasos acumulados desde la última llamada (positivo = horario). */
int16_t knob_take_steps(void);

/* Evento del botón (se consume al leerlo). Llamar en el lazo principal. */
knob_event_t knob_poll_button(void);

/* Color de los dos LEDs en 0xRRGGBB (ya escalado por el brillo). */
void knob_set_leds(uint32_t rgb_d1, uint32_t rgb_d2);

/* Llamado desde SysTick (1 ms): decodifica la cuadratura. */
void knob_tick_1ms(void);

#endif /* KNOB_H */
