/*
 * STM32H745 (núcleo M7) — primera versión:
 *
 *  - Enlace con la pantalla por USART6 (PG14 = TX -> RX2 de la pantalla,
 *    PG9 = RX <- TX2 de la pantalla), 115200 8N1.
 *  - Al arrancar comprueba el firmware de la pantalla y, si no coincide con el
 *    incrustado, se lo manda (display_fwpush).
 *  - Después le envía datos simulados del detector a 10 Hz y atiende sus
 *    pedidos (umbrales, configuración, reconocimiento de alarma), para probar
 *    la interfaz mientras no estén los sensores.
 *
 * LEDs de la Nucleo: LD1 (verde) latido, LD2 (amarillo) actualizando la
 * pantalla, LD3 (rojo) falla de la actualización o sin respuesta de la
 * pantalla. Botón USER (B1): fuerza la actualización de la pantalla.
 *
 * Modo programador: si la H745 arranca con USER presionado, suelta su SWD
 * (PA13/PA14 en analógico) para que el ST-Link de la Nucleo programe la
 * pantalla por SWD (recuperación). LD3 parpadea mientras está en este modo.
 */
#include "stm32h7xx_hal.h"
#include "display_link.h"
#include "display_fwpush.h"
#include "display_fw_info.h"
#include "demo.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

extern const uint8_t display_fw_image[];

#define LD1_PORT GPIOB
#define LD1_PIN  GPIO_PIN_0
#define LD2_PORT GPIOE
#define LD2_PIN  GPIO_PIN_1
#define LD3_PORT GPIOB
#define LD3_PIN  GPIO_PIN_14
#define BTN_PORT GPIOC
#define BTN_PIN  GPIO_PIN_13

#define RX_BUF   512u

static UART_HandleTypeDef huart6;
static UART_HandleTypeDef huart3;    /* puerto virtual del ST-Link (log) */
static volatile uint8_t  rx_buf[RX_BUF];
static volatile uint16_t rx_head;
static uint16_t rx_tail;

static dl_parser_t parser;
static fwpush_t    push;

/* ------------------------------------------------------------- hardware */

static void gpio_init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin = LD1_PIN | LD3_PIN;
    HAL_GPIO_Init(GPIOB, &g);
    g.Pin = LD2_PIN;
    HAL_GPIO_Init(GPIOE, &g);

    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_NOPULL;
    g.Pin = BTN_PIN;
    HAL_GPIO_Init(BTN_PORT, &g);
}

static void uart_init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_USART6_CLK_ENABLE();

    g.Pin = GPIO_PIN_9 | GPIO_PIN_14;       /* PG9 RX, PG14 TX */
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_MEDIUM;
    g.Alternate = GPIO_AF7_USART6;
    HAL_GPIO_Init(GPIOG, &g);

    huart6.Instance = USART6;
    huart6.Init.BaudRate = DL_BAUDRATE;
    huart6.Init.WordLength = UART_WORDLENGTH_8B;
    huart6.Init.StopBits = UART_STOPBITS_1;
    huart6.Init.Parity = UART_PARITY_NONE;
    huart6.Init.Mode = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart6);

    __HAL_UART_ENABLE_IT(&huart6, UART_IT_RXNE);
    HAL_NVIC_SetPriority(USART6_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART6_IRQn);
}

/* Log por USART3 (PD8 TX / PD9 RX), que la Nucleo conecta al puerto
 * virtual del ST-Link: /dev/ttyACM0 en Linux, 115200 baud. */
static void log_init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_USART3_CLK_ENABLE();
    g.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOD, &g);

    huart3.Instance = USART3;
    huart3.Init.BaudRate = 115200;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart3);
}

static void log_printf(const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0)
        HAL_UART_Transmit(&huart3, (uint8_t *)buf, (uint16_t)(n < (int)sizeof(buf) ? n : (int)sizeof(buf) - 1), 50);
}

static const char *state_name(fwpush_state_t s)
{
    static const char *names[] = { "espera", "consultando", "borrando", "enviando",
                                   "verificando", "reiniciando", "al dia", "fallo" };
    return (unsigned)s < sizeof(names) / sizeof(names[0]) ? names[s] : "?";
}

