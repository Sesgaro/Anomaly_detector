#ifndef FWUPDATE_H
#define FWUPDATE_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Actualización del firmware de la pantalla por el UART del enlace.
 *
 * 1. FW_BEGIN: borra un área de la flash SPI W25Qxx.
 * 2. FW_DATA:  guarda cada bloque en la W25Qxx (en orden).
 * 3. FW_END:   verifica el CRC32 y la tabla de vectores; si todo está bien,
 *              una rutina en RAM copia la imagen a 0x08008000 y reinicia.
 *
 * El bootloader de BTT (0x08000000–0x08007FFF) nunca se toca.
 */

/* Procesa una trama FW_*; responde con FW_ACK por el enlace. */
void fwupdate_handle(uint8_t type, const uint8_t *payload, uint8_t len);

/* Cancela por inactividad y quita los mensajes; llamar en el lazo. */
void fwupdate_poll(void);

/* true mientras hay una actualización en curso */
bool fwupdate_active(void);

#endif /* FWUPDATE_H */
