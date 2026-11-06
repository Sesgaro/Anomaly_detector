#include "ina3221.h"

/* Registros del INA3221 */
#define REG_CONFIG     0x00
#define REG_SHUNT(ch)  (uint8_t)(0x01 + 2 * (ch))
#define REG_BUS(ch)    (uint8_t)(0x02 + 2 * (ch))
#define REG_MANUF_ID   0xFE
#define REG_DIE_ID     0xFF

static I2C_HandleTypeDef hi2c1;
static uint16_t addr7;

static bool rd(uint8_t reg, uint16_t *v)
{
    uint8_t b[2];
    if (HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(addr7 << 1), reg, 1, b, 2, 10) != HAL_OK)
        return false;
    *v = (uint16_t)((b[0] << 8) | b[1]);
    return true;
}

static bool wr(uint8_t reg, uint16_t v)
{
    uint8_t b[2] = { (uint8_t)(v >> 8), (uint8_t)v };
    return HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(addr7 << 1), reg, 1, b, 2, 10) == HAL_OK;
}

uint16_t ina3221_init(uint16_t *manuf, uint16_t *die)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();

    g.Pin = GPIO_PIN_8 | GPIO_PIN_9;           /* PB8 SCL, PB9 SDA (CN7-2, CN7-4) */
    g.Mode = GPIO_MODE_AF_OD;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &g);

    g.Pin = GPIO_PIN_11 | GPIO_PIN_14;         /* PE11 CRITICAL, PE14 WARNING */
    g.Mode = GPIO_MODE_INPUT;
    g.Alternate = 0;
    HAL_GPIO_Init(GPIOE, &g);

    hi2c1.Instance = I2C1;
    hi2c1.Init.Timing = 0x10707DBC;            /* 100 kHz con reloj de 64 MHz (HSI) */
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
        return 0;

    /* A0 a GND/VS/SDA/SCL da 0x40–0x43 */
    for (uint16_t a = 0x40; a <= 0x43; a++) {
        if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(a << 1), 2, 5) == HAL_OK) {
            addr7 = a;
            if (!rd(REG_MANUF_ID, manuf) || !rd(REG_DIE_ID, die))
                return 0;
            /* 3 canales, promedio de 16, conversiones de 1.1 ms, continuo */
            wr(REG_CONFIG, 0x7527);
            return addr7;
        }
    }
    return 0;
}

bool ina3221_read(int ch, int32_t *bus_mv, int32_t *shunt_uv)
{
    uint16_t s, b;
    if (!addr7 || !rd(REG_SHUNT(ch), &s) || !rd(REG_BUS(ch), &b))
        return false;
    *shunt_uv = ((int16_t)s >> 3) * 40;        /* LSB de 40 µV */
    *bus_mv = ((int16_t)b >> 3) * 8;           /* LSB de 8 mV */
    return true;
}

bool ina3221_critical(void)
{
    return HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_11) == GPIO_PIN_RESET;   /* activo en bajo */
}

bool ina3221_warning(void)
{
    return HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_14) == GPIO_PIN_RESET;
}
