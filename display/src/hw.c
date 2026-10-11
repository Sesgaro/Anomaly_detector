#include "hw.h"
#include "board.h"

static TIM_HandleTypeDef htim4;

static volatile bool     buzzer_alarm;   /* patrón 200 ms encendido / 800 ms apagado */
static volatile uint16_t buzzer_beep_ms; /* pitido corto (toques en la pantalla)    */
static volatile uint16_t buzzer_phase;

static void clock_config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* HSE 8 MHz -> PLL: 8 / 8 * 240 / 2 = 120 MHz; USB = 240 / 5 = 48 MHz */
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 8;
    osc.PLL.PLLN = 240;
    osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLQ = 5;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
        for (;;) {}

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;   /* 120 MHz */
    clk.APB1CLKDivider = RCC_HCLK_DIV4;    /* 30 MHz (timers a 60 MHz) */
    clk.APB2CLKDivider = RCC_HCLK_DIV2;    /* 60 MHz */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_3) != HAL_OK)
        for (;;) {}
}

static void backlight_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    TIM_OC_InitTypeDef oc = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_TIM4_CLK_ENABLE();

    gpio.Pin = BL_PIN;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF2_TIM4;
    HAL_GPIO_Init(BL_PORT, &gpio);

    /* 60 MHz / 60 = 1 MHz; periodo 1000 -> PWM de 1 kHz */
    htim4.Instance = TIM4;
    htim4.Init.Prescaler = 59;
    htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim4.Init.Period = 999;
    htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_PWM_Init(&htim4);

    oc.OCMode = TIM_OCMODE_PWM1;
    oc.Pulse = 0;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    HAL_TIM_PWM_ConfigChannel(&htim4, &oc, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
}

static void buzzer_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = BUZZER_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BUZZER_PORT, &gpio);
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
}

void hw_init(void)
{
    /* El bootloader de BTT salta aquí con sus periféricos configurados:
     * apagar SysTick, limpiar interrupciones y volver al HSI antes de
     * reconfigurar el PLL. */
    __disable_irq();
    SysTick->CTRL = 0;
    for (unsigned i = 0; i < sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0]); i++) {
        NVIC->ICER[i] = 0xFFFFFFFFu;
        NVIC->ICPR[i] = 0xFFFFFFFFu;
    }
    SCB->VTOR = APP_VECTOR_ADDR;
    HAL_RCC_DeInit();
    __enable_irq();

    HAL_Init();
    clock_config();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    backlight_init();
    buzzer_init();
}

void hw_backlight_set(uint8_t percent)
{
    if (percent > 100)
        percent = 100;
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, percent * 10u);
}

void hw_buzzer_set(bool on)
{
    buzzer_alarm = on;
}

void hw_buzzer_beep(uint16_t ms)
{
    buzzer_beep_ms = ms;
}

void hw_tick_1ms(void)
{
    bool sound = false;

    if (buzzer_beep_ms) {
        buzzer_beep_ms--;
        sound = true;
    } else if (buzzer_alarm) {
        buzzer_phase = (uint16_t)((buzzer_phase + 1u) % 1000u);
        sound = buzzer_phase < 200u;
    }

    /* Conmutar a 1 kHz da un tono de 500 Hz (sirve para buzzer pasivo o activo) */
    if (sound)
        HAL_GPIO_TogglePin(BUZZER_PORT, BUZZER_PIN);
    else
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
}
