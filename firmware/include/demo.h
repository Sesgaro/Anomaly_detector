#ifndef DEMO_H
#define DEMO_H

#include <stdint.h>

/*
 * Datos simulados del detector para probar la pantalla mientras no estén los
 * sensores: ciclo normal -> oscilación -> falla -> recuperación (40 s),
 * equivalente a pc/display_sim/sim.py.
 */
void demo_init(void);
void demo_poll(uint32_t now_ms);
void demo_on_frame(uint8_t type, const uint8_t *payload, uint8_t len);

/* Implementado en main.c */
void link_send(uint8_t type, const void *payload, uint8_t len);

#endif /* DEMO_H */
