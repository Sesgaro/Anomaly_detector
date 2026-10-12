#include "knob.h"
#include "board.h"

/* Transiciones de cuadratura por "clic" mecánico de la rueda. Si un clic
 * avanza 2 pasos o la mitad, cambiar a 2 o 4. Si gira al revés, KNOB_DIR = -1. */
#define TRANSITIONS_PER_STEP 4
#define KNOB_DIR             (-1)

#define ENCA_PORT GPIOA
#define ENCA_PIN  GPIO_PIN_8
#define ENCB_PORT GPIOC
#define ENCB_PIN  GPIO_PIN_9
#define BTN_PORT  GPIOC
#define BTN_PIN   GPIO_PIN_8
#define ENCEN_PIN GPIO_PIN_6     /* GPIOC */
#define LED_PIN   GPIO_PIN_7     /* GPIOC */

static volatile int16_t  steps;
static volatile int8_t   sub;
static volatile uint8_t  last_ab;

static bool     btn_down;
static uint32_t btn_t0;
static bool     long_sent;

static uint32_t last_led_ms;

static inline uint8_t read_ab(void)
{
    /* Activos en bajo: un 1 aquí significa contacto cerrado */
    uint8_t a = (ENCA_PORT->IDR & ENCA_PIN) ? 0u : 1u;
    uint8_t b = (ENCB_PORT->IDR & ENCB_PIN) ? 0u : 1u;
    return (uint8_t)((a << 1) | b);
}

void knob_init(void)
{
    GPIO_InitTypeDef g = {0};

    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    g.Pin = ENCA_PIN;
    HAL_GPIO_Init(ENCA_PORT, &g);
    g.Pin = ENCB_PIN | BTN_PIN;
    HAL_GPIO_Init(GPIOC, &g);

    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Pin = ENCEN_PIN | LED_PIN;
    HAL_GPIO_Init(GPIOC, &g);
    HAL_GPIO_WritePin(GPIOC, ENCEN_PIN | LED_PIN, GPIO_PIN_RESET);

    /* Contador de ciclos para la temporización de los WS2812 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    last_ab = read_ab();
}

void knob_tick_1ms(void)
{
    /* Tabla de transiciones de cuadratura: +1, -1 o 0 (inválida/sin cambio) */
    static const int8_t table[16] = {
         0, -1, +1,  0,
        +1,  0,  0, -1,
        -1,  0,  0, +1,
         0, +1, -1,  0,
    };
    uint8_t ab = read_ab();

    if (ab == last_ab)
        return;
    sub = (int8_t)(sub + table[(last_ab << 2) | ab]);
    last_ab = ab;

    if (sub >= TRANSITIONS_PER_STEP) {
        steps += KNOB_DIR;
        sub = 0;
    } else if (sub <= -TRANSITIONS_PER_STEP) {
        steps -= KNOB_DIR;
        sub = 0;
    }
}

int16_t knob_take_steps(void)
{
    __disable_irq();
    int16_t s = steps;
    steps = 0;
    __enable_irq();
    return s;
}

knob_event_t knob_poll_button(void)
{
    bool down = (BTN_PORT->IDR & BTN_PIN) == 0;
    uint32_t now = HAL_GetTick();

    if (down && !btn_down) {
        btn_down = true;
        btn_t0 = now;
        long_sent = false;
    } else if (down && btn_down) {
        if (!long_sent && now - btn_t0 >= KNOB_LONG_MS) {
            long_sent = true;
            return KNOB_EV_LONG;
        }
    } else if (!down && btn_down) {
        btn_down = false;
        if (!long_sent && now - btn_t0 >= 30u)   /* antirrebote */
            return KNOB_EV_CLICK;
    }
    return KNOB_EV_NONE;
}

/* ------------------------------------------------------------- WS2812 */

#define CYC_T0H   42u     /* 0.35 us a 120 MHz */
#define CYC_T1H  108u     /* 0.90 us */
#define CYC_BIT  150u     /* 1.25 us */

static void ws2812_send(const uint8_t *grb, int n)
{
    uint32_t t;

    __disable_irq();
    for (int i = 0; i < n; i++) {
        for (int bit = 7; bit >= 0; bit--) {
            uint32_t high = (grb[i] & (1u << bit)) ? CYC_T1H : CYC_T0H;
            t = DWT->CYCCNT;
            GPIOC->BSRR = LED_PIN;
            while (DWT->CYCCNT - t < high) {}
            GPIOC->BSRR = (uint32_t)LED_PIN << 16;
            while (DWT->CYCCNT - t < CYC_BIT) {}
        }
    }
    __enable_irq();
}

void knob_set_leds(uint32_t c1, uint32_t c2)
{
    static uint32_t last1 = 0xFFFFFFFFu, last2 = 0xFFFFFFFFu;
    uint32_t now = HAL_GetTick();

    /* Solo si cambió, y con al menos 2 ms entre tramas (reset > 280 us) */
    if ((c1 == last1 && c2 == last2) || now - last_led_ms < 2u)
        return;
    last1 = c1;
    last2 = c2;
    last_led_ms = now;

    uint8_t grb[6] = {
        (uint8_t)(c1 >> 8), (uint8_t)(c1 >> 16), (uint8_t)c1,
        (uint8_t)(c2 >> 8), (uint8_t)(c2 >> 16), (uint8_t)c2,
    };
    ws2812_send(grb, 6);
}
