/*
 * Puente USB <-> UART con una ESP32, para conectar la PC a la pantalla
 * BTT TFT35 B1 (o más adelante a la STM32H745) sin un adaptador USB-UART.
 *
 * Conexiones (niveles de 3.3 V, compatibles con el STM32F207):
 *   ESP32 GPIO17 (TX2) -> RX2 (PA3) de la pantalla
 *   ESP32 GPIO16 (RX2) <- TX2 (PA2) de la pantalla
 *   ESP32 GND          -- GND de la pantalla
 *   ESP32 VIN / 5V     -> +5V de la pantalla (alimentación desde el USB)
 *
 * Cargar con el IDE de Arduino (placa "ESP32 Dev Module") y después usar
 * el puerto de la ESP32 con pc/display_sim/sim.py.
 */

#define BRIDGE_BAUD 115200
#define PIN_RX2     16
#define PIN_TX2     17
#define LED_PIN     2     /* LED integrado en la mayoría de las DevKit */

static uint32_t led_off_ms;

void setup()
{
    Serial.begin(BRIDGE_BAUD);
    Serial2.begin(BRIDGE_BAUD, SERIAL_8N1, PIN_RX2, PIN_TX2);
    pinMode(LED_PIN, OUTPUT);
}

static void blink()
{
    digitalWrite(LED_PIN, HIGH);
    led_off_ms = millis() + 20;
}

void loop()
{
    uint8_t buf[128];
    size_t n;

    n = Serial.available();
    if (n) {
        n = Serial.readBytes(buf, n > sizeof(buf) ? sizeof(buf) : n);
        Serial2.write(buf, n);
        blink();
    }

    n = Serial2.available();
    if (n) {
        n = Serial2.readBytes(buf, n > sizeof(buf) ? sizeof(buf) : n);
        Serial.write(buf, n);
        blink();
    }

    if (led_off_ms && (int32_t)(millis() - led_off_ms) >= 0) {
        digitalWrite(LED_PIN, LOW);
        led_off_ms = 0;
    }
}
