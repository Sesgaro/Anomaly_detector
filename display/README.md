# Firmware de la pantalla BTT TFT35 B1

Firmware propio para la pantalla táctil BTT TFT35 B1 V3.0 (STM32F207VCT6),
hecho con PlatformIO, STM32Cube HAL y LVGL 8.3. Muestra el estado que envía la
STM32H745 y permite ajustar los umbrales de las alarmas. Los datos de la placa
están en [docs/display.md](../docs/display.md).

## Interfaz

- **Barra superior**: nivel de alerta (`NORMAL`, `ADVERTENCIA`, `ANOMALIA`,
  `FALLA`) con su color, y estado del enlace con la H745 y de Ethernet. Sin
  tramas por 1.5 s muestra `SIN ENLACE`.
- **Estado**: indicador del ángulo DOA (semicírculo de -90° a 90° con 2
  micrófonos; círculo completo con 4), barras del error del autoencoder, de la
  vibración y de la corriente del motor, coloreadas según los umbrales.
  `ANOMALIA` y `FALLA` hacen sonar el buzzer hasta que se toca **Reconocer
  alarma**.
- **Umbrales**: deslizadores con botones -/+ para los 6 umbrales. **Enviar**
  los manda a la H745; la pantalla muestra "Umbrales aplicados" cuando la H745
  los devuelve confirmados.
- **Ajustes**: arreglo de 2 o 4 micrófonos, modo DOA (barrido o enfoque fijo)
  y ángulo de enfoque, brillo, calibración del táctil y diagnóstico (controlador
  del LCD, tramas recibidas y errores).

La calibración del táctil y el brillo se guardan en el último sector de la
flash SPI W25Qxx de la pantalla. La primera vez que arranca pide calibrar
(3 puntos).

## Compilar

```bash
cd display
pio run
```

Se generan dos archivos en `.pio/build/tft35_b1/`:

- `BIGTREE_TFT35_V3.0_B1.01.x.bin`: el binario con el formato de nombre de los
  firmwares oficiales de BTT, para actualizar desde la microSD.
- `firmware.bin`: el mismo binario, para cargarlo por SWD.

## Cargar

**Por microSD** (no hace falta ST-Link): copiar
`BIGTREE_TFT35_V3.0_B1.01.x.bin` a la raíz de una microSD FAT32 (8 GB o
menos), insertarla en la pantalla y reiniciarla. El bootloader de BTT lo
instala y lo renombra a `.CUR`; si se quiere cargar otra vez el mismo archivo,
hay que quitarle el `.CUR`.

El firmware queda en `0x08008000` y el bootloader de BTT no se toca, así que
**siempre se puede volver al firmware de impresora** copiando a la microSD el
`.bin` oficial (`BIGTREE_TFT35_V3.0_B1.28.x.bin`, o
`BIGTREE_GD_TFT35_V3.0_B1.28.x.bin` si el chip es GD32) de la carpeta
*Copy to SD Card root directory to update* del repositorio
[BIGTREETECH-TouchScreenFirmware](https://github.com/bigtreetech/BIGTREETECH-TouchScreenFirmware).

**Por SWD** (opcional, para depurar): requiere un programador externo
(ST-Link V2/V3 o J-Link) conectado al conector SWD de la pantalla (3.3V, DIO,
GND, CLK, RST) y `pio run -t upload`. El ST-Link integrado de la Nucleo-144
**no** sirve para programar otro microcontrolador: ST lo diseñó solo para el
STM32H7 de la placa.

## Probar sin la STM32H745

`pc/display_sim/sim.py` hace de STM32H745 por un puente USB-UART de 3.3 V. Se
puede usar una **ESP32** como puente con el sketch
[pc/display_sim/esp32_uart_bridge](../pc/display_sim/esp32_uart_bridge/esp32_uart_bridge.ino):

| ESP32 | Pantalla (conector RS232/TFT) |
|---|---|
| GPIO17 (TX2) | RX2 (PA3) |
| GPIO16 (RX2) | TX2 (PA2) |
| GND | GND |
| VIN / 5V | +5V |

```bash
pip install pyserial
python3 pc/display_sim/sim.py /dev/ttyUSB0
```

(El puerto es el de la ESP32: `/dev/ttyUSB0` o `/dev/ttyACM0` en Linux,
`COMx` en Windows.)

El simulador recorre un ciclo normal → oscilación → falla, responde a la
solicitud de parámetros y aplica los umbrales y la configuración que se
cambian en la pantalla. `python3 pc/display_sim/sim.py --self-test` prueba el
protocolo sin hardware.

## Protocolo

Definido en [common/display_link.h](../common/display_link.h), que también
usará el firmware de la STM32H745. Tramas `0xA5 | tipo | len | payload |
CRC-16/CCITT` a 115200 8N1:

| Tipo | Sentido | Contenido |
|---|---|---|
| `0x01` STATUS | H745 → pantalla | nivel, banderas, DOA, error AE, vibración, corrientes, voltaje del motor, uptime |
| `0x02` THRESHOLDS | ambos | umbrales de AE, vibración y corriente (la H745 confirma devolviéndolos) |
| `0x03` CONFIG | ambos | número de micrófonos, modo DOA, ángulo de enfoque |
| `0x04` ACK_ALARM | pantalla → H745 | alarma reconocida |
| `0x05` REQ_PARAMS | pantalla → H745 | pide umbrales y configuración (se repite cada 2 s hasta recibirlos) |

## Pendiente de verificar en la placa

- Orientación de la imagen: si sale girada 180°, cambiar
  `LCD_MADCTL_LANDSCAPE` a `0xE8` en `src/lcd.c`.
- Que el bootloader de BTT acepte el nombre `BIGTREE_TFT35_V3.0_B1.01.x.bin`
  (si no lo instala, renombrarlo exactamente como el oficial:
  `BIGTREE_TFT35_V3.0_B1.28.x.bin`).
- Lotes con GD32F205: el reloj y los periféricos son compatibles, pero no está
  probado.
