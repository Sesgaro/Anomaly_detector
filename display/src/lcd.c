#include "lcd.h"
#include "board.h"

typedef struct {
    volatile uint16_t REG;
    volatile uint16_t RAM;
} lcd_bus_t;

#define LCD ((lcd_bus_t *)LCD_BASE_ADDR)

/* MADCTL para horizontal (480x320), igual en ILI9488 y ST7796S.
 * Si la imagen sale girada 180°, usar 0xE8. */
#define LCD_MADCTL_LANDSCAPE 0x28

static SRAM_HandleTypeDef hsram;
static int driver = LCD_DRV_UNKNOWN;

static inline void wr_cmd(uint16_t c) { LCD->REG = c; }
static inline void wr_data(uint16_t d) { LCD->RAM = d; }
static inline uint16_t rd_data(void) { return LCD->RAM; }

static void cmd_seq(uint8_t cmd, const uint8_t *data, uint8_t n)
{
    wr_cmd(cmd);
    for (uint8_t i = 0; i < n; i++)
        wr_data(data[i]);
}

#define CMD(c, ...) do {                                   \
        static const uint8_t d_[] = { __VA_ARGS__ };       \
        cmd_seq((c), d_, sizeof(d_));                      \
    } while (0)

static void fsmc_gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_FSMC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF12_FSMC;

    /* D0 PD14, D1 PD15, D2 PD0, D3 PD1, D13 PD8, D14 PD9, D15 PD10,
     * NOE PD4, NWE PD5, NE1 PD7 */
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_7 |
               GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOD, &gpio);

    /* A23 (RS) PE2, D4–D12 PE7–PE15 */
    gpio.Pin = GPIO_PIN_2 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
               GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOE, &gpio);
}

static void fsmc_init(void)
{
    FSMC_NORSRAM_TimingTypeDef rd = {0};
    FSMC_NORSRAM_TimingTypeDef wr = {0};

    hsram.Instance = FSMC_NORSRAM_DEVICE;
    hsram.Extended = FSMC_NORSRAM_EXTENDED_DEVICE;
    hsram.Init.NSBank = FSMC_NORSRAM_BANK1;
    hsram.Init.DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE;
    hsram.Init.MemoryType = FSMC_MEMORY_TYPE_SRAM;
    hsram.Init.MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_16;
    hsram.Init.BurstAccessMode = FSMC_BURST_ACCESS_MODE_DISABLE;
    hsram.Init.WaitSignalPolarity = FSMC_WAIT_SIGNAL_POLARITY_LOW;
    hsram.Init.WrapMode = FSMC_WRAP_MODE_DISABLE;
    hsram.Init.WaitSignalActive = FSMC_WAIT_TIMING_BEFORE_WS;
    hsram.Init.WriteOperation = FSMC_WRITE_OPERATION_ENABLE;
    hsram.Init.WaitSignal = FSMC_WAIT_SIGNAL_DISABLE;
    hsram.Init.ExtendedMode = FSMC_EXTENDED_MODE_ENABLE;
    hsram.Init.AsynchronousWait = FSMC_ASYNCHRONOUS_WAIT_DISABLE;
    hsram.Init.WriteBurst = FSMC_WRITE_BURST_DISABLE;

    /* Lectura lenta (ID del controlador): ~500 ns por acceso a 120 MHz */
    rd.AddressSetupTime = 2;
    rd.AddressHoldTime = 1;
    rd.DataSetupTime = 60;
    rd.BusTurnAroundDuration = 0;
    rd.CLKDivision = 2;
    rd.DataLatency = 2;
    rd.AccessMode = FSMC_ACCESS_MODE_A;

    /* Escritura: ~40 ns por píxel */
    wr.AddressSetupTime = 1;
    wr.AddressHoldTime = 1;
    wr.DataSetupTime = 3;
    wr.BusTurnAroundDuration = 0;
    wr.CLKDivision = 2;
    wr.DataLatency = 2;
    wr.AccessMode = FSMC_ACCESS_MODE_A;

    HAL_SRAM_Init(&hsram, &rd, &wr);
}

