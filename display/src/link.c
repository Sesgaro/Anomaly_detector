#include "link.h"
#include "board.h"
#include "fwupdate.h"
#include <string.h>

#define RX_BUF_SIZE 256u   /* potencia de 2 */
#define REQ_PERIOD_MS 2000u

static UART_HandleTypeDef huart;
static dl_parser_t parser;

static volatile uint8_t  rx_buf[RX_BUF_SIZE];
static volatile uint16_t rx_head;
static uint16_t rx_tail;
static volatile uint32_t rx_overruns;

static dl_status_t     status;
static dl_thresholds_t thresholds;
static dl_config_t     config;
static bool            have_thresholds, have_config;
static uint32_t        last_status_ms;
static bool            ever_status;
static uint32_t        last_req_ms;
static uint32_t        frames_ok;

void link_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_USART2_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;     /* TX PA2, RX PA3 */
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
    gpio.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &gpio);

    huart.Instance = LINK_UART;
    huart.Init.BaudRate = DL_BAUDRATE;
    huart.Init.WordLength = UART_WORDLENGTH_8B;
    huart.Init.StopBits = UART_STOPBITS_1;
    huart.Init.Parity = UART_PARITY_NONE;
    huart.Init.Mode = UART_MODE_TX_RX;
    huart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart);

    /* Recepción por interrupción, byte a byte, hacia un buffer circular */
    __HAL_UART_ENABLE_IT(&huart, UART_IT_RXNE);
    HAL_NVIC_SetPriority(USART2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

void link_irq(void)
{
    uint32_t sr = LINK_UART->SR;

    if (sr & (USART_SR_RXNE | USART_SR_ORE)) {
        uint8_t b = (uint8_t)LINK_UART->DR;   /* leer DR limpia RXNE y ORE */
        uint16_t next = (uint16_t)((rx_head + 1u) & (RX_BUF_SIZE - 1u));
        if (sr & USART_SR_ORE)
            rx_overruns++;
        if (next != rx_tail) {
            rx_buf[rx_head] = b;
            rx_head = next;
        } else {
            rx_overruns++;
        }
    }
}

void link_send(uint8_t type, const void *payload, uint8_t len)
{
    uint8_t frame[DL_MAX_PAYLOAD + 5];
    size_t n = dl_encode(type, payload, len, frame);
    HAL_UART_Transmit(&huart, frame, (uint16_t)n, 20);
}

static void handle_frame(void)
{
    frames_ok++;

    switch (parser.type) {
    case DL_MSG_STATUS:
        if (parser.len == sizeof(status)) {
            memcpy(&status, parser.payload, sizeof(status));
            last_status_ms = HAL_GetTick();
            ever_status = true;
        }
        break;
    case DL_MSG_THRESHOLDS:
        if (parser.len == sizeof(thresholds)) {
            memcpy(&thresholds, parser.payload, sizeof(thresholds));
            have_thresholds = true;
        }
        break;
    case DL_MSG_CONFIG:
        if (parser.len == sizeof(config)) {
            memcpy(&config, parser.payload, sizeof(config));
            have_config = true;
        }
        break;
    case DL_MSG_FW_BEGIN:
    case DL_MSG_FW_DATA:
    case DL_MSG_FW_END:
        fwupdate_handle(parser.type, parser.payload, parser.len);
        break;
    default:
        break;
    }
}

void link_poll(void)
{
    while (rx_tail != rx_head) {
        uint8_t b = rx_buf[rx_tail];
        rx_tail = (uint16_t)((rx_tail + 1u) & (RX_BUF_SIZE - 1u));
        if (dl_parse_byte(&parser, b))
            handle_frame();
    }

    /* Mientras no tengamos umbrales y configuración, pedirlos periódicamente */
    uint32_t now = HAL_GetTick();
    if ((!have_thresholds || !have_config) && !fwupdate_active() &&
        (now - last_req_ms) >= REQ_PERIOD_MS) {
        last_req_ms = now;
        link_send(DL_MSG_REQ_PARAMS, NULL, 0);
    }
}

bool link_alive(void)
{
    return ever_status && (HAL_GetTick() - last_status_ms) < LINK_TIMEOUT_MS;
}

const dl_status_t *link_status(void)
{
    return &status;
}

bool link_thresholds(dl_thresholds_t *out)
{
    if (have_thresholds)
        *out = thresholds;
    return have_thresholds;
}

bool link_config(dl_config_t *out)
{
    if (have_config)
        *out = config;
    return have_config;
}

void link_send_thresholds(const dl_thresholds_t *t)
{
    link_send(DL_MSG_THRESHOLDS, t, sizeof(*t));
}

void link_send_config(const dl_config_t *c)
{
    link_send(DL_MSG_CONFIG, c, sizeof(*c));
}

void link_send_ack_alarm(void)
{
    link_send(DL_MSG_ACK_ALARM, NULL, 0);
}

uint32_t link_rx_frames(void)
{
    return frames_ok;
}

uint32_t link_rx_errors(void)
{
    return rx_overruns;
}
