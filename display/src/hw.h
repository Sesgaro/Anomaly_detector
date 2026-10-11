#ifndef HW_H
#define HW_H

#include <stdint.h>
#include <stdbool.h>

/* Deja el MCU limpio después del bootloader de BTT y configura 120 MHz. */
void hw_init(void);

/* Retroiluminación en porcentaje (0–100), por PWM en TIM4_CH1. */
void hw_backlight_set(uint8_t percent);

/* Buzzer: el patrón se genera en SysTick. */
void hw_buzzer_set(bool on);
void hw_buzzer_beep(uint16_t ms);

/* Llamado desde SysTick_Handler cada 1 ms. */
void hw_tick_1ms(void);

#endif /* HW_H */
