#ifndef LCD_H
#define LCD_H

#include <stdint.h>

enum {
    LCD_DRV_UNKNOWN = 0,
    LCD_DRV_ILI9488,
    LCD_DRV_ST7796S,
};

/* Inicializa el FSMC y el controlador del LCD (480x320, horizontal, RGB565).
 * Devuelve el ID leído con el comando 0xD3 (p. ej. 0x9488 o 0x7796). */
uint16_t lcd_init(void);
int lcd_driver(void);

void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void lcd_write_pixels(const uint16_t *px, uint32_t count);
void lcd_fill_rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);

#define LCD_RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

#endif /* LCD_H */
