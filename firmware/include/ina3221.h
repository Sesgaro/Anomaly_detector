#ifndef INA3221_H
#define INA3221_H

#include "stm32h7xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

/* Shunt de los módulos INA3221 comunes (R100). Cambiar si el módulo es otro. */
#define INA3221_SHUNT_MOHM 100

/* I2C1 en PB8/PB9 y alertas en PE11/PE14 (docs/conexiones.md).
 * Devuelve la dirección de 7 bits encontrada (0x40–0x43) o 0 si no responde. */
uint16_t ina3221_init(uint16_t *manuf_id, uint16_t *die_id);

/* ch = 0..2 */
bool ina3221_read(int ch, int32_t *bus_mv, int32_t *shunt_uv);
bool ina3221_critical(void);
bool ina3221_warning(void);

#endif /* INA3221_H */
