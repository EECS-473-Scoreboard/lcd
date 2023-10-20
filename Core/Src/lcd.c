#include "lcd.h"

#include "stm32h7xx_hal_def.h"
#include <stdlib.h>
#include <string.h>

// take the median of this number of adc readings as the final output
#define ADC_MEDIAN_SIZE 5001

// framebuffer stored in 320K SRAM1
__attribute__((section(".ram_d1")))
uint8_t fb[LCD_RENDER_WIDTH * LCD_RENDER_HEIGHT];
// color look-up table
uint32_t CLUT[256];

void init_display() {
    // point LTDC to framebuffer
    HAL_LTDC_SetAddress(&hltdc, (uint32_t)fb, LTDC_LAYER_1);

    // create a CLUT so that every rgb332 color maps to its rgb888 value
    for (uint8_t rgb332 = 0; rgb332 != 255; rgb332++) {
        uint8_t r = (uint32_t)(((rgb332 & 0xE0) >> 5) * 255 / 7);
        uint8_t g = (uint32_t)(((rgb332 & 0x1C) >> 2) * 255 / 7);
        uint8_t b = (uint32_t)((rgb332 & 0x03) * 255 / 3);
        // LTDC_LxCLUTWR[31:0]: CLUTADD[7:0] RED[7:0] GREEN[7:0] BLUE[7:0]
        // Note on endianess: will be writing to register, not memory.
        CLUT[rgb332] = (rgb332 << 24) + (r << 16) + (g << 8) + b;
    }
    // point LTDC to CLUT
    HAL_LTDC_ConfigCLUT(&hltdc, CLUT, 256, LTDC_LAYER_1);
    // load CLUT. Each word in the array will be written to the CLUTWR register
    // of layer 1. Such write happens for 256 times.
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

inline static void to_ground(GPIO_TypeDef *port, uint16_t pin) {
    GPIO_InitTypeDef s = {0};
    s.Pin = pin;
    s.Mode = GPIO_MODE_OUTPUT_PP;
    s.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(port, &s);
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
}

inline static void to_power(GPIO_TypeDef *port, uint16_t pin) {
    GPIO_InitTypeDef s = {0};
    s.Pin = pin;
    s.Mode = GPIO_MODE_OUTPUT_PP;
    s.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(port, &s);
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
}

inline static void to_floating(GPIO_TypeDef *port, uint16_t pin) {
    GPIO_InitTypeDef s = {0};
    s.Pin = pin;
    s.Mode = GPIO_MODE_INPUT;
    s.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(port, &s);
}

// MUX the GPIO pin to ADC
inline static void to_adc(GPIO_TypeDef *port, uint16_t pin) {
    GPIO_InitTypeDef s = {0};
    s.Pin = pin;
    s.Mode = GPIO_MODE_ANALOG;
    s.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(port, &s);
}

static int compare_u16(const void *a, const void *b) {
    uint16_t left = *(uint16_t *)a;
    uint16_t right = *(uint16_t *)b;
    return (left > right) - (left < right);
}

// size must be an odd number
// modifies provided array
static uint16_t median(uint16_t *begin, size_t size) {
    qsort(begin, size, 2, compare_u16);
    return begin[size / 2];
}

// DMA hardware cannot touch DTCM, but can touch SRAM1
__attribute__((
    section(".ram_d1"))) volatile uint16_t adc_dma_buffer[ADC_DMA_BUF_SIZE];
volatile int adc_ready;

// called when all ADC channels have finished conversion
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *_) { adc_ready = 1; }

static uint16_t read_adc1(uint32_t rank) {
    adc_ready = 0;
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_dma_buffer, ADC_DMA_BUF_SIZE);
    while (!adc_ready)
        ;
    HAL_ADC_Stop_DMA(&hadc1);
    return adc_dma_buffer[rank];
}

static uint16_t read_x() {
    // XL: V+, XR: GND, YU: float, YD: ADC
    to_power(LCD_X_L_PORT, LCD_X_L_PIN);
    to_ground(LCD_X_R_PORT, LCD_X_R_PIN);
    to_floating(LCD_Y_U_PORT, LCD_Y_U_PIN);
    to_adc(LCD_Y_D_PORT, LCD_Y_D_PIN);

    uint16_t buffer[ADC_MEDIAN_SIZE];
    for (size_t i = 0; i < ADC_MEDIAN_SIZE; i++) {
        buffer[i] = read_adc1(LCD_Y_D_ADC_RANK);
    }
    return median(buffer, ADC_MEDIAN_SIZE);
}

static uint16_t read_y() {
    // YU: V+, YD: GND, XL: float, XR: ADC
    to_power(LCD_Y_U_PORT, LCD_Y_U_PIN);
    to_ground(LCD_Y_D_PORT, LCD_Y_D_PIN);
    to_floating(LCD_X_L_PORT, LCD_X_L_PIN);
    to_adc(LCD_X_R_PORT, LCD_X_R_PIN);

    uint16_t buffer[ADC_MEDIAN_SIZE];
    for (size_t i = 0; i < ADC_MEDIAN_SIZE; i++) {
        buffer[i] = read_adc1(LCD_X_R_ADC_RANK);
    }
    return median(buffer, ADC_MEDIAN_SIZE);
}

void init_touch() {}

int read_touch(lv_disp_drv_t *_, lv_indev_data_t *data) {
    // if no touch, x and y value should persist
    static uint16_t x = 0;
    static uint16_t y = 0;

    uint16_t newX = read_x();
    uint16_t newY = read_y();

    if (newX == 0 && newY == 0) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_REL;
    } else {
        x = newX;
        y = newY;
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PR;
    }

    return 0;
}