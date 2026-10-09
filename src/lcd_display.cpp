#include "stm32f4xx_hal.h"
#include "lcd_display.h"

/* =========================================================
   RT-Spark LCD
   ST7789V3 - 240x240 - FSMC
   ========================================================= */

#define LCD_BASE 0x6803FFFEUL

typedef struct
{
    volatile uint8_t  REG;
    volatile uint8_t  RESERVED;
    volatile uint8_t  RAM8;
    volatile uint16_t RAM16;
} LCD_Controller;

#define LCD ((LCD_Controller *)LCD_BASE)

/* =========================================================
   LCD Low-Level
   ========================================================= */

static void LCD_WriteCommand(uint8_t command)
{
    LCD->REG = command;
}

static void LCD_WriteData8(uint8_t data)
{
    LCD->RAM8 = data;
}

static void LCD_WriteColor(uint16_t color)
{
    LCD->RAM16 = color;
}

/* =========================================================
   LCD GPIO + FSMC
   ========================================================= */

static void LCD_GPIO_FSMC_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_FSMC_CLK_ENABLE();

    /* LCD Reset = PD3 */
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* LCD Backlight = PF9 */
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    /* FSMC D4-D7 = PE7-PE10 */
    GPIO_InitStruct.Pin =
        GPIO_PIN_7 |
        GPIO_PIN_8 |
        GPIO_PIN_9 |
        GPIO_PIN_10;

    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_FSMC;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    /* FSMC A18, D0-D3, NOE, NWE */
    GPIO_InitStruct.Pin =
        GPIO_PIN_13 |
        GPIO_PIN_14 |
        GPIO_PIN_15 |
        GPIO_PIN_0 |
        GPIO_PIN_1 |
        GPIO_PIN_4 |
        GPIO_PIN_5;

    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* FSMC NE3 = PG10 */
    GPIO_InitStruct.Pin = GPIO_PIN_10;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
}

/* =========================================================
   FSMC
   ========================================================= */

static void LCD_FSMC_Init(void)
{
    SRAM_HandleTypeDef hsram = {0};
    FSMC_NORSRAM_TimingTypeDef readTiming = {0};
    FSMC_NORSRAM_TimingTypeDef writeTiming = {0};

    hsram.Instance = FSMC_NORSRAM_DEVICE;
    hsram.Extended = FSMC_NORSRAM_EXTENDED_DEVICE;

    hsram.Init.NSBank = FSMC_NORSRAM_BANK3;
    hsram.Init.DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE;
    hsram.Init.MemoryType = FSMC_MEMORY_TYPE_SRAM;
    hsram.Init.MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_8;
    hsram.Init.BurstAccessMode = FSMC_BURST_ACCESS_MODE_DISABLE;
    hsram.Init.WaitSignalPolarity = FSMC_WAIT_SIGNAL_POLARITY_LOW;
    hsram.Init.WrapMode = FSMC_WRAP_MODE_DISABLE;
    hsram.Init.WaitSignalActive = FSMC_WAIT_TIMING_BEFORE_WS;
    hsram.Init.WriteOperation = FSMC_WRITE_OPERATION_ENABLE;
    hsram.Init.WaitSignal = FSMC_WAIT_SIGNAL_DISABLE;
    hsram.Init.ExtendedMode = FSMC_EXTENDED_MODE_ENABLE;
    hsram.Init.AsynchronousWait = FSMC_ASYNCHRONOUS_WAIT_DISABLE;
    hsram.Init.WriteBurst = FSMC_WRITE_BURST_DISABLE;
    hsram.Init.PageSize = FSMC_PAGE_SIZE_NONE;

    readTiming.AddressSetupTime = 15;
    readTiming.AddressHoldTime = 0;
    readTiming.DataSetupTime = 60;
    readTiming.BusTurnAroundDuration = 0;
    readTiming.CLKDivision = 0;
    readTiming.DataLatency = 0;
    readTiming.AccessMode = FSMC_ACCESS_MODE_A;

    writeTiming.AddressSetupTime = 9;
    writeTiming.AddressHoldTime = 0;
    writeTiming.DataSetupTime = 8;
    writeTiming.BusTurnAroundDuration = 0;
    writeTiming.CLKDivision = 0;
    writeTiming.DataLatency = 0;
    writeTiming.AccessMode = FSMC_ACCESS_MODE_A;

    HAL_SRAM_Init(
        &hsram,
        &readTiming,
        &writeTiming
    );
}

