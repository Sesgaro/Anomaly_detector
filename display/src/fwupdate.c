#include "fwupdate.h"
#include "link.h"
#include "w25q.h"
#include "board.h"
#include "lvgl.h"
#include <string.h>

#define APP_ADDR        0x08008000u
#define APP_MAX_SIZE    (224u * 1024u)
#define RAM_START       0x20000000u
#define RAM_END         0x2001C000u
#define STAGING_SIZE    (256u * 1024u)    /* área en la W25Qxx para la imagen */

#define TIMEOUT_MS      10000u   /* sin tramas FW_* -> se cancela          */
#define MSG_MS          5000u    /* tiempo que queda visible un error      */

static bool     active;
static uint32_t last_rx_ms;
static uint32_t img_size, img_crc, next_off, staging;
static lv_obj_t *box, *lbl, *bar;

/* ------------------------------------------------------------------ CRC32 */

static uint32_t crc32_update(uint32_t crc, const uint8_t *p, uint32_t n)
{
    crc = ~crc;
    while (n--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

/* --------------------------------------------------------------- interfaz */

static void ui_show(const char *txt)
{
    if (!box) {
        box = lv_obj_create(lv_layer_top());
        lv_obj_set_size(box, 360, 120);
        lv_obj_center(box);
        lbl = lv_label_create(box);
        lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 4);
        bar = lv_bar_create(box);
        lv_obj_set_size(bar, 300, 18);
        lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -8);
        lv_bar_set_range(bar, 0, 100);
    }
    lv_label_set_text(lbl, txt);
    lv_refr_now(NULL);
}

static void ui_progress(void)
{
    if (bar && img_size)
        lv_bar_set_value(bar, (int32_t)((uint64_t)next_off * 100u / img_size), LV_ANIM_OFF);
}

static void ui_hide(void)
{
    if (box) {
        lv_obj_del(box);
        box = NULL;
    }
}

/* ------------------------------------------------------------- respuestas */

static void ack(uint8_t status)
{
    dl_fw_ack_t a = { .status = status, .next_offset = next_off };
    link_send(DL_MSG_FW_ACK, &a, sizeof(a));
}

static void fail(uint8_t status, const char *txt)
{
    ack(status);
    active = false;
    ui_show(txt);
}

/* ------------------------------------------------- instalación (desde RAM) */

#define RAMFUNC __attribute__((section(".RamFunc"), noinline, used))
#define RAMINL  __attribute__((section(".RamFunc"), always_inline)) static inline

RAMINL uint8_t spi_xfer(uint8_t b)
{
    while (!(SPI3->SR & SPI_SR_TXE)) {}
    *(volatile uint8_t *)&SPI3->DR = b;
    while (!(SPI3->SR & SPI_SR_RXNE)) {}
    return *(volatile uint8_t *)&SPI3->DR;
}

RAMINL void flash_wait(void)
{
    while (FLASH->SR & FLASH_SR_BSY) {}
}

/*
 * Copia size bytes desde la W25Qxx (dirección src) a la flash interna en
 * APP_ADDR y reinicia. Corre desde RAM, con interrupciones deshabilitadas y
 * sin llamar a nada que esté en la flash interna (que se está borrando).
 */
RAMFUNC static void install_from_ram(uint32_t src, uint32_t size)
{
    uint32_t end = APP_ADDR + size;

    /* Desbloquear la flash y limpiar errores */
    if (FLASH->CR & FLASH_CR_LOCK) {
        FLASH->KEYR = 0x45670123u;
        FLASH->KEYR = 0xCDEF89ABu;
    }
    flash_wait();
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_SOP | FLASH_SR_WRPERR |
                FLASH_SR_PGAERR | FLASH_SR_PGPERR | FLASH_SR_PGSERR;

    /* Sectores del STM32F207VC: 2 = 0x08008000 (16 KB), 3 = 0x0800C000 (16 KB),
     * 4 = 0x08010000 (64 KB), 5 = 0x08020000 (128 KB). Los sectores 0 y 1
     * (bootloader de BTT) no se borran. */
    for (uint32_t sec = 2u; sec <= 5u; sec++) {
        uint32_t start = (sec == 2u) ? 0x08008000u :
                         (sec == 3u) ? 0x0800C000u :
                         (sec == 4u) ? 0x08010000u : 0x08020000u;
        if (start >= end)
            break;
        FLASH->CR = FLASH_CR_PSIZE_1 | FLASH_CR_SER | (sec << FLASH_CR_SNB_Pos);
        FLASH->CR |= FLASH_CR_STRT;
        flash_wait();
    }

    /* Leer la W25Qxx (comando 0x03) y programar en palabras de 32 bits */
    GPIOB->BSRR = (uint32_t)FLASH_CS_PIN << 16;          /* CS bajo */
    spi_xfer(0x03);
    spi_xfer((uint8_t)(src >> 16));
    spi_xfer((uint8_t)(src >> 8));
    spi_xfer((uint8_t)src);

    FLASH->CR = FLASH_CR_PSIZE_1 | FLASH_CR_PG;
    for (uint32_t addr = APP_ADDR; addr < end; addr += 4u) {
        uint32_t w = spi_xfer(0xFF);
        w |= (uint32_t)spi_xfer(0xFF) << 8;
        w |= (uint32_t)spi_xfer(0xFF) << 16;
        w |= (uint32_t)spi_xfer(0xFF) << 24;
        *(volatile uint32_t *)addr = w;
        flash_wait();
    }
    GPIOB->BSRR = FLASH_CS_PIN;                           /* CS alto */

    FLASH->CR = 0;
    FLASH->CR |= FLASH_CR_LOCK;

    /* Reinicio por software */
    __DSB();
    SCB->AIRCR = (0x5FAu << SCB_AIRCR_VECTKEY_Pos) | SCB_AIRCR_SYSRESETREQ_Msk;
    __DSB();
    for (;;) {}
}

