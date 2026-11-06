#include "demo.h"
#include "display_link.h"
#include <math.h>
#include <string.h>

#define CYCLE_MS  40000u
#define PERIOD_MS 100u

static dl_thresholds_t thr = {
    .audio_warn_m = 800, .audio_anom_m = 1500,
    .vib_warn_mg = 300, .vib_anom_mg = 700,
    .current_warn_ma = 1500, .current_crit_ma = 2500,
};
static dl_config_t cfg = { .n_mics = 2, .focus_ddeg = 900, .aperture_ddeg = 300 };
static uint32_t last_ms;
static bool     real_valid;
static int16_t  real_ma[3];
static uint16_t real_bus_mv;

void demo_set_power(bool valid, const int16_t ma[3], uint16_t bus_mv)
{
    real_valid = valid;
    if (valid) {
        for (int i = 0; i < 3; i++)
            real_ma[i] = ma[i];
        real_bus_mv = bus_mv;
    }
}

void demo_init(void)
{
    last_ms = 0;
}

static float severity(uint32_t now)
{
    float ph = (float)(now % CYCLE_MS) / CYCLE_MS;
    if (ph < 0.40f) return 0.0f;
    if (ph < 0.70f) return (ph - 0.40f) / 0.30f;
    if (ph < 0.85f) return 1.3f;
    return 1.3f * (1.0f - (ph - 0.85f) / 0.15f);
}

static uint8_t level_for(int audio, int vib, int current)
{
    if (current >= thr.current_crit_ma)
        return DL_LEVEL_FALLA;
    if (audio >= thr.audio_anom_m || vib >= thr.vib_anom_mg)
        return DL_LEVEL_ANOMALIA;
    if (audio >= thr.audio_warn_m || vib >= thr.vib_warn_mg || current >= thr.current_warn_ma)
        return DL_LEVEL_ADVERTENCIA;
    return DL_LEVEL_NORMAL;
}

void demo_poll(uint32_t now)
{
    if (now - last_ms < PERIOD_MS)
        return;
    last_ms = now;

    float t = now / 1000.0f;
    float k = severity(now);
    int audio = (int)(400 + k * 1400 + 60 * sinf(t * 7.3f));
    int vib = (int)(120 + k * 650 + 40 * sinf(t * 3.1f));
    int current = real_valid ? real_ma[DL_MOTOR_CH]
                             : (int)(900 + k * 1500 + 300 * k * fabsf(sinf(t * 9.0f)));
    /* La fuente simulada se mueve alrededor del punto de enfoque, dentro del
     * arco de apertura */
    int doa = cfg.focus_ddeg + (int)(cfg.aperture_ddeg * 0.4f * sinf(t * 0.4f));
    if (cfg.n_mics == 4)
        doa = ((doa % 3600) + 3600) % 3600;               /* 0–359.9° */
    else
        doa = doa < 0 ? 0 : (doa > 1800 ? 1800 : doa);    /* 0–180° */

    dl_status_t st;
    memset(&st, 0, sizeof(st));
    st.level = level_for(audio, vib, current);
    st.flags = DL_FLAG_DETECTOR_OK | DL_FLAG_DOA_VALID;
    if (cfg.n_mics == 4)
        st.flags |= DL_FLAG_FOUR_MICS;
    if (current >= thr.current_crit_ma)
        st.flags |= DL_FLAG_INA_CRITICAL;
    st.doa_ddeg = (int16_t)doa;
    st.audio_score_m = (uint16_t)(audio < 0 ? 0 : audio);
    st.vib_rms_mg = (uint16_t)(vib < 0 ? 0 : vib);
    st.current_ma[0] = real_valid ? real_ma[0] : 120;
    st.current_ma[1] = real_valid ? real_ma[1] : 80;
    st.current_ma[DL_MOTOR_CH] = (int16_t)current;
    st.motor_bus_mv = real_valid ? real_bus_mv : 12000;
    st.uptime_ms = now;
    link_send(DL_MSG_STATUS, &st, sizeof(st));
}

void demo_on_frame(uint8_t type, const uint8_t *p, uint8_t len)
{
    switch (type) {
    case DL_MSG_REQ_PARAMS:
        link_send(DL_MSG_THRESHOLDS, &thr, sizeof(thr));
        link_send(DL_MSG_CONFIG, &cfg, sizeof(cfg));
        break;
    case DL_MSG_THRESHOLDS:
        if (len == sizeof(thr)) {
            memcpy(&thr, p, sizeof(thr));
            link_send(DL_MSG_THRESHOLDS, &thr, sizeof(thr));   /* confirmación */
        }
        break;
    case DL_MSG_CONFIG:
        if (len == sizeof(cfg)) {
            memcpy(&cfg, p, sizeof(cfg));
            link_send(DL_MSG_CONFIG, &cfg, sizeof(cfg));
        }
        break;
    default:
        break;
    }
}
