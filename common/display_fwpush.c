#include "display_fwpush.h"
#include <string.h>

#define BOOT_WAIT_MS     1500u   /* tiempo para que la pantalla arranque        */
#define INFO_TIMEOUT_MS   400u
#define INFO_TRIES          4u
#define BEGIN_TIMEOUT_MS 20000u  /* incluye el borrado de la flash SPI           */
#define DATA_TIMEOUT_MS   300u
#define DATA_RETRIES        8u
#define END_TIMEOUT_MS   20000u
#define REBOOT_WAIT_MS   3000u
#define RETRY_LATER_MS  10000u
#define MAX_ATTEMPTS        3u

uint32_t dl_crc32(uint32_t crc, const uint8_t *p, uint32_t n)
{
    crc = ~crc;
    while (n--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

static void send_frame(fwpush_t *p, uint8_t type, const void *payload, uint8_t len, uint32_t now)
{
    uint8_t buf[DL_MAX_PAYLOAD + 5];
    size_t n = dl_encode(type, payload, len, buf);
    p->send(buf, (uint32_t)n);
    p->t_sent = now;
    p->waiting = true;
}

static void set_state(fwpush_t *p, fwpush_state_t s, uint32_t now)
{
    p->state = s;
    p->t_state = now;
    p->retries = 0;
    p->waiting = false;
}

static void send_data(fwpush_t *p, uint32_t now)
{
    uint8_t pl[4 + DL_FW_CHUNK];
    uint32_t n = p->size - p->offset;
    if (n > DL_FW_CHUNK)
        n = DL_FW_CHUNK;
    memcpy(pl, &p->offset, 4);
    memcpy(pl + 4, p->image + p->offset, n);
    send_frame(p, DL_MSG_FW_DATA, pl, (uint8_t)(4u + n), now);
}

static void start_push(fwpush_t *p, uint32_t now)
{
    dl_fw_begin_t b = { .size = p->size, .crc32 = p->crc32 };
    p->offset = 0;
    p->attempts++;
    p->start_ms = now;
    set_state(p, FWPUSH_BEGIN, now);
    send_frame(p, DL_MSG_FW_BEGIN, &b, sizeof(b), now);
}

static void fail(fwpush_t *p, uint8_t err, uint32_t now)
{
    p->last_error = err;
    set_state(p, FWPUSH_FAILED, now);
}

void fwpush_init(fwpush_t *p, uint32_t now)
{
    p->offset = 0;
    p->attempts = 0;
    p->last_error = 0;
    set_state(p, FWPUSH_IDLE, now);
}

void fwpush_force(fwpush_t *p, uint32_t now)
{
    p->attempts = 0;
    start_push(p, now);
}

bool fwpush_busy(const fwpush_t *p)
{
    return p->state == FWPUSH_BEGIN || p->state == FWPUSH_DATA ||
           p->state == FWPUSH_END || p->state == FWPUSH_REBOOT;
}

uint8_t fwpush_progress(const fwpush_t *p)
{
    return p->size ? (uint8_t)((uint64_t)p->offset * 100u / p->size) : 0;
}

void fwpush_poll(fwpush_t *p, uint32_t now)
{
    uint32_t in_state = now - p->t_state;
    uint32_t since_tx = now - p->t_sent;

    switch (p->state) {
    case FWPUSH_IDLE:
        if (in_state >= BOOT_WAIT_MS) {
            set_state(p, FWPUSH_QUERY, now);
            send_frame(p, DL_MSG_FW_INFO_REQ, NULL, 0, now);
        }
        break;

    case FWPUSH_QUERY:
        if (p->waiting && since_tx >= INFO_TIMEOUT_MS) {
            if (++p->retries < INFO_TRIES) {
                send_frame(p, DL_MSG_FW_INFO_REQ, NULL, 0, now);
            } else {
                /* Sin respuesta: puede ser la versión 0.1 (sin FW_INFO) o que
                 * la pantalla no esté conectada. Se intenta actualizar; si
                 * nadie contesta a FW_BEGIN, queda en FAILED y se reintenta. */
                start_push(p, now);
            }
        }
        break;

    case FWPUSH_BEGIN:
        if (since_tx >= BEGIN_TIMEOUT_MS)
            fail(p, 0xFF, now);
        break;

    case FWPUSH_DATA:
        if (since_tx >= DATA_TIMEOUT_MS) {
            if (++p->retries > DATA_RETRIES)
                fail(p, 0xFF, now);
            else
                send_data(p, now);
        }
        break;

    case FWPUSH_END:
        if (since_tx >= END_TIMEOUT_MS)
            fail(p, 0xFF, now);
        break;

    case FWPUSH_REBOOT:
        /* Confirmar que arrancó con la imagen nueva */
        if (in_state >= REBOOT_WAIT_MS)
            set_state(p, FWPUSH_IDLE, now - BOOT_WAIT_MS);
        break;

    case FWPUSH_FAILED:
        if (p->attempts < MAX_ATTEMPTS && in_state >= RETRY_LATER_MS)
            set_state(p, FWPUSH_IDLE, now - BOOT_WAIT_MS);
        break;

    case FWPUSH_UP_TO_DATE:
    default:
        break;
    }
}

bool fwpush_on_frame(fwpush_t *p, uint8_t type, const uint8_t *pl, uint8_t len, uint32_t now)
{
    if (type == DL_MSG_FW_INFO && len == sizeof(dl_fw_info_t)) {
        dl_fw_info_t info;
        memcpy(&info, pl, sizeof(info));
        if (p->state == FWPUSH_QUERY) {
            if (info.crc32 == p->crc32 && info.size == p->size) {
                set_state(p, FWPUSH_UP_TO_DATE, now);
            } else if (p->attempts < MAX_ATTEMPTS) {
                start_push(p, now);
            } else {
                fail(p, DL_FW_ERR_CRC, now);
            }
        }
        return true;
    }

    if (type != DL_MSG_FW_ACK || len != sizeof(dl_fw_ack_t))
        return false;

    dl_fw_ack_t a;
    memcpy(&a, pl, sizeof(a));

    switch (p->state) {
    case FWPUSH_BEGIN:
        if (a.status != DL_FW_OK) {
            fail(p, a.status, now);
            break;
        }
        set_state(p, FWPUSH_DATA, now);
        send_data(p, now);
        break;

    case FWPUSH_DATA:
        if (a.status == DL_FW_OK || a.status == DL_FW_ERR_SEQ) {
            p->offset = a.next_offset;     /* la pantalla dice dónde seguir */
            p->retries = 0;
            if (p->offset >= p->size) {
                set_state(p, FWPUSH_END, now);
                send_frame(p, DL_MSG_FW_END, NULL, 0, now);
            } else {
                send_data(p, now);
            }
        } else {
            fail(p, a.status, now);
        }
        break;

    case FWPUSH_END:
        if (a.status == DL_FW_OK)
            set_state(p, FWPUSH_REBOOT, now);
        else
            fail(p, a.status, now);
        break;

    default:
        break;
    }
    return true;
}
