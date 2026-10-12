/*
 * Envío del firmware de la pantalla desde la STM32H745 por el UART del
 * enlace (protocolo de display_link.h).
 *
 * Al arrancar pregunta a la pantalla qué firmware tiene (FW_INFO). Si el CRC
 * no coincide con la imagen incrustada (o la pantalla no responde a FW_INFO,
 * como la versión 0.1), se la manda con FW_BEGIN / FW_DATA / FW_END.
 *
 * Es código portable: no usa HAL. El firmware de la H745 le da las funciones
 * para enviar bytes y la hora en ms, y le pasa las tramas recibidas.
 */
#ifndef DISPLAY_FWPUSH_H
#define DISPLAY_FWPUSH_H

#include <stdint.h>
#include <stdbool.h>
#include "display_link.h"

typedef enum {
    FWPUSH_IDLE,          /* aún no empieza (espera el arranque de la pantalla) */
    FWPUSH_QUERY,         /* preguntando FW_INFO                                */
    FWPUSH_BEGIN,         /* esperando ACK de FW_BEGIN (borrado)                */
    FWPUSH_DATA,          /* enviando bloques                                   */
    FWPUSH_END,           /* esperando verificación                             */
    FWPUSH_REBOOT,        /* la pantalla se reinicia con el firmware nuevo      */
    FWPUSH_UP_TO_DATE,    /* la pantalla ya tiene la imagen incrustada          */
    FWPUSH_FAILED,        /* error; se reintenta más tarde                      */
} fwpush_state_t;

typedef struct {
    /* Proporcionado por el usuario */
    const uint8_t *image;
    uint32_t       size;              /* múltiplo de 4                     */
    uint32_t       crc32;
    void         (*send)(const uint8_t *data, uint32_t len);

    /* Estado interno */
    fwpush_state_t state;
    uint32_t       t_state, t_sent, start_ms;
    uint32_t       offset;
    uint8_t        retries;
    uint8_t        attempts;
    bool           waiting;
    uint8_t        last_error;        /* DL_FW_* de la pantalla o 0xFF = timeout */
} fwpush_t;

void fwpush_init(fwpush_t *p, uint32_t now_ms);

/* Llamar seguido (cada pocos ms). */
void fwpush_poll(fwpush_t *p, uint32_t now_ms);

/* Pasar cada trama recibida de la pantalla; devuelve true si la consumió. */
bool fwpush_on_frame(fwpush_t *p, uint8_t type, const uint8_t *payload, uint8_t len,
                     uint32_t now_ms);

/* Fuerza una actualización aunque la versión coincida. */
void fwpush_force(fwpush_t *p, uint32_t now_ms);

/* true mientras se está transfiriendo (no mandar otras tramas a la pantalla) */
bool fwpush_busy(const fwpush_t *p);

/* Progreso 0–100 */
uint8_t fwpush_progress(const fwpush_t *p);

uint32_t dl_crc32(uint32_t crc, const uint8_t *data, uint32_t len);

#endif /* DISPLAY_FWPUSH_H */
