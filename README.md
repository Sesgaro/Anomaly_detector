# Anomaly_detector

Detector de anomalías acústicas y de vibración embebido en una **STM32H745ZIT6**
(doble núcleo Cortex-M7 + Cortex-M4), con estimación de **dirección de llegada
(DOA)** del sonido. La STM32 clasifica el estado de una máquina (un motor
controlado por PID desde una PC) y envía **alertas por Ethernet** según el
nivel de anomalía detectado.

---

## 1. Visión general

- Un arreglo de micrófonos **INMP441 (I2S)** escucha la máquina.
- El **M7** estima de dónde viene el sonido (DOA), enfoca el arreglo hacia esa
  dirección (beamforming) y pasa la señal por un **autoencoder** entrenado solo
  con sonido *normal*. Si el autoencoder no logra reconstruir bien la señal, el
  error de reconstrucción sube y eso indica una anomalía.
- El **M4** lee una IMU **MPU-9250/6500** por SPI (vibración) y un monitor de
  corriente **INA3221** por I2C (picos de corriente del motor cuando oscila),
  combina esa información con el puntaje acústico, vigila el estado del sistema
  (fail-safe) y envía las alertas a la PC por **Ethernet (LwIP)**.
- Una **pantalla táctil BTT TFT35 B1 V3** muestra el estado (nivel de alerta,
  ángulo DOA, corriente, vibración) y permite **ajustar los umbrales de las
  alarmas** sin usar la PC.
- Todo va montado en una **PCB propia diseñada en KiCad** (carpeta
  `hardware/`): un *shield* (placa portadora) sobre el que se monta la
  NUCLEO-H745ZI-Q; la H745 no va soldada en la PCB. El motor es una máquina
  aparte, con su propia fuente de 12 V, su driver (puente H) y su encoder; la
  PCB solo comparte esa línea de 12 V para medir la corriente.
- La **PC** controla un motor con un **PID de posición angular**. Para validar
  el detector, las ganancias o el código del PID se pueden cambiar **en
  caliente** y así provocar a propósito oscilaciones, vibraciones y fallas.

## 2. Roles: PC y STM32

| | PC | STM32H745ZIT6 |
|---|---|---|
| Función principal | Planta de pruebas y monitoreo | Detección de anomalías en tiempo real |
| Motor | Control PID de posición angular (con hot-swap) | — |
| Sensado | Encoder del motor (para el PID) | Micrófonos INMP441, IMU MPU-9250/6500, corriente INA3221 |
| Procesamiento | Entrenamiento del autoencoder, prototipos DSP en Python, registro de datos | DOA, beamforming, inferencia del autoencoder, fusión con la IMU y la corriente |
| Comunicación | Recibe alertas y telemetría; envía configuración | Envía alertas y telemetría por Ethernet |
| Interfaz local | — | Pantalla táctil TFT35: estado y ajuste de umbrales |

## 3. Flujo de señal

```
INMP441 x2/x4 ──I2S/SAI──► M7: DMA + buffers ──► DOA (GCC-PHAT / SRP-PHAT)
                                                   │ ángulo
                                                   ▼
                                       Beamforming (delay-and-sum)
                                                   │
                                                   ▼
                                  Features (log-mel) ─► Autoencoder
                                                   │ error de reconstrucción
                                                   ▼
                                   ── SRAM4 + HSEM (IPC) ──
                                                   │
MPU-9250/6500 ──SPI──► M4: features de vibración ──┤
INA3221 ──I2C──► M4: features de corriente ────────┤
INA3221 pin CRITICAL ──EXTI──► FALLA inmediata ────┤
                                                   ▼
                              M4: fusión ─► nivel de alerta
                                   ▲            │
       Pantalla TFT35 ◄──UART──────┘ umbrales   │ estado
       (estado + ajuste) ◄──────────────────────┤
                                                ▼
                                Ethernet (LwIP, UDP/TCP)
                                                │
                                                ▼
                        PC: monitor de alertas + registro
                                                │
                                                ▼
                        PID de posición angular ──► Motor
                        (hot-swap de ganancias/código)
                                                │
                                                └─► vibración, ruido y corriente ─► sensores
```

### Niveles de alerta

| Nivel | Significado | Acción sugerida en la PC |
|---|---|---|
| `NORMAL` | Error de reconstrucción, vibración y corriente dentro del rango normal | Ninguna |
| `ADVERTENCIA` | Se superó el umbral bajo o hay una tendencia creciente | Registrar y notificar |
| `ANOMALIA` | Se superó el umbral alto de forma sostenida | Notificar y opcionalmente reducir la referencia |
| `FALLA` | Falla grave: pico de corriente peligroso, falla del proceso o del propio detector (fail-safe) | Detener el motor |

