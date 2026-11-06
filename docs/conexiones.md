# Conexiones de los sensores a la NUCLEO-H745ZI-Q

Todo va por los conectores **ST Zio CN7, CN8, CN9 y CN10**. Pines y funciones
según el UM2408 (tabla 17 y figura 17) que está en esta carpeta.

## Resumen por periférico

| Periférico | Bus de la H745 | Pines |
|---|---|---|
| 4× INMP441 | SAI1 bloque A (maestro) + bloque B (síncrono) | PE5, PE4, PE6, PE3 |
| MPU-9250/6500 | SPI1 | PA5, PA6, PB5, PD14 (CS), PD15 (INT) |
| INA3221 | I2C1 | PB8, PB9, PE11 (CRITICAL), PE14 (WARNING) |
| Pantalla TFT35 | USART6 | PG14 (TX), PG9 (RX) — ya cableada |

## Micrófonos INMP441 (SAI1, AF6)

Los cuatro micrófonos comparten el reloj (SCK) y la selección de canal (WS).
Cada línea de datos lleva dos micrófonos: uno en el canal izquierdo
(`L/R` a GND) y otro en el derecho (`L/R` a 3V3). El bloque B del SAI1 trabaja
**síncrono** con el bloque A, así que usa su mismo SCK/WS por dentro y las
cuatro señales se muestrean con el mismo reloj (necesario para el DOA).

| Señal | Pin H745 | Conector | Va a |
|---|---|---|---|
| SCK (SAI1_SCK_A) | PE5 | **CN9-18** (D58) | SCK de los 4 micrófonos |
| WS (SAI1_FS_A) | PE4 | **CN9-16** (D57) | WS de los 4 micrófonos |
| SD A (SAI1_SD_A) | PE6 | **CN9-20** (D59) | SD de mic 1 y mic 2 |
| SD B (SAI1_SD_B) | PE3 | **CN9-22** (D60) | SD de mic 3 y mic 4 |
| 3V3 | — | CN8-7 | VDD de los 4 micrófonos |
| GND | — | CN9-23 | GND de los 4 micrófonos |

| Micrófono | Línea de datos | `L/R` | Canal |
|---|---|---|---|
| Mic 1 | SD A (PE6) | GND | izquierdo, slot 0 |
| Mic 2 | SD A (PE6) | 3V3 | derecho, slot 1 |
| Mic 3 | SD B (PE3) | GND | izquierdo, slot 0 |
| Mic 4 | SD B (PE3) | 3V3 | derecho, slot 1 |

- Con 2 micrófonos solo se usa SD A (mic 1 y mic 2).
- Formato: 48 kHz, ranuras de 32 bits → SCK = 64 × 48 kHz = 3.072 MHz.
- Una resistencia de 100 kΩ de cada línea SD a GND, porque cada
  micrófono deja la línea en alta impedancia cuando no es su ranura.
- PE6 también sale en CN10-28 (D38): es el mismo pin, no usar los dos.
- No hace falta MCLK (PE2): el INMP441 no lo usa.

## IMU MPU-9250/6500 (SPI1, AF5)

| Señal del módulo | Pin H745 | Conector |
|---|---|---|
| SCL/SCLK | PA5 (SPI1_SCK) | **CN7-10** (D13) |
| SDO/AD0 (MISO) | PA6 (SPI1_MISO) | **CN7-12** (D12) |
| SDA/SDI (MOSI) | PB5 (SPI1_MOSI) | **CN7-14** (D11) |
| NCS | PD14 (GPIO) | **CN7-16** (D10) |
| INT | PD15 (EXTI15) | **CN7-18** (D9) |
| VCC | — | CN8-7 (3V3) |
| GND | — | CN7-8 |

- En la H745ZI-Q, D11 va a PB5 de fábrica (SB57 ON); PA7 queda para Ethernet.
- SPI a 1 MHz para configurar y hasta 20 MHz para leer datos (límites del
  MPU-9250).

## Monitor de corriente INA3221 (I2C1, AF4)

| Señal del módulo | Pin H745 | Conector |
|---|---|---|
| SCL | PB8 (I2C1_SCL) | **CN7-2** (D15) |
| SDA | PB9 (I2C1_SDA) | **CN7-4** (D14) |
| CRITICAL (open-drain) | PE11 (EXTI11) | **CN10-6** (D5) |
| WARNING (open-drain) | PE14 (EXTI14) | **CN10-8** (D4) |
| VS | — | CN8-7 (3V3) |
| GND | — | CN10-5 |

- Dirección 0x40 (`A0` a GND).
- Pull-ups de 10 kΩ a 3V3 en SCL, SDA, CRITICAL y WARNING (muchos módulos ya
  traen las de I2C).
- Canal 3 en serie con la línea de 12 V del motor (IN+ del lado de la
  fuente, IN− del lado del motor), con conexión Kelvin al shunt.
- WARNING va en PE14 y no en PE13 (D3), porque PE13 comparte la línea EXTI13
  con el botón USER (PC13).

## Pantalla BTT TFT35 B1 (USART6, AF7) — ya cableada

| Señal | Pin H745 | Conector | Pantalla |
|---|---|---|---|
| TX | PG14 | **CN10-12** (D2) | RX2 (PA3) |
| RX | PG9 | **CN7-20** (D8) | TX2 (PA2) |
| 5V | — | CN8-9 | +5V |
| GND | — | CN8-11 | GND |

- PG9 es `SMPS_EN` solo si se monta un SMPS externo en CN6 (SB35 ON); con la
  configuración de fábrica va al Zio.
- Con la Nucleo alimentada por el USB del ST-Link, la retroiluminación de la
  pantalla sale de ese mismo 5 V; en el shield conviene alimentarla aparte.

## Pines del Zio que no se deben usar

| Pin | Conector | Motivo |
|---|---|---|
| PA2 | CN10-11 (A8) | Ethernet RMII_MDIO |
| PB13 | CN7-5 (D18) | Ethernet RMII_TXD1 |
| PB0 | CN10-31 (D33) | LED LD1 |
| PB14 | CN9-29 (D65) | LED LD3 |
| PB3 | CN7-15 (D23) | SWO del ST-Link |
| PE13 | CN10-10 (D3) | EXTI13 compartida con el botón USER |
| PG8, PG10, PG12 | CN8-16, CN8-14, CN10-2 | SMPS externo en CN6 (libres de fábrica, pero mejor evitarlos) |

## Nota para el firmware

Al subir los relojes de la H745, `SystemClock_Config` debe llamar a
`HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY)` (configuración de fábrica de
la placa). Con otra configuración, la placa queda bloqueada y hay que
recuperarla con BOOT0 (UM2408, sección 7.4.8).
