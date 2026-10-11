/*
 * Pines de la BTT TFT35 B1 V3.0 (STM32F207VCT6, HSE de 8 MHz).
 * Fuente: definiciones de hardware del firmware abierto de BIGTREETECH
 * (variante TFT35 V3.0 / E3 / B1).
 */
#ifndef BOARD_H
#define BOARD_H

#include "stm32f2xx_hal.h"

/* LCD: FSMC banco 1 (NE1 = PD7), RS = A23 (PE2), bus de 16 bits.
 * Con RS en A23 y bus de 16 bits, LCD_REG queda en 0x60FFFFFE y LCD_RAM en
 * 0x61000000. */
#define LCD_BASE_ADDR   (0x60000000u | 0x00FFFFFEu)
#define LCD_WIDTH       480
#define LCD_HEIGHT      320

/* Retroiluminación: PD12 = TIM4_CH1 */
#define BL_PORT         GPIOD
#define BL_PIN          GPIO_PIN_12

/* Buzzer */
#define BUZZER_PORT     GPIOD
#define BUZZER_PIN      GPIO_PIN_13

/* Táctil XPT2046 (SPI por software) */
#define TP_SCK_PORT     GPIOE
#define TP_SCK_PIN      GPIO_PIN_5
#define TP_MOSI_PORT    GPIOE
#define TP_MOSI_PIN     GPIO_PIN_3
#define TP_MISO_PORT    GPIOE
#define TP_MISO_PIN     GPIO_PIN_4
#define TP_CS_PORT      GPIOE
#define TP_CS_PIN       GPIO_PIN_6
#define TP_PEN_PORT     GPIOC
#define TP_PEN_PIN      GPIO_PIN_13

/* Flash SPI W25Qxx en SPI3 (PB3 SCK, PB4 MISO, PB5 MOSI), CS = PB6 */
#define FLASH_CS_PORT   GPIOB
#define FLASH_CS_PIN    GPIO_PIN_6

/* Enlace con la STM32H745: USART2 (TX PA2, RX PA3), conector RS232/TFT */
#define LINK_UART       USART2

#endif /* BOARD_H */