/* ------------------------------------------------------------- comandos */

static void on_begin(const uint8_t *p, uint8_t len)
{
    dl_fw_begin_t b;

    if (len != sizeof(b))
        return;
    memcpy(&b, p, sizeof(b));

    next_off = 0;
    if (b.size == 0 || b.size > APP_MAX_SIZE || (b.size & 3u)) {
        fail(DL_FW_ERR_SIZE, "Actualizacion: tamano invalido");
        return;
    }
    if (w25q_size() < 2u * STAGING_SIZE) {
        fail(DL_FW_ERR_FLASH, "Actualizacion: sin flash SPI");
        return;
    }

    img_size = b.size;
    img_crc = b.crc32;
    staging = w25q_size() - 2u * STAGING_SIZE;   /* lejos del sector de ajustes */
    active = true;

    ui_show("Actualizando firmware...\nborrando");
    for (uint32_t a = 0; a < img_size; a += 0x10000u)
        w25q_erase_block64(staging + a);

    ui_show("Actualizando firmware...\nrecibiendo");
    ui_progress();
    ack(DL_FW_OK);
}

static void on_data(const uint8_t *p, uint8_t len)
{
    uint32_t off;

    if (!active) {
        ack(DL_FW_ERR_STATE);
        return;
    }
    if (len < 5u)
        return;
    memcpy(&off, p, 4);
    len = (uint8_t)(len - 4u);

    if (off + len <= next_off) {          /* bloque repetido: ya lo tenemos */
        ack(DL_FW_OK);
        return;
    }
    if (off != next_off || off + len > img_size) {
        ack(DL_FW_ERR_SEQ);
        return;
    }

    w25q_write(staging + off, p + 4, len);
    next_off += len;

    if ((next_off & 0x3FFu) < len || next_off == img_size)
        ui_progress();
    ack(DL_FW_OK);
}

static void on_end(void)
{
    uint8_t buf[256];
    uint32_t crc = 0;

    if (!active) {
        ack(DL_FW_ERR_STATE);
        return;
    }
    if (next_off != img_size) {
        ack(DL_FW_ERR_SEQ);
        return;
    }

    ui_show("Actualizando firmware...\nverificando");
    for (uint32_t a = 0; a < img_size; a += sizeof(buf)) {
        uint32_t n = img_size - a < sizeof(buf) ? img_size - a : sizeof(buf);
        w25q_read(staging + a, buf, n);
        crc = crc32_update(crc, buf, n);
    }
    if (crc != img_crc) {
        fail(DL_FW_ERR_CRC, "Actualizacion: CRC incorrecto");
        return;
    }

    /* Tabla de vectores: SP inicial en RAM y Reset_Handler dentro de la app */
    uint32_t vec[2];
    w25q_read(staging, vec, sizeof(vec));
    if (vec[0] < RAM_START || vec[0] > RAM_END ||
        vec[1] < APP_ADDR || vec[1] >= APP_ADDR + img_size || !(vec[1] & 1u)) {
        fail(DL_FW_ERR_IMAGE, "Actualizacion: imagen invalida");
        return;
    }

    ack(DL_FW_OK);
    ui_show("Instalando, no apagar...");
    HAL_Delay(50);                        /* que salga el ACK por el UART */

    __disable_irq();
    SysTick->CTRL = 0;
    install_from_ram(staging, img_size);
}

void fwupdate_handle(uint8_t type, const uint8_t *payload, uint8_t len)
{
    switch (type) {
    case DL_MSG_FW_BEGIN: on_begin(payload, len); break;
    case DL_MSG_FW_DATA:  on_data(payload, len);  break;
    case DL_MSG_FW_END:   on_end();               break;
    default: break;
    }
    last_rx_ms = HAL_GetTick();
}

void fwupdate_poll(void)
{
    uint32_t idle = HAL_GetTick() - last_rx_ms;

    if (active && idle > TIMEOUT_MS) {
        active = false;
        ui_show("Actualizacion cancelada (sin datos)");
    }
    /* Los mensajes de error se quitan solos después de unos segundos */
    if (!active && box && idle > MSG_MS)
        ui_hide();
}

bool fwupdate_active(void)
{
    return active;
}