static uint16_t read_id(void)
{
    wr_cmd(0xD3);
    (void)rd_data();               /* lectura falsa */
    (void)rd_data();               /* 0x00 */
    uint16_t id = (uint16_t)((rd_data() & 0xFF) << 8);
    id |= rd_data() & 0xFF;
    return id;
}

static void init_ili9488(void)
{
    CMD(0xE0, 0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78,
              0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F);  /* gamma + */
    CMD(0xE1, 0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45,
              0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F);  /* gamma - */
    CMD(0xC0, 0x17, 0x15);         /* Power Control 1 */
    CMD(0xC1, 0x41);               /* Power Control 2 */
    CMD(0xC5, 0x00, 0x12, 0x80);   /* VCOM */
    CMD(0x3A, 0x55);               /* 16 bits/píxel */
    CMD(0xB0, 0x00);               /* Interface Mode Control */
    CMD(0xB1, 0xA0);               /* Frame rate 60 Hz */
    CMD(0xB4, 0x02);               /* Inversión de 2 puntos */
    CMD(0xB6, 0x02, 0x02, 0x3B);   /* Display Function Control */
    CMD(0xE9, 0x00);               /* Set Image Function */
    CMD(0xF7, 0xA9, 0x51, 0x2C, 0x82); /* Adjust Control 3 */
}

static void init_st7796s(void)
{
    CMD(0xF0, 0xC3);               /* habilitar comandos extendidos */
    CMD(0xF0, 0x96);
    CMD(0x3A, 0x55);               /* 16 bits/píxel */
    CMD(0xB4, 0x01);               /* inversión de 1 punto */
    CMD(0xB6, 0x80, 0x02, 0x3B);   /* Display Function Control */
    CMD(0xE8, 0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33);
    CMD(0xC1, 0x06);
    CMD(0xC2, 0xA7);
    CMD(0xC5, 0x18);               /* VCOM */
    CMD(0xE0, 0xF0, 0x09, 0x0B, 0x06, 0x04, 0x15, 0x2F,
              0x54, 0x42, 0x3C, 0x17, 0x14, 0x18, 0x1B);  /* gamma + */
    CMD(0xE1, 0xE0, 0x09, 0x0B, 0x06, 0x04, 0x03, 0x2B,
              0x43, 0x42, 0x3B, 0x16, 0x14, 0x17, 0x1B);  /* gamma - */
    CMD(0xF0, 0x3C);               /* deshabilitar comandos extendidos */
    CMD(0xF0, 0x69);
}

uint16_t lcd_init(void)
{
    fsmc_gpio_init();
    fsmc_init();

    wr_cmd(0x01);                  /* reset por software */
    HAL_Delay(120);

    uint16_t id = read_id();
    if (id == 0x7796) {
        driver = LCD_DRV_ST7796S;
        init_st7796s();
    } else {
        /* 0x9488 o desconocido: la secuencia del ILI9488 es la más común */
        driver = (id == 0x9488) ? LCD_DRV_ILI9488 : LCD_DRV_UNKNOWN;
        init_ili9488();
    }

    CMD(0x36, LCD_MADCTL_LANDSCAPE);
    wr_cmd(0x11);                  /* salir de sleep */
    HAL_Delay(120);
    wr_cmd(0x29);                  /* encender pantalla */

    lcd_fill_rect(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, 0x0000);
    return id;
}

int lcd_driver(void)
{
    return driver;
}

void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    wr_cmd(0x2A);
    wr_data(x0 >> 8);
    wr_data(x0 & 0xFF);
    wr_data(x1 >> 8);
    wr_data(x1 & 0xFF);
    wr_cmd(0x2B);
    wr_data(y0 >> 8);
    wr_data(y0 & 0xFF);
    wr_data(y1 >> 8);
    wr_data(y1 & 0xFF);
    wr_cmd(0x2C);
}

void lcd_write_pixels(const uint16_t *px, uint32_t count)
{
    while (count--)
        LCD->RAM = *px++;
}

void lcd_fill_rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    uint32_t n = (uint32_t)(x1 - x0 + 1) * (uint32_t)(y1 - y0 + 1);

    lcd_set_window(x0, y0, x1, y1);
    while (n--)
        LCD->RAM = color;
}