Los umbrales se calibran con datos del motor funcionando con un PID bien
sintonizado (por ejemplo, percentiles del error de reconstrucción en operación
normal). Después se pueden ajustar desde la pantalla táctil o desde la PC; los
valores se guardan en la flash de la STM32.

## 4. Hardware e interfaces

### Micrófonos: INMP441 (MEMS, salida I2S digital)

- Salida I2S de 24 bits en una ranura de 32 bits. El pin `L/R` define si el
  micrófono transmite en el canal izquierdo o en el derecho.
- **2 micrófonos:** comparten un bloque SAI (SD común): uno con `L/R=GND`
  (izquierdo) y otro con `L/R=VDD` (derecho).
- **4 micrófonos:** dos bloques SAI (`SAI1_A` maestro y `SAI1_B` en modo
  síncrono) con el **mismo BCLK/WS**. Para el DOA es crítico que todos los
  canales se muestreen con el mismo reloj, sin desfase entre bloques.
- Frecuencia de muestreo prevista: 48 kHz (configurable a 16 kHz para el
  autoencoder).
- La separación entre micrófonos limita el retardo máximo y la ambigüedad
  espacial: `τ_max = d / c`. Con d = 10 cm, τ_max ≈ 0.29 ms ≈ 14 muestras a
  48 kHz. Para evitar aliasing espacial a la frecuencia f se requiere d < c / (2f).

### IMU: MPU-9250 / MPU-6500 (SPI)

- Giroscopio y acelerómetro de 3 ejes; el MPU-9250 añade un magnetómetro
  (AK8963).
- SPI de hasta 1 MHz para configurar los registros y de 20 MHz para leer los
  datos, lo que permite muestrear a ~1–4 kHz y capturar la vibración.
- El pin `INT` (data ready) dispara la lectura por DMA.
- Nota: el MPU-9250 está descontinuado; el MPU-6500 es compatible a nivel de
  registros para el acelerómetro y el giroscopio.

### Monitor de corriente: INA3221 (I2C)

- Tres canales de corriente y voltaje con resistencia shunt (rango del shunt:
  ±163.8 mV; voltaje de bus de 0 a 26 V). El canal 1 mide la línea de **12 V
  del motor**, que pasa por la PCB (entrada de la fuente → shunt → salida al
  motor); los canales 2 y 3 quedan para la alimentación de la placa (por
  ejemplo, 5 V y 3.3 V).
- La resistencia shunt se elige según la corriente máxima del motor:
  `R = 0.163 V / I_max` (por ejemplo, 0.05 Ω da hasta ~3.2 A). En un sistema
  de 12 V el voltaje de bus queda dentro del rango del INA3221 (26 V máx.); hay
  que dimensionar la potencia del shunt (`P = I_max² · R`, p. ej. 0.5 W para
  3.2 A con 0.05 Ω) y medir con conexión Kelvin. La corriente de arranque o de
  bloqueo del motor debe quedar por debajo de `I_max`.
- Pines de alerta por hardware (open-drain), conectados a EXTI del M4:
  - **`CRITICAL`**: se compara en cada conversión con un límite por canal, así
    que detecta **picos de corriente** sin esperar a que el software lea el
    I2C. Va directo a `FALLA`.
  - **`WARNING`**: compara contra el promedio y sirve para detectar una
    sobrecorriente sostenida (`ADVERTENCIA`).
- El M4 lee periódicamente las corrientes por I2C para calcular features
  (RMS, picos, rizado). Así, cuando el PID hace oscilar el motor, se ven los
  picos de corriente correlacionados con la vibración.

### Pantalla: BTT TFT35 B1 V3 (BIGTREETECH)

Pantalla táctil de 3.5" reutilizada de una impresora 3D. Tiene **su propio
microcontrolador**, así que se le carga un **firmware propio** (carpeta
`display/`, PlatformIO) y se conecta a la STM32H745 por UART.

