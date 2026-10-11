#include "settings.h"
#include "w25q.h"
#include "display_link.h"
#include <string.h>

#define SETTINGS_MAGIC   0x41444554u   /* "TEDA" */
#define SETTINGS_VERSION 1u

typedef struct {
    uint32_t   magic;
    uint16_t   version;
    uint16_t   crc;
    settings_t data;
} stored_t;

static settings_t current = {
    .brightness = 80,
    .cal_valid = 0,
};
static bool flash_ok;

static uint32_t settings_addr(void)
{
    return w25q_size() - 4096u;   /* último sector de 4 KB */
}

static uint16_t crc_of(const settings_t *s)
{
    return dl_crc16(0xFFFFu, (const uint8_t *)s, sizeof(*s));
}

bool settings_load(void)
{
    stored_t st;

    flash_ok = w25q_init();
    if (!flash_ok)
        return false;

    w25q_read(settings_addr(), &st, sizeof(st));
    if (st.magic != SETTINGS_MAGIC || st.version != SETTINGS_VERSION ||
        st.crc != crc_of(&st.data))
        return false;

    current = st.data;
    if (current.brightness < 10 || current.brightness > 100)
        current.brightness = 80;
    return true;
}

bool settings_save(void)
{
    stored_t st;

    if (!flash_ok)
        return false;

    memset(&st, 0, sizeof(st));
    st.magic = SETTINGS_MAGIC;
    st.version = SETTINGS_VERSION;
    st.data = current;
    st.crc = crc_of(&st.data);

    w25q_erase_sector(settings_addr());
    w25q_write(settings_addr(), &st, sizeof(st));
    return true;
}

settings_t *settings(void)
{
    return &current;
}