void USART6_IRQHandler(void)
{
    uint32_t isr = USART6->ISR;

    if (isr & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE))
        USART6->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF;
    if (isr & USART_ISR_RXNE_RXFNE) {
        uint8_t b = (uint8_t)USART6->RDR;
        uint16_t next = (uint16_t)((rx_head + 1u) % RX_BUF);
        if (next != rx_tail) {
            rx_buf[rx_head] = b;
            rx_head = next;
        }
    }
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

static void uart_send(const uint8_t *data, uint32_t len)
{
    HAL_UART_Transmit(&huart6, (uint8_t *)data, (uint16_t)len, 50);
}

void link_send(uint8_t type, const void *payload, uint8_t len)
{
    uint8_t buf[DL_MAX_PAYLOAD + 5];
    size_t n = dl_encode(type, payload, len, buf);
    uart_send(buf, (uint32_t)n);
}

/* ------------------------------------------------------------- lazo */

static void handle_frame(uint32_t now)
{
    if (fwpush_on_frame(&push, parser.type, parser.payload, parser.len, now))
        return;
    if (!fwpush_busy(&push))
        demo_on_frame(parser.type, parser.payload, parser.len);
}

static void programmer_mode(void)
{
    log_printf("[H745] modo programador: SWD liberado para la pantalla\r\n");
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD13 | GPIO_PUPDR_PUPD14);
    GPIOA->MODER |= GPIO_MODER_MODE13 | GPIO_MODER_MODE14;
    for (;;) {
        HAL_GPIO_TogglePin(LD3_PORT, LD3_PIN);
        HAL_Delay(250);
    }
}

int main(void)
{
    HAL_Init();          /* HSI a 64 MHz; los relojes rápidos vendrán después */
    gpio_init();
    log_init();
    if (HAL_GPIO_ReadPin(BTN_PORT, BTN_PIN) == GPIO_PIN_SET)
        programmer_mode();
    uart_init();
    log_printf("\r\n[H745] arranque. Firmware de pantalla incrustado: %lu bytes, CRC %08lX\r\n",
               (unsigned long)DISPLAY_FW_SIZE, (unsigned long)DISPLAY_FW_CRC32);

    push.image = display_fw_image;
    push.size = DISPLAY_FW_SIZE;
    push.crc32 = DISPLAY_FW_CRC32;
    push.send = uart_send;
    fwpush_init(&push, HAL_GetTick());
    demo_init();

    uint32_t last_blink = 0, btn_t0 = 0, last_log = 0;
    bool btn_prev = false;
    fwpush_state_t prev_state = push.state;

    for (;;) {
        uint32_t now = HAL_GetTick();

        while (rx_tail != rx_head) {
            uint8_t b = rx_buf[rx_tail];
            rx_tail = (uint16_t)((rx_tail + 1u) % RX_BUF);
            if (dl_parse_byte(&parser, b))
                handle_frame(now);
        }

        fwpush_poll(&push, now);
        if (push.state != prev_state) {
            log_printf("[H745] pantalla: %s", state_name(push.state));
            if (push.state == FWPUSH_FAILED)
                log_printf(" (error %u)", push.last_error);
            if (push.state == FWPUSH_REBOOT)
                log_printf(" (transferencia en %lu ms)", (unsigned long)(now - push.start_ms));
            log_printf("\r\n");
            prev_state = push.state;
        }
        if (push.state == FWPUSH_DATA && now - last_log >= 2000u) {
            last_log = now;
            log_printf("[H745] enviando %u%%\r\n", fwpush_progress(&push));
        }
        if (!fwpush_busy(&push) && push.state != FWPUSH_IDLE && push.state != FWPUSH_QUERY)
            demo_poll(now);

        /* Botón USER: forzar actualización (flanco de subida, antirrebote) */
        bool btn = HAL_GPIO_ReadPin(BTN_PORT, BTN_PIN) == GPIO_PIN_SET;
        if (btn && !btn_prev && now - btn_t0 > 300u) {
            btn_t0 = now;
            log_printf("[H745] boton USER: forzando actualizacion\r\n");
            fwpush_force(&push, now);
        }
        btn_prev = btn;

        /* LEDs */
        if (now - last_blink >= (fwpush_busy(&push) ? 100u : 500u)) {
            last_blink = now;
            HAL_GPIO_TogglePin(LD1_PORT, LD1_PIN);
        }
        HAL_GPIO_WritePin(LD2_PORT, LD2_PIN, fwpush_busy(&push) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LD3_PORT, LD3_PIN,
                          push.state == FWPUSH_FAILED ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}