| Recurso | Detalle |
|---|---|
| MCU | STM32F207VCT6 (Cortex-M3, 120 MHz, 256 KB de flash, 128 KB de RAM); hay lotes con GD32F205 |
| Reloj | HSE de 8 MHz |
| LCD | 480×320, controlador ILI9488 / ST7796S / NT35310 (se detecta al arrancar), bus **FSMC de 16 bits** (NE1 = CS, A23 = RS, base `0x60FFFFFE`) |
| Táctil | XPT2046 por SPI por software: SCK PE5, MOSI PE3, MISO PE4, CS PE6, PEN PC13 |
| Retroiluminación | PD12 (TIM4_CH1, PWM) |
| Buzzer | PD13 |
| UART hacia la STM32H745 | USART2 (conector RS232/TFT: TX PA2, RX PA3, 5 V, GND) |
| Depuración | Conector SWD (3.3V, DIO, GND, CLK, RST) para ST-Link |
| Conectores EXP1/EXP2 | Se usan solo en el modo de impresora original; el pinout tentativo está en [docs/display.md](docs/display.md) |
| Memoria | Bootloader de BTT en `0x08000000`; la aplicación arranca en **`0x08008000`** (se actualiza desde la microSD) |

Protocolo UART propuesto (tramas binarias con encabezado, longitud y CRC):

- STM32H745 → pantalla: nivel de alerta, ángulo DOA, puntajes, corriente y
  vibración.
- Pantalla → STM32H745: nuevos umbrales, modo (2 o 4 micrófonos, ángulo de
  enfoque) y reconocimiento de alarmas.

### Ethernet (LwIP)

- Usa el MAC interno del H745 con el PHY LAN8742 de la NUCLEO-H745ZI-Q (RMII).
- Pila LwIP en el M4. Los descriptores DMA y los buffers de ETH van en la
  SRAM del dominio D2, con la MPU configurada (no cacheable) para evitar
  problemas de coherencia.
- Protocolo propuesto:
  - **UDP** STM32 → PC: alertas y telemetría (nivel, puntaje acústico, puntaje
    de vibración, ángulo DOA, marca de tiempo).
  - **TCP** PC → STM32: configuración (umbrales, número de micrófonos, ángulo
    de enfoque elegido por el usuario, modo de captura de datos crudos).
- CAN queda como opción a futuro; no es el bus principal.

## 5. Reparto de tareas M7 / M4

| Núcleo | Tareas |
|---|---|
| **Cortex-M7** (480 MHz, FPU DP, caché) | Captura I2S/SAI por DMA (doble buffer); DOA: GCC-PHAT con 2 micrófonos y SRP-PHAT con 4 (barrido de ángulos o ángulo elegido por el usuario); beamforming delay-and-sum; extracción de features (FFT/log-mel con CMSIS-DSP); inferencia del autoencoder; cálculo del error de reconstrucción |
| **Cortex-M4** (240 MHz) | Lectura de la IMU por SPI y DMA; lectura del INA3221 por I2C y atención a sus pines `CRITICAL`/`WARNING`; features de vibración y de corriente; fusión de los puntajes y máquina de estados de los niveles de alerta; fail-safe (watchdog del M7, verificación de sensores); pila Ethernet/LwIP con alertas y configuración; UART hacia la pantalla TFT35 y guardado de umbrales en flash |
| **IPC** | Memoria compartida en **SRAM4** (estructuras con número de secuencia) + **HSEM** para exclusión mutua y notificación entre núcleos |

### Fail-safe

- El M7 publica un *heartbeat* en SRAM4; si deja de actualizarse, el M4 emite
  `FALLA` (detector caído).
- Sensores ausentes o saturados (I2S sin datos, IMU o INA3221 que no
  responden) → `FALLA`.
- La línea `CRITICAL` del INA3221 genera una interrupción inmediata, aunque el
  software de fusión esté ocupado → `FALLA`.
- Si se pierde el enlace Ethernet, el M4 guarda las últimas alertas y las
  reenvía cuando se recupera la conexión.

## 6. Lazo de pruebas: PID con hot-swap

El objetivo es **provocar fallas de forma controlada** para comprobar que el
detector las identifica:

1. La PC ejecuta el control PID de posición angular del motor (lectura del
   encoder → PID → driver PWM).
2. Las ganancias `Kp, Ki, Kd` y la propia ley de control se pueden cambiar
   **sin detener el lazo**:
   - cambio de parámetros en vivo (archivo de configuración vigilado o comandos
     por socket);
   - recarga del módulo del controlador (por ejemplo, `importlib.reload` en
     Python) manteniendo el estado del integrador.
3. Escenarios de prueba:
   - PID bien sintonizado → datos **normales** (entrenamiento y referencia).
   - `Kp` alta o `Kd` baja → oscilación y sobreimpulso → vibración.
   - `Ki` excesiva → *windup* y oscilación lenta.
   - Ruido inyectado en la referencia o saturación → golpeteo mecánico.
