# Pantalla BTT TFT35 B1 V3.0

Pantalla táctil de 3.5" de BIGTREETECH, reutilizada de una impresora 3D. Es
el **punto de partida** del proyecto (fase 0): se le carga un firmware propio
hecho con PlatformIO (carpeta `display/`).

## Placa

- MCU: **STM32F207VCT6** (Cortex-M3, 120 MHz, 256 KB de flash, 128 KB de RAM).
  Hay lotes "GD" con GD32F205VCT6, así que hay que revisar la marca del chip.
- Cristal HSE de 8 MHz.
- LCD de 480×320 por FSMC de 16 bits (ILI9488, ST7796S o NT35310, según el lote).
- Táctil resistivo XPT2046.
- Flash SPI W25Qxx (SPI3, CS PB6), usada por el firmware de BTT para íconos y
  fuentes.
- USB host, ranura micro-SD, header ESP-01, conector SWD y conectores
  EXP1/EXP2/RS232.

## Pines usados por el firmware propio

Confirmados con las definiciones de hardware del firmware abierto de BTT
(`pin_TFT35_V3_0.h`, que la variante B1 reutiliza):

| Función | Pines |
|---|---|
| LCD (FSMC) | D0–D15: PD14, PD15, PD0, PD1, PE7–PE15, PD8–PD10; NOE PD4, NWE PD5, NE1 PD7, RS = A23 PE2 |
| Retroiluminación | PD12 (TIM4_CH1) |
| Buzzer | PD13 |
| Táctil XPT2046 | SCK PE5, MOSI PE3, MISO PE4, CS PE6, PEN PC13 |
| Flash W25Qxx | SPI3: SCK PB3, MISO PB4, MOSI PB5; CS PB6 |
| UART hacia la H745 | USART2: TX PA2, RX PA3 |
| SD (SPI1) | SCK PA5, MISO PA6, MOSI PA7, CS PA4, detección PC4 |

## Conectores (pinout tentativo)

> El pinout de EXP1/EXP2 proviene de una imagen anotada de la comunidad marcada
> como *suposición no verificada*. Los pines del MCU coinciden con el firmware
> de BTT, pero conviene verificar con un multímetro antes de conectar algo.

**EXP1** (10 pines; hilos 1–10 del cable plano)

| Pin | Señal | MCU |
|---|---|---|
| 1 | BTN | PC8 |
| 2 | BEEP | PD13 |
| 3 | LCD-CS | PB12 |
| 4 | LCD-EN | PB15 |
| 5 | LCD-D5 | PB14 |
| 6 | LCD-D4 | PB13 |
| 7 | LCD-D7 | PC6 |
| 8 | LCD-D6 | PC7 |
| 9 | +5V | — |
| 10 | GND | — |

**EXP2** (numerado como los hilos 11–18 del cable plano en la imagen)

| Hilo | Señal | MCU |
|---|---|---|
| 11 | SD-CLK | PA5 |
| 12 | SD-MISO | PA6 |
| 13 | SD-CS | PA4 |
| 14 | ENCA | PA8 |
| 15 | SD-MOSI | PA7 |
| 16 | ENCB | PC9 |
| 17 | RESET | RST |
| 18 | SD-DET | PC4 |
| — | NC / GND | — |

**RS232 / TFT** (5 pines; hilos 19–20 = RX2/TX2): el que se usa para el enlace con la STM32H745.

| Señal | MCU |
|---|---|
| RST | reset |
| RX2 | PA3 (USART2 RX) |
| TX2 | PA2 (USART2 TX) |
| GND | — |
| +5V | alimentación de la pantalla |

Los niveles del UART son de 3.3 V (lógica del STM32F207), compatibles con la
H745. La pantalla se alimenta con 5 V por este conector. RESET, +5V y GND están
compartidos entre conectores.

**SWD** (5 pines): 3.3V, DIO (PA13), GND, CLK (PA14), RST. Sirve para
programar y depurar con un ST-Link.

## Memoria y actualización

- El bootloader de BTT ocupa `0x08000000–0x08007FFF`; la aplicación arranca en
  **`0x08008000`**.
- **Por microSD** (conserva el bootloader): copiar
  `display/.pio/build/tft35_b1/BIGTREE_TFT35_V3.0_B1.bin` a la raíz de una
  microSD FAT32 de 8 GB o menos y reiniciar la pantalla. El bootloader lo
  instala y lo renombra a `.CUR`.
- **Por SWD** (ST-Link): `pio run -t upload` escribe desde `0x08008000` sin
  tocar el bootloader.
- Respaldo: antes de nada conviene guardar la flash original completa con el
  ST-Link (`st-flash read respaldo_tft35.bin 0x08000000 0x40000`), para poder
  volver al firmware de impresora.
