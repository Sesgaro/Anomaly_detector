/*
 * Prueba en la PC de common/display_fwpush.c contra una pantalla simulada que
 * imita a display/src/fwupdate.c: con y sin FW_INFO (versión 0.1), tramas
 * perdidas, verificación de CRC y reinicio con la imagen nueva.
 *   cc -I../../common test_fwpush.c ../../common/display_fwpush.c -o test_fwpush && ./test_fwpush
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "display_fwpush.h"

static uint32_t now;
static fwpush_t push;
static dl_parser_t disp_parser, host_parser;

/* --- pantalla simulada --- */
static uint8_t flash_img[256 * 1024], staging[256 * 1024];
static uint32_t flash_size, img_size, img_crc, next_off;
static int has_info, active, drop_every, rx_count, reboots;
static uint8_t to_host[4096];
static int to_host_len;

static void disp_send(uint8_t type, const void *p, uint8_t len)
{
    to_host_len += (int)dl_encode(type, p, len, to_host + to_host_len);
}

static void disp_ack(uint8_t st)
{
    dl_fw_ack_t a = { st, next_off };
    disp_send(DL_MSG_FW_ACK, &a, sizeof(a));
}

static void disp_frame(void)
{
    uint8_t t = disp_parser.type, *p = disp_parser.payload, len = disp_parser.len;
    if (drop_every && (++rx_count % drop_every) == 0)
        return;                                    /* trama perdida */
    if (t == DL_MSG_FW_INFO_REQ && has_info) {
        dl_fw_info_t i = { dl_crc32(0, flash_img, flash_size), flash_size, "sim" };
        disp_send(DL_MSG_FW_INFO, &i, sizeof(i));
    } else if (t == DL_MSG_FW_BEGIN) {
        dl_fw_begin_t b; memcpy(&b, p, sizeof b);
        img_size = b.size; img_crc = b.crc32; next_off = 0; active = 1;
        disp_ack(DL_FW_OK);
    } else if (t == DL_MSG_FW_DATA && active) {
        uint32_t off; memcpy(&off, p, 4); len -= 4;
        if (off + len <= next_off) { disp_ack(DL_FW_OK); return; }
        if (off != next_off) { disp_ack(DL_FW_ERR_SEQ); return; }
        memcpy(staging + off, p + 4, len); next_off += len; disp_ack(DL_FW_OK);
    } else if (t == DL_MSG_FW_END && active) {
        if (dl_crc32(0, staging, img_size) != img_crc) { disp_ack(DL_FW_ERR_CRC); return; }
        disp_ack(DL_FW_OK);
        memcpy(flash_img, staging, img_size); flash_size = img_size;
        active = 0; has_info = 1; reboots++;        /* la versión nueva sí tiene FW_INFO */
    }
}

static void host_send(const uint8_t *d, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        if (dl_parse_byte(&disp_parser, d[i]))
            disp_frame();
}

static int run(const char *name, int info, int drop, int expect_reboots)
{
    static uint8_t image[158708];
    for (size_t i = 0; i < sizeof image; i++) image[i] = (uint8_t)(i * 7 + 3);
    memset(flash_img, 0xAA, sizeof flash_img); flash_size = 1000;
    has_info = info; drop_every = drop; rx_count = 0; reboots = 0; active = 0;
    if (info == 2) { memcpy(flash_img, image, sizeof image); flash_size = sizeof image; }

    push.image = image; push.size = sizeof image; push.crc32 = dl_crc32(0, image, sizeof image);
    push.send = host_send;
    now = 0;
    fwpush_init(&push, now);
    for (int step = 0; step < 2000000 && push.state != FWPUSH_UP_TO_DATE; step++) {
        now += 1;
        fwpush_poll(&push, now);
        /* Las respuestas de la pantalla llegan en el siguiente ms (como por UART) */
        static uint8_t rx[4096];
        int n = to_host_len;
        memcpy(rx, to_host, (size_t)n);
        to_host_len = 0;
        for (int i = 0; i < n; i++)
            if (dl_parse_byte(&host_parser, rx[i]))
                fwpush_on_frame(&push, host_parser.type, host_parser.payload, host_parser.len, now);
    }
    int ok = push.state == FWPUSH_UP_TO_DATE && reboots == expect_reboots &&
             flash_size == sizeof image && memcmp(flash_img, image, sizeof image) == 0;
    printf("%-42s %s (estado %d, reinicios %d, %u ms simulados)\n", name, ok ? "OK" : "FALLA",
           push.state, reboots, now);
    return ok;
}

int main(void)
{
    int ok = 1;
    ok &= run("Pantalla v0.1 (sin FW_INFO) -> actualiza", 0, 0, 1);
    ok &= run("Pantalla con FW_INFO distinto -> actualiza", 1, 0, 1);
    ok &= run("Pantalla ya actualizada -> no hace nada", 2, 0, 0);
    ok &= run("Con 1 de cada 37 tramas perdidas", 0, 37, 1);
    return ok ? 0 : 1;
}