4. Cada cambio queda registrado con una marca de tiempo, junto con las alertas
   recibidas de la STM32 y la corriente medida por el INA3221. Así se pueden medir la latencia de detección, los
   falsos positivos y los falsos negativos.

## 7. Plan por fases

| Fase | Contenido | Entregable |
|---|---|---|
| 0 | **Pantalla TFT35 B1**: firmware propio con PlatformIO (reloj, LCD por FSMC, táctil XPT2046, UART), interfaz de estado y ajuste de umbrales, protocolo UART con un simulador en la PC | Pantalla funcional, controlada desde la PC como si fuera la STM32H745 |
| 1 | Esqueleto de la STM32H745: proyecto CubeMX doble núcleo (M7 + M4), estructura del repo, relojes, UART de depuración, IPC básico HSEM + SRAM4 | Ambos núcleos arrancan e intercambian un contador |
| 2 | Captura de audio: INMP441 por SAI + DMA (2 micrófonos); envío de audio crudo a la PC (UART/USB o Ethernet) | Grabaciones WAV estéreo desde la STM32 |
| 3 | Ethernet: LwIP en el M4, UDP de telemetría y TCP de configuración; programa receptor en la PC | La PC recibe paquetes de prueba |
| 4 | Banco de pruebas en la PC: control PID del motor con hot-swap y registro sincronizado | Datasets normales y con fallas inducidas |
| 5 | Prototipo en la PC (Python): GCC-PHAT, beamforming, features y entrenamiento del autoencoder | Modelo entrenado y umbrales calibrados |
| 6 | DSP embebido en el M7: GCC-PHAT y beamforming con CMSIS-DSP, validados contra el prototipo | Ángulo DOA en tiempo real |
| 7 | Autoencoder en el M7 (X-CUBE-AI / TFLite Micro / CMSIS-NN) | Error de reconstrucción en tiempo real |
| 8 | IMU por SPI e INA3221 por I2C en el M4 (con pines de alerta por hardware); fusión, niveles de alerta y fail-safe; enlace UART con la pantalla y umbrales guardados en flash | Alertas `NORMAL/ADVERTENCIA/ANOMALIA/FALLA` en la PC y en la pantalla |
| 9 | Validación de punta a punta con fallas inducidas por el PID | Métricas de detección |
| 10 | Escalamiento a 4 micrófonos (2 bloques SAI síncronos, SRP-PHAT, ángulo elegido por el usuario) | DOA con 4 micrófonos |

### PCB en KiCad (en paralelo)

Decisión abierta: el arreglo de micrófonos puede ir en una **placa aparte**
(recomendado) para fijar con precisión su separación y alejarlo del ruido del
motor, conectado al *shield* con un cable corto para I2S.

La PCB **no es opcional**: es parte del proyecto final. El diseño en KiCad
avanza en paralelo con el firmware, y cada versión se basa en lo que se valida
antes en protoboard:

| Etapa | Contenido |
|---|---|
| A | Esquemático del *shield* para la NUCLEO-H745ZI-Q (conectores ST Zio/morpho): entrada de 12 V de la fuente del motor y salida al motor pasando por el shunt del INA3221, regulador 12 V → 5 V, INMP441, MPU-9250/6500, INA3221, conector de la pantalla TFT35 (UART + 5 V). Ethernet usa el RJ45 de la Nucleo. El driver y el encoder del motor **no** van en esta PCB |
| B | Revisión del esquemático contra el pinout validado (CubeMX) y reglas eléctricas (ERC) |
| C | Layout: geometría del arreglo de micrófonos (separación d, simetría), pistas anchas para los 12 V del motor, separadas de la tierra analógica, shunt con conexión Kelvin |
| D | DRC, archivos de fabricación (Gerber, BOM, posiciones) y pruebas de la placa armada |

## 8. Estructura prevista del repositorio

```
firmware/        Proyecto STM32CubeIDE/CMake (CM7/, CM4/, Common/ para IPC)
pc/              Herramientas en Python
  pid_bench/     Control PID del motor con hot-swap
  monitor/       Receptor de alertas/telemetría por Ethernet
  dsp_proto/     Prototipos de DOA, beamforming y features
  model/         Entrenamiento y exportación del autoencoder
data/            Datasets (fuera de git o con Git LFS)
display/         Firmware propio de la pantalla BTT TFT35 (PlatformIO)
docs/            Diagramas, pinout, notas de diseño
hardware/        Proyecto KiCad de la PCB (esquemático, layout, librerías, fabricación)
```
