/*
 * Firmware de la pantalla BTT TFT35 B1 V3.0 para el detector de anomalías.
 * Muestra el estado que envía la STM32H745 y permite ajustar los umbrales.
 */
#include "board.h"
#include "hw.h"
#include "lcd.h"
#include "touch.h"
#include "settings.h"
#include "link.h"
#include "calib.h"
#include "lv_port.h"
#include "ui.h"
#include "fwupdate.h"
#include "lvgl.h"

int main(void)
{
    hw_init();

    uint16_t lcd_id = lcd_init();
    touch_init();

    settings_load();
    hw_backlight_set(settings()->brightness);
    if (settings()->cal_valid)
        touch_set_cal(&settings()->cal);

    link_init();

    lv_init();
    lv_port_init();
    ui_init(lcd_id);

    if (!settings()->cal_valid)
        calib_start();

    hw_buzzer_beep(60);

    for (;;) {
        link_poll();
        calib_poll();
        fwupdate_poll();
        ui_update();
        lv_timer_handler();
    }
}