/* =========================================================
   LCD Initialization
   ========================================================= */

void LCD_Init(void)
{
    LCD_GPIO_FSMC_Init();

    HAL_GPIO_WritePin(
        GPIOD,
        GPIO_PIN_3,
        GPIO_PIN_RESET
    );

    HAL_Delay(100);

    HAL_GPIO_WritePin(
        GPIOD,
        GPIO_PIN_3,
        GPIO_PIN_SET
    );

    HAL_Delay(100);

    LCD_FSMC_Init();
    HAL_Delay(100);

    LCD_WriteCommand(0x36);
    LCD_WriteData8(0x00);

    LCD_WriteCommand(0x3A);
    LCD_WriteData8(0x65);

    LCD_WriteCommand(0xB2);
    LCD_WriteData8(0x0C);
    LCD_WriteData8(0x0C);
    LCD_WriteData8(0x00);
    LCD_WriteData8(0x33);
    LCD_WriteData8(0x33);

    LCD_WriteCommand(0xB7);
    LCD_WriteData8(0x35);

    LCD_WriteCommand(0xBB);
    LCD_WriteData8(0x37);

    LCD_WriteCommand(0xC0);
    LCD_WriteData8(0x2C);

    LCD_WriteCommand(0xC2);
    LCD_WriteData8(0x01);

    LCD_WriteCommand(0xC3);
    LCD_WriteData8(0x12);

    LCD_WriteCommand(0xC4);
    LCD_WriteData8(0x20);

    LCD_WriteCommand(0xC6);
    LCD_WriteData8(0x0F);

    LCD_WriteCommand(0xD0);
    LCD_WriteData8(0xA4);
    LCD_WriteData8(0xA1);

    LCD_WriteCommand(0xE0);

    const uint8_t gammaPositive[] =
    {
        0xD0, 0x04, 0x0D, 0x11,
        0x13, 0x2B, 0x3F, 0x54,
        0x4C, 0x18, 0x0D, 0x0B,
        0x1F, 0x23
    };

    for (uint32_t i = 0; i < sizeof(gammaPositive); i++)
    {
        LCD_WriteData8(gammaPositive[i]);
    }

    LCD_WriteCommand(0xE1);

    const uint8_t gammaNegative[] =
    {
        0xD0, 0x04, 0x0C, 0x11,
        0x13, 0x2C, 0x3F, 0x44,
        0x51, 0x2F, 0x1F, 0x1F,
        0x20, 0x23
    };

    for (uint32_t i = 0; i < sizeof(gammaNegative); i++)
    {
        LCD_WriteData8(gammaNegative[i]);
    }

    LCD_WriteCommand(0x21);

    LCD_WriteCommand(0x35);
    LCD_WriteData8(0x00);

    LCD_WriteCommand(0x11);
    HAL_Delay(120);

    LCD_WriteCommand(0x29);

    HAL_GPIO_WritePin(
        GPIOF,
        GPIO_PIN_9,
        GPIO_PIN_SET
    );
}

/* =========================================================
   LCD Drawing
   ========================================================= */

static void LCD_SetArea(
    uint16_t x1,
    uint16_t y1,
    uint16_t x2,
    uint16_t y2)
{
    LCD_WriteCommand(0x2A);

    LCD_WriteData8(x1 >> 8);
    LCD_WriteData8(x1 & 0xFF);
    LCD_WriteData8(x2 >> 8);
    LCD_WriteData8(x2 & 0xFF);

    LCD_WriteCommand(0x2B);

    LCD_WriteData8(y1 >> 8);
    LCD_WriteData8(y1 & 0xFF);
    LCD_WriteData8(y2 >> 8);
    LCD_WriteData8(y2 & 0xFF);

    LCD_WriteCommand(0x2C);
}

void LCD_Fill(uint16_t color)
{
    LCD_SetArea(0, 0, 239, 239);

    for (uint32_t i = 0; i < 240UL * 240UL; i++)
    {
        LCD_WriteColor(color);
    }
}

