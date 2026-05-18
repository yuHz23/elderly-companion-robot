#pragma once

/*
 * SSD1306 128×64 OLED — I2C 0x3C on the shared i2c_bus.
 *
 * Minimal text-only driver. No graphics primitives — we just need to
 * render the status overlay built by task_oled. Uses a built-in 6×8
 * monospace font (128 ASCII glyphs); enough for the strings in the
 * status display.
 *
 * Coordinate system:
 *   col: 0..127  (pixel column)
 *   page: 0..7   (each page is 8 vertically-stacked pixels — SSD1306
 *                  GDDRAM is page-addressed natively)
 *
 * Text rendering writes one full row of 8 pixels at a time → no need
 * to readback existing pixels, just overwrite the page.
 */

#include <stdbool.h>
#include <stdint.h>

#define SSD1306_W 128
#define SSD1306_H 64
#define SSD1306_PAGES (SSD1306_H / 8)

bool ssd1306_init(void);
void ssd1306_clear(void);
void ssd1306_set_pos(uint8_t col, uint8_t page);
void ssd1306_write_text(const char *s);
void ssd1306_printf_line(uint8_t page, const char *fmt, ...);  // shorthand
