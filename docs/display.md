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
- **Por microSD** (conserva el bootloader; no requiere programador): copiar
  `display/.pio/build/tft35_b1/BIGTREE_TFT35_V3.0_B1.01.x.bin` a la raíz de
  una microSD FAT32 de 8 GB o menos y reiniciar la pantalla. El bootloader lo
  instala y lo renombra a `.CUR`.
- **Volver al firmware de impresora**: copiar a la microSD el `.bin` oficial
  de BTT para la B1 (`BIGTREE_TFT35_V3.0_B1.28.x.bin`; versión GD:
  `BIGTREE_GD_TFT35_V3.0_B1.28.x.bin`).
- **Por SWD con el ST-Link de la Nucleo**: ver la sección siguiente.

## Programar la pantalla con el ST-Link de la NUCLEO-H745ZI-Q

En la Nucleo, el conector **CN5** (MIPI-10) está conectado a las mismas
líneas SWD que van a la STM32H745 (PA13/PA14). Si se cablea la pantalla a CN5
sin más, el ST-Link ve **dos chips en el mismo bus** y no funciona (o
programa la H745). Hay que sacar a la H745 del bus primero.

### Paso 1: aislar la H745 (sin soldar)

El programa auxiliar `tools/h745_liberar_swd` pone PA13/PA14 de la H745 en
modo analógico después de 2 s, así que la H745 deja de responder por SWD. LD1
(verde) parpadea mientras está activo.

1. Desconectar los cables de la pantalla de CN5.
2. Cargar el programa en la H745 por el USB del ST-Link:
   ```bash
   cd tools/h745_liberar_swd
   pio run -t upload
   ```
3. Esperar a que LD1 parpadee.

Para volver a programar la H745 en las fases siguientes: desconectar la
pantalla de CN5 y conectar en modo **"Under reset"** (STM32CubeProgrammer:
*Mode: Under reset*; o mantener presionado RESET al iniciar la conexión).

Alternativa con soldador: abrir los puentes de soldadura entre las líneas SWD
del ST-Link y la H745. **No pude verificar sus nombres** (el resumen del
UM2408 menciona SB46 y SB51); hay que confirmarlos en el esquemático MB1363
antes de cortar, y resoldarlos después para volver a usar la H745.

### Paso 2: cablear CN5 a la pantalla

CN5 es MIPI-10 (2×5, paso de 1.27 mm). Pinout estándar MIPI-10:

| CN5 | Señal | Pantalla (conector SWD) |
|---|---|---|
| 1 | VTref (3.3 V de la Nucleo) | **no conectar** |
| 2 | SWDIO | DIO |
| 3, 5, 9 | GND | GND |
| 4 | SWCLK | CLK |
| 6 | SWO | no conectar |
| 10 | NRST | **no conectar** (también resetea la H745) |

> El resumen del UM2408 que se usó pone SWDIO en el pin 4 y SWCLK en el 6, lo
> que no coincide con el estándar MIPI-10 ni con otras fuentes (SWDIO = 2,
> SWCLK = 4). Verificar con un multímetro: continuidad entre CN5-2 y el pin
> PA13 del conector morpho CN11, y entre CN5-4 y PA14.

**Alternativa más cómoda que CN5:** como las líneas SWD del ST-Link llegan a
PA13/PA14 de la H745, también salen por el conector morpho **CN11** (pines de
2.54 mm, normalmente CN11-13 = PA13/SWDIO y CN11-15 = PA14/SWCLK; confirmar
con la tabla del UM2408 o con continuidad). Con la H745 aislada por
`tools/h745_liberar_swd`, basta con cables dupont:
CN11 PA13 → DIO, CN11 PA14 → CLK, GND → GND.

- La pantalla se alimenta con su propio 5 V (conector RS232/TFT o el USB de la
  ESP32); **GND común** con la Nucleo.
- La Nucleo se alimenta por el USB del ST-Link (CN1).

### Paso 3: cargar

```bash
cd display
pio run -t upload
```

`scripts/flash_tft35.tcl` lee el ID del chip antes de escribir:

- **0x411** (STM32F20x, la pantalla): escribe desde `0x08008000`, verifica y
  reinicia. El bootloader de BTT no se toca.
- Cualquier otro (p. ej. **0x450**, la STM32H745): aborta sin escribir nada.

Si OpenOCD no puede abrir el ST-Link (`LIBUSB_ERROR_ACCESS`), faltan las
reglas udev de PlatformIO:

```bash
curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core/develop/platformio/assets/system/99-platformio-udev.rules | sudo tee /etc/udev/rules.d/99-platformio-udev.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
```

Con STM32CubeProgrammer: conectar por SWD, comprobar que el *Device ID* sea
**0x411** (STM32F2xx) y no 0x450 (STM32H7), y cargar
`display/.pio/build/tft35_b1/firmware.bin` en la dirección **0x08008000** sin
borrar todo el chip (*Full chip erase* desactivado).