void LCD_FillRect(
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    uint16_t color)
{
    LCD_SetArea(
        x,
        y,
        x + width - 1,
        y + height - 1
    );

    for (uint32_t i = 0; i < (uint32_t)width * height; i++)
    {
        LCD_WriteColor(color);
    }
}

/* =========================================================
   Simple 5x7 Font
   ========================================================= */

static const char FONT_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-";

static const uint8_t FONT_5X7[][5] =
{
    {0x7E,0x11,0x11,0x11,0x7E}, // A
    {0x7F,0x49,0x49,0x49,0x36}, // B
    {0x3E,0x41,0x41,0x41,0x22}, // C
    {0x7F,0x41,0x41,0x22,0x1C}, // D
    {0x7F,0x49,0x49,0x49,0x41}, // E
    {0x7F,0x09,0x09,0x09,0x01}, // F
    {0x3E,0x41,0x49,0x49,0x7A}, // G
    {0x7F,0x08,0x08,0x08,0x7F}, // H
    {0x00,0x41,0x7F,0x41,0x00}, // I
    {0x20,0x40,0x41,0x3F,0x01}, // J
    {0x7F,0x08,0x14,0x22,0x41}, // K
    {0x7F,0x40,0x40,0x40,0x40}, // L
    {0x7F,0x02,0x0C,0x02,0x7F}, // M
    {0x7F,0x04,0x08,0x10,0x7F}, // N
    {0x3E,0x41,0x41,0x41,0x3E}, // O
    {0x7F,0x09,0x09,0x09,0x06}, // P
    {0x3E,0x41,0x51,0x21,0x5E}, // Q
    {0x7F,0x09,0x19,0x29,0x46}, // R
    {0x46,0x49,0x49,0x49,0x31}, // S
    {0x01,0x01,0x7F,0x01,0x01}, // T
    {0x3F,0x40,0x40,0x40,0x3F}, // U
    {0x1F,0x20,0x40,0x20,0x1F}, // V
    {0x7F,0x20,0x18,0x20,0x7F}, // W
    {0x63,0x14,0x08,0x14,0x63}, // X
    {0x03,0x04,0x78,0x04,0x03}, // Y
    {0x61,0x51,0x49,0x45,0x43}, // Z

    {0x3E,0x51,0x49,0x45,0x3E}, // 0
    {0x00,0x42,0x7F,0x40,0x00}, // 1
    {0x42,0x61,0x51,0x49,0x46}, // 2
    {0x21,0x41,0x45,0x4B,0x31}, // 3
    {0x18,0x14,0x12,0x7F,0x10}, // 4
    {0x27,0x45,0x45,0x45,0x39}, // 5
    {0x3C,0x4A,0x49,0x49,0x30}, // 6
    {0x01,0x71,0x09,0x05,0x03}, // 7
    {0x36,0x49,0x49,0x49,0x36}, // 8
    {0x06,0x49,0x49,0x29,0x1E}, // 9

    {0x08,0x08,0x08,0x08,0x08}  // -
};

static const uint8_t *GetGlyph(char c)
{
    if (c == ' ')
    {
        return nullptr;
    }

    for (uint32_t i = 0; i < sizeof(FONT_CHARS) - 1; i++)
    {
        if (FONT_CHARS[i] == c)
        {
            return FONT_5X7[i];
        }
    }

    return nullptr;
}

static void LCD_DrawChar(
    uint16_t x,
    uint16_t y,
    char c,
    uint16_t color,
    uint8_t scale)
{
    const uint8_t *glyph = GetGlyph(c);

    if (glyph == nullptr)
    {
        return;
    }

    for (uint8_t column = 0; column < 5; column++)
    {
        for (uint8_t row = 0; row < 7; row++)
        {
            if (glyph[column] & (1 << row))
            {
                LCD_FillRect(
                    x + column * scale,
                    y + row * scale,
                    scale,
                    scale,
                    color
                );
            }
        }
    }
}

void LCD_DrawText(
    uint16_t x,
    uint16_t y,
    const char *text,
    uint16_t color,
    uint8_t scale)
{
    while (*text)
    {
        LCD_DrawChar(
            x,
            y,
            *text,
            color,
            scale
        );

        x += 6 * scale;
        text++;
    }
}

