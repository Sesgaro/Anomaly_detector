/*
 * Protocolo UART entre la STM32H745 (M4) y la pantalla BTT TFT35 B1.
 *
 * Trama:  SOF | tipo | len | payload[len] | crc16 (LSB, MSB)
 *   - SOF   = 0xA5
 *   - crc16 = CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) sobre tipo, len
 *             y payload.
 *   - Todos los campos multibyte van en little-endian.
 *
 * Este archivo lo comparten el firmware de la pantalla y el de la STM32H745;
 * pc/display_sim/sim.py implementa lo mismo en Python.
 */
#ifndef DISPLAY_LINK_H
#define DISPLAY_LINK_H

#include <stdint.h>
#include <stddef.h>

#define DL_SOF          0xA5u
#define DL_MAX_PAYLOAD  64u
#define DL_BAUDRATE     115200u

/* Tipos de trama */
enum {
    DL_MSG_STATUS      = 0x01, /* H745 -> pantalla: estado periódico (~10 Hz)   */
    DL_MSG_THRESHOLDS  = 0x02, /* ambos sentidos: umbrales vigentes / nuevos    */
    DL_MSG_CONFIG      = 0x03, /* ambos sentidos: configuración de DOA          */
    DL_MSG_ACK_ALARM   = 0x04, /* pantalla -> H745: alarma reconocida           */
    DL_MSG_REQ_PARAMS  = 0x05, /* pantalla -> H745: pide umbrales y config      */

    /* Actualización del firmware de la pantalla por el mismo UART */
    DL_MSG_FW_BEGIN    = 0x10, /* PC/H745 -> pantalla: dl_fw_begin_t           */
    DL_MSG_FW_DATA     = 0x11, /* PC/H745 -> pantalla: offset u32 + datos      */
    DL_MSG_FW_END      = 0x12, /* PC/H745 -> pantalla: verificar e instalar    */
    DL_MSG_FW_ACK      = 0x13, /* pantalla -> PC/H745: dl_fw_ack_t             */
    DL_MSG_FW_INFO_REQ = 0x14, /* PC/H745 -> pantalla: ¿qué firmware tienes?   */
    DL_MSG_FW_INFO     = 0x15, /* pantalla -> PC/H745: dl_fw_info_t            */
};

/* ------------------------------------------------ actualización de firmware */

#define DL_FW_CHUNK 56u        /* bytes de datos por trama FW_DATA            */

enum {
    DL_FW_OK         = 0,
    DL_FW_ERR_SIZE   = 1,      /* imagen demasiado grande o vacía             */
    DL_FW_ERR_SEQ    = 2,      /* offset inesperado (next_offset dice cuál)   */
    DL_FW_ERR_CRC    = 3,      /* CRC32 de la imagen no coincide              */
    DL_FW_ERR_FLASH  = 4,      /* falla de la flash SPI                       */
    DL_FW_ERR_IMAGE  = 5,      /* tabla de vectores inválida                  */
    DL_FW_ERR_STATE  = 6,      /* FW_DATA/FW_END sin FW_BEGIN                 */
};

typedef struct __attribute__((packed)) {
    uint32_t size;             /* bytes del .bin                              */
    uint32_t crc32;            /* CRC-32 (zlib) del .bin completo             */
} dl_fw_begin_t;

/* Firmware en ejecución: la H745 lo compara con el que lleva incrustado */
typedef struct __attribute__((packed)) {
    uint32_t crc32;            /* CRC-32 (zlib) de la imagen en flash         */
    uint32_t size;             /* bytes (múltiplo de 4)                       */
    char     version[8];       /* p. ej. "0.2"                                */
    uint32_t flash_jedec;      /* ID de la flash SPI (0 = no responde)        */
    uint32_t flash_size;       /* bytes de la flash SPI (0 = no detectada)    */
} dl_fw_info_t;

typedef struct __attribute__((packed)) {
    uint8_t  status;           /* DL_FW_*                                     */
    uint32_t next_offset;      /* siguiente offset esperado                   */
} dl_fw_ack_t;

/* Niveles de alerta */
enum {
    DL_LEVEL_NORMAL      = 0,
    DL_LEVEL_ADVERTENCIA = 1,
    DL_LEVEL_ANOMALIA    = 2,
    DL_LEVEL_FALLA       = 3,
};

