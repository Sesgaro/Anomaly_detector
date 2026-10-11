#ifndef UI_H
#define UI_H

#include <stdint.h>

/* Crea la interfaz. lcd_id se muestra en la pestaña de ajustes. */
void ui_init(uint16_t lcd_id);

/* Refresca la interfaz con los datos del enlace (llamar en el lazo). */
void ui_update(void);

#endif /* UI_H */
