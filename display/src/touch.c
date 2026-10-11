#include "touch.h"
#include "board.h"

#define XPT_CMD_X  0xD0   /* canal X, 12 bits, diferencial, PENIRQ activo */
#define XPT_CMD_Y  0x90   /* canal Y */
#define SAMPLES    7
#define MAX_SPREAD 60     /* diferencia máxima aceptada entre muestras */

static touch_cal_t cal = {
    /* Valores por defecto aproximados hasta calibrar */
    .a = 480.0f / 3800.0f, .b = 0.0f, .c = -480.0f * 150.0f / 3800.0f,
    .d = 0.0f, .e = 320.0f / 3800.0f, .f = -320.0f * 150.0f / 3800.0f,
};

static inline void spi_delay(void)
{
    for (volatile int i = 0; i < 20; i++) {}
}

#define PIN_HIGH(p) HAL_GPIO_WritePin(p##_PORT, p##_PIN, GPIO_PIN_SET)
#define PIN_LOW(p)  HAL_GPIO_WritePin(p##_PORT, p##_PIN, GPIO_PIN_RESET)

static bool pen_down(void)
{
    return HAL_GPIO_ReadPin(TP_PEN_PORT, TP_PEN_PIN) == GPIO_PIN_RESET;
}

void touch_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
    gpio.Pin = TP_SCK_PIN | TP_MOSI_PIN | TP_CS_PIN;
    HAL_GPIO_Init(GPIOE, &gpio);

    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Pin = TP_MISO_PIN;
    HAL_GPIO_Init(TP_MISO_PORT, &gpio);
    gpio.Pin = TP_PEN_PIN;
    HAL_GPIO_Init(TP_PEN_PORT, &gpio);

    PIN_HIGH(TP_CS);
    PIN_LOW(TP_SCK);
}

static uint16_t xpt_read(uint8_t cmd)
{
    uint16_t v = 0;

    PIN_LOW(TP_CS);
    for (int i = 7; i >= 0; i--) {
        if (cmd & (1u << i))
            PIN_HIGH(TP_MOSI);
        else
            PIN_LOW(TP_MOSI);
        spi_delay();
        PIN_HIGH(TP_SCK);
        spi_delay();
        PIN_LOW(TP_SCK);
    }
    PIN_LOW(TP_MOSI);
    spi_delay();

    /* 1 ciclo de BUSY + 12 bits de dato + 3 de relleno */
    for (int i = 0; i < 16; i++) {
        PIN_HIGH(TP_SCK);
        spi_delay();
        v = (uint16_t)(v << 1);
        if (HAL_GPIO_ReadPin(TP_MISO_PORT, TP_MISO_PIN) == GPIO_PIN_SET)
            v |= 1u;
        PIN_LOW(TP_SCK);
        spi_delay();
    }
    PIN_HIGH(TP_CS);

    return (uint16_t)((v >> 3) & 0x0FFFu);
}

/* Promedio de las muestras centrales; false si son muy dispersas */
static bool xpt_filtered(uint8_t cmd, uint16_t *out)
{
    uint16_t s[SAMPLES];

    for (int i = 0; i < SAMPLES; i++)
        s[i] = xpt_read(cmd);

    for (int i = 1; i < SAMPLES; i++) {
        uint16_t k = s[i];
        int j = i - 1;
        while (j >= 0 && s[j] > k) {
            s[j + 1] = s[j];
            j--;
        }
        s[j + 1] = k;
    }

    if (s[SAMPLES - 2] - s[1] > MAX_SPREAD)
        return false;

    *out = (uint16_t)((s[2] + s[3] + s[4]) / 3u);
    return true;
}

bool touch_read_raw(uint16_t *rx, uint16_t *ry)
{
    if (!pen_down())
        return false;

    uint16_t x, y;
    if (!xpt_filtered(XPT_CMD_X, &x) || !xpt_filtered(XPT_CMD_Y, &y))
        return false;

    /* Si se soltó durante la medición, la lectura no es confiable */
    if (!pen_down())
        return false;

    *rx = x;
    *ry = y;
    return true;
}

bool touch_read(int16_t *x, int16_t *y)
{
    uint16_t rx, ry;

    if (!touch_read_raw(&rx, &ry))
        return false;

    float fx = cal.a * rx + cal.b * ry + cal.c;
    float fy = cal.d * rx + cal.e * ry + cal.f;

    if (fx < 0) fx = 0;
    if (fy < 0) fy = 0;
    if (fx > LCD_WIDTH - 1) fx = LCD_WIDTH - 1;
    if (fy > LCD_HEIGHT - 1) fy = LCD_HEIGHT - 1;

    *x = (int16_t)fx;
    *y = (int16_t)fy;
    return true;
}

void touch_set_cal(const touch_cal_t *c)
{
    cal = *c;
}

/* Resuelve [rx ry 1] * [k1 k2 k3]^T = s para los 3 puntos (regla de Cramer) */
static bool solve3(const float m[3][3], const float s[3], float out[3])
{
    float det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
              - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
              + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);

    if (det > -1.0f && det < 1.0f)
        return false;

    for (int c = 0; c < 3; c++) {
        float t[3][3];
        for (int r = 0; r < 3; r++)
            for (int k = 0; k < 3; k++)
                t[r][k] = (k == c) ? s[r] : m[r][k];
        out[c] = (t[0][0] * (t[1][1] * t[2][2] - t[1][2] * t[2][1])
                - t[0][1] * (t[1][0] * t[2][2] - t[1][2] * t[2][0])
                + t[0][2] * (t[1][0] * t[2][1] - t[1][1] * t[2][0])) / det;
    }
    return true;
}

bool touch_compute_cal(const int16_t sx[3], const int16_t sy[3],
                       const uint16_t rx[3], const uint16_t ry[3],
                       touch_cal_t *out)
{
    float m[3][3], vx[3], vy[3], kx[3], ky[3];

    for (int i = 0; i < 3; i++) {
        m[i][0] = rx[i];
        m[i][1] = ry[i];
        m[i][2] = 1.0f;
        vx[i] = sx[i];
        vy[i] = sy[i];
    }
    if (!solve3(m, vx, kx) || !solve3(m, vy, ky))
        return false;

    out->a = kx[0]; out->b = kx[1]; out->c = kx[2];
    out->d = ky[0]; out->e = ky[1]; out->f = ky[2];
    return true;
}
