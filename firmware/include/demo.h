#ifndef DEMO_H
#define DEMO_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Datos simulados del detector para probar la pantalla mientras no estén los
 * sensores: ciclo normal -> oscilación -> falla -> recuperación (40 s),
 * equivalente a pc/display_sim/sim.py.
 */
void demo_init(void);
void demo_poll(uint32_t now_ms);
void demo_on_frame(uint8_t type, const uint8_t *payload, uint8_t len);

/* Corriente y voltaje reales del INA3221; si valid, reemplazan a los simulados */
void demo_set_power(bool valid, const int16_t ma[3], uint16_t bus_mv);

/* Implementado en main.c */
void link_send(uint8_t type, const void *payload, uint8_t len);

#endif /* DEMO_H */
