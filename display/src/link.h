#ifndef LINK_H
#define LINK_H

#include <stdint.h>
#include <stdbool.h>
#include "display_link.h"

/* Enlace UART con la STM32H745 (USART2, 115200 8N1). */
void link_init(void);

/* Procesa los bytes recibidos; llamar en el lazo principal. */
void link_poll(void);

/* true si llegó un STATUS en los últimos LINK_TIMEOUT_MS */
bool link_alive(void);
#define LINK_TIMEOUT_MS 1500u

/* Último estado recibido */
const dl_status_t *link_status(void);

/* Umbrales y configuración recibidos de la H745 (valid = ya llegaron) */
bool link_thresholds(dl_thresholds_t *out);
bool link_config(dl_config_t *out);

/* Envía una trama cualquiera */
void link_send(uint8_t type, const void *payload, uint8_t len);

void link_send_thresholds(const dl_thresholds_t *t);
void link_send_config(const dl_config_t *c);
void link_send_ack_alarm(void);

/* Contadores para la pantalla de diagnóstico */
uint32_t link_rx_frames(void);
uint32_t link_rx_errors(void);

/* Llamado desde USART2_IRQHandler */
void link_irq(void);

#endif /* LINK_H */
