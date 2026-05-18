#pragma once

/*
 * task_oled — render robot status to the 128×64 SSD1306 every 500ms.
 *
 * 8 page rows × ~20 chars = enough room for everything we want to
 * surface without a phone in hand. Layout in architecture.md §7.
 */

void task_oled_start(void);
