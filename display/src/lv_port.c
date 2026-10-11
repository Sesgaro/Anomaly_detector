#include "lv_port.h"
#include "lcd.h"
#include "touch.h"
#include "calib.h"
#include "board.h"
#include "lvgl.h"

#define BUF_LINES 24

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[LCD_WIDTH * BUF_LINES];
static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px)
{
    uint32_t n = (uint32_t)lv_area_get_width(area) * (uint32_t)lv_area_get_height(area);

    lcd_set_window((uint16_t)area->x1, (uint16_t)area->y1,
                   (uint16_t)area->x2, (uint16_t)area->y2);
    lcd_write_pixels((const uint16_t *)px, n);
    lv_disp_flush_ready(drv);
}

static void touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    static int16_t last_x, last_y;
    int16_t x, y;

    (void)drv;
    if (!calib_active() && touch_read(&x, &y)) {
        last_x = x;
        last_y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
    data->point.x = last_x;
    data->point.y = last_y;
}

void lv_port_init(void)
{
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, LCD_WIDTH * BUF_LINES);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_WIDTH;
    disp_drv.ver_res = LCD_HEIGHT;
    disp_drv.flush_cb = flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_cb;
    lv_indev_drv_register(&indev_drv);
}
