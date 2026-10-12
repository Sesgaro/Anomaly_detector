#include "w25q.h"
#include "board.h"

#define CMD_WRITE_ENABLE  0x06
#define CMD_READ_STATUS1  0x05
#define CMD_READ_DATA     0x03
#define CMD_PAGE_PROGRAM  0x02
#define CMD_SECTOR_ERASE  0x20
#define CMD_BLOCK64_ERASE 0xD8
#define CMD_JEDEC_ID      0x9F
#define PAGE_SIZE         256u

static SPI_HandleTypeDef hspi3;
static uint32_t flash_size;
static uint32_t flash_jedec;

static inline void cs_low(void)  { HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_RESET); }
static inline void cs_high(void) { HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_SET); }

static void tx(const uint8_t *d, uint16_t n)
{
    HAL_SPI_Transmit(&hspi3, (uint8_t *)d, n, 100);
}

static void rx(uint8_t *d, uint16_t n)
{
    HAL_SPI_Receive(&hspi3, d, n, 100);
}

static void send_addr_cmd(uint8_t cmd, uint32_t addr)
{
    uint8_t b[4] = { cmd, (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr };
    tx(b, 4);
}

static void wait_busy(void)
{
    uint8_t cmd = CMD_READ_STATUS1, st;
    uint32_t t0 = HAL_GetTick();

    cs_low();
    tx(&cmd, 1);
    do {
        rx(&st, 1);
    } while ((st & 0x01u) && (HAL_GetTick() - t0) < 3000u);
    cs_high();
}

static void write_enable(void)
{
    uint8_t cmd = CMD_WRITE_ENABLE;
    cs_low();
    tx(&cmd, 1);
    cs_high();
}

bool w25q_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_SPI3_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF6_SPI3;
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin = FLASH_CS_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Alternate = 0;
    HAL_GPIO_Init(FLASH_CS_PORT, &gpio);
    cs_high();

    hspi3.Instance = SPI3;
    hspi3.Init.Mode = SPI_MODE_MASTER;
    hspi3.Init.Direction = SPI_DIRECTION_2LINES;
    hspi3.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi3.Init.CLKPolarity = SPI_POLARITY_HIGH;
    hspi3.Init.CLKPhase = SPI_PHASE_2EDGE;
    hspi3.Init.NSS = SPI_NSS_SOFT;
    hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;  /* 30 MHz / 4 */
    hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    if (HAL_SPI_Init(&hspi3) != HAL_OK)
        return false;

    /* El bootloader pudo dejarla en power-down o a medio comando: liberar
     * (0xAB) y reiniciar (0x66 + 0x99) antes de leer el ID. */
    static const uint8_t wake[] = { 0xAB }, rst_en[] = { 0x66 }, rst[] = { 0x99 };
    cs_low(); tx(wake, 1); cs_high();
    HAL_Delay(1);
    cs_low(); tx(rst_en, 1); cs_high();
    cs_low(); tx(rst, 1); cs_high();
    HAL_Delay(1);

    uint8_t id[3] = {0};
    for (int attempt = 0; attempt < 3; attempt++) {
        uint8_t cmd = CMD_JEDEC_ID;
        cs_low();
        tx(&cmd, 1);
        rx(id, 3);
        cs_high();
        flash_jedec = ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | id[2];
        /* El tercer byte codifica la capacidad como 2^n bytes */
        if (id[0] != 0x00 && id[0] != 0xFF && id[2] >= 0x10 && id[2] <= 0x19) {
            flash_size = 1ul << id[2];
            return true;
        }
        HAL_Delay(2);
    }
    return false;
}

uint32_t w25q_size(void)
{
    return flash_size;
}

uint32_t w25q_jedec(void)
{
    return flash_jedec;
}

void w25q_read(uint32_t addr, void *buf, uint32_t len)
{
    uint8_t *p = buf;

    cs_low();
    send_addr_cmd(CMD_READ_DATA, addr);
    while (len) {
        uint16_t n = (len > 0xFFFFu) ? 0xFFFFu : (uint16_t)len;
        rx(p, n);
        p += n;
        len -= n;
    }
    cs_high();
}

void w25q_erase_sector(uint32_t addr)
{
    write_enable();
    cs_low();
    send_addr_cmd(CMD_SECTOR_ERASE, addr & ~0xFFFul);
    cs_high();
    wait_busy();
}

void w25q_erase_block64(uint32_t addr)
{
    write_enable();
    cs_low();
    send_addr_cmd(CMD_BLOCK64_ERASE, addr & ~0xFFFFul);
    cs_high();
    wait_busy();
}

void w25q_write(uint32_t addr, const void *buf, uint32_t len)
{
    const uint8_t *p = buf;

    while (len) {
        uint32_t room = PAGE_SIZE - (addr % PAGE_SIZE);
        uint32_t n = len < room ? len : room;

        write_enable();
        cs_low();
        send_addr_cmd(CMD_PAGE_PROGRAM, addr);
        tx(p, (uint16_t)n);
        cs_high();
        wait_busy();

        addr += n;
        p += n;
        len -= n;
    }
}
