#include "lcd.h"

#include <string.h>

// framebuffer stored in 320K SRAM1
__attribute__((section(".ram_d1"))) uint8_t fb[800 * 400];
// color look-up table
uint32_t CLUT[256];

void init_display() {
    // point LTDC to framebuffer
    HAL_LTDC_SetAddress(&hltdc, (uint32_t)fb, LTDC_LAYER_1);

    // rgb332 to rgb888 conversion
    for (uint8_t rgb332 = 0; rgb332 != 255; rgb332++) {
        // LTDC_LxCLUTWR[31:0]: CLUTADD[7:0] RED[7:0] GREEN[7:0] BLUE[7:0]
        uint8_t r = (uint32_t)(((rgb332 & 0xE0) >> 5) * 255 / 7);
        uint8_t g = (uint32_t)(((rgb332 & 0x1C) >> 2) * 255 / 7);
        uint8_t b = (uint32_t)((rgb332 & 0x03) * 255 / 3);
        // Each word in the array will be written to the CLUTWR register of
        // layer 1. Such write happens for 256 times.
        CLUT[rgb332] = (rgb332 << 24) + (r << 16) + (g << 8) + b;
    }
    // point LTDC to CLUT
    HAL_LTDC_ConfigCLUT(&hltdc, CLUT, 256, LTDC_LAYER_1);
    // load CLUT
    HAL_LTDC_EnableCLUT(&hltdc, LTDC_LAYER_1);
}

void test_display() {
    for (int line = 0; line <= LCD_RENDER_HEIGHT; line++) {
        uint8_t color;
        if (line < LCD_RENDER_HEIGHT / 4) {
            // R
            color = 0b11100000;
        } else if (line < LCD_RENDER_HEIGHT / 2) {
            // G
            color = 0b00011100;
        } else if (line < LCD_RENDER_HEIGHT / 4 * 3) {
            // B
            color = 0b00000011;
        } else {
            // Miku 39c5bb is R: 2, G: 5, B: 2 = 0b01010110
            color = 0b01010110;
        }
        memset((void *)(fb + line * LCD_RENDER_WIDTH), color, LCD_RENDER_WIDTH);
    }
}