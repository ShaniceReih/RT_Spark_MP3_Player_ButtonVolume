#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include <cstdint>

/* =========================================================
   Colors
   ========================================================= */

#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_WHITE   0xFFFF
#define COLOR_BLACK   0x0000
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF

// Existing hardware driver; callers must supply nonzero, in-bounds regions.
void LCD_Init(void);
void LCD_Fill(uint16_t color);
void LCD_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                  uint16_t color);
void LCD_DrawText(uint16_t x, uint16_t y, const char *text, uint16_t color,
                  uint8_t scale);

#endif