/* Bits de dl_status_t.flags */
#define DL_FLAG_DETECTOR_OK  (1u << 0) /* M7 vivo (heartbeat)          */
#define DL_FLAG_ETH_LINK     (1u << 1) /* enlace Ethernet con la PC    */
#define DL_FLAG_FOUR_MICS    (1u << 2) /* arreglo de 4 micrófonos      */
#define DL_FLAG_DOA_VALID    (1u << 3) /* hay una fuente detectada     */
#define DL_FLAG_INA_CRITICAL (1u << 4) /* pin CRITICAL del INA3221     */

typedef struct __attribute__((packed)) {
    uint8_t  level;            /* DL_LEVEL_*                                   */
    uint8_t  flags;            /* DL_FLAG_*                                    */
    int16_t  doa_ddeg;         /* ángulo DOA en décimas de grado               */
    uint16_t audio_score_m;    /* error de reconstrucción ×1000                */
    uint16_t vib_rms_mg;       /* vibración RMS en mg                          */
    int16_t  current_ma[3];    /* corriente de los 3 canales del INA3221 (mA)  */
    uint16_t motor_bus_mv;     /* voltaje de la línea de 12 V del motor (mV)   */
    uint32_t uptime_ms;
} dl_status_t;

typedef struct __attribute__((packed)) {
    uint16_t audio_warn_m;     /* error de reconstrucción ×1000 */
    uint16_t audio_anom_m;
    uint16_t vib_warn_mg;
    uint16_t vib_anom_mg;
    uint16_t current_warn_ma;  /* límite WARNING del INA3221 (promedio)      */
    uint16_t current_crit_ma;  /* límite CRITICAL del INA3221 (por muestra)  */
} dl_thresholds_t;

enum {
    DL_DOA_SCAN  = 0,          /* barrido: la fuente más fuerte              */
    DL_DOA_FIXED = 1,          /* enfoque fijo en el ángulo elegido          */
};

typedef struct __attribute__((packed)) {
    uint8_t  n_mics;           /* 2 o 4                                      */
    uint8_t  doa_mode;         /* DL_DOA_*                                   */
    int16_t  focus_ddeg;       /* ángulo de enfoque en décimas de grado      */
} dl_config_t;

_Static_assert(sizeof(dl_status_t) <= DL_MAX_PAYLOAD, "status demasiado grande");

static inline uint16_t dl_crc16(uint16_t crc, const uint8_t *data, size_t len)
{
    while (len--) {
        crc ^= (uint16_t)(*data++) << 8;
        for (int i = 0; i < 8; i++)
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}

/* Arma una trama en out (tamaño >= len + 5). Devuelve el número de bytes. */
static inline size_t dl_encode(uint8_t type, const void *payload, uint8_t len, uint8_t *out)
{
    const uint8_t *p = (const uint8_t *)payload;
    out[0] = DL_SOF;
    out[1] = type;
    out[2] = len;
    for (uint8_t i = 0; i < len; i++)
        out[3 + i] = p[i];
    uint16_t crc = dl_crc16(0xFFFFu, &out[1], (size_t)len + 2u);
    out[3 + len] = (uint8_t)(crc & 0xFFu);
    out[4 + len] = (uint8_t)(crc >> 8);
    return (size_t)len + 5u;
}

/* Decodificador byte a byte */
typedef struct {
    uint8_t state;
    uint8_t type;
    uint8_t len;
    uint8_t idx;
    uint8_t crc_lo;
    uint8_t payload[DL_MAX_PAYLOAD];
} dl_parser_t;

/* Devuelve 1 cuando hay una trama válida en p->type / p->payload / p->len. */
static inline int dl_parse_byte(dl_parser_t *p, uint8_t b)
{
    switch (p->state) {
    case 0:
        if (b == DL_SOF) p->state = 1;
        break;
    case 1:
        p->type = b;
        p->state = 2;
        break;
    case 2:
        if (b > DL_MAX_PAYLOAD) { p->state = 0; break; }
        p->len = b;
        p->idx = 0;
        p->state = b ? 3 : 4;
        break;
    case 3:
        p->payload[p->idx++] = b;
        if (p->idx >= p->len) p->state = 4;
        break;
    case 4:
        p->crc_lo = b;
        p->state = 5;
        break;
    case 5: {
        p->state = 0;
        uint8_t hdr[2] = { p->type, p->len };
        uint16_t crc = dl_crc16(0xFFFFu, hdr, 2);
        crc = dl_crc16(crc, p->payload, p->len);
        if (crc == (uint16_t)(p->crc_lo | ((uint16_t)b << 8)))
            return 1;
        break;
    }
    default:
        p->state = 0;
        break;
    }
    return 0;
}

#endif /* DISPLAY_LINK_H */
