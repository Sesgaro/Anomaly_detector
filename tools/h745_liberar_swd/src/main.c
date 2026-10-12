/*
 * Libera el SWD de la STM32H745 de la Nucleo.
 *
 * Tras 2 s (margen para reprogramarla sin "connect under reset"), pone PA13
 * (SWDIO) y PA14 (SWCLK) en modo analógico: la H745 deja de responder por
 * SWD y el ST-Link de la Nucleo queda libre para programar otro chip por CN5.
 * LD1 (verde, PB0) parpadea mientras el SWD está liberado.
 *
 * Para volver a programar la H745: STM32CubeProgrammer con
 * "Mode: Under reset" (o mantener RESET presionado al conectar).
 *
 * No toca la configuración de alimentación (PWR_CR3) ni los relojes: corre
 * con el HSI de reset.
 */
#include "stm32h7xx.h"

static void delay(volatile uint32_t n)
{
    while (n--) {}
}

int main(void)
{
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOBEN;
    (void)RCC->AHB4ENR;

    /* LD1 = PB0, salida */
    GPIOB->MODER = (GPIOB->MODER & ~GPIO_MODER_MODE0) | GPIO_MODER_MODE0_0;

    /* Margen de 2 s con LD1 encendido fijo */
    GPIOB->BSRR = GPIO_BSRR_BS0;
    delay(64000000u / 8u * 2u);

    /* PA13/PA14: modo analógico y sin pull-up/pull-down */
    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD13 | GPIO_PUPDR_PUPD14);
    GPIOA->MODER |= GPIO_MODER_MODE13 | GPIO_MODER_MODE14;

    for (;;) {
        GPIOB->ODR ^= GPIO_ODR_OD0;
        delay(64000000u / 8u / 4u);
    }
}
