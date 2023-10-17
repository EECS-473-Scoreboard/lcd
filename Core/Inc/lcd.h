#ifndef LCD_H
#define LCD_H

#include "adc.h"
#include "ltdc.h"

#define LCD_RENDER_WIDTH 800
#define LCD_RENDER_HEIGHT 400

#define LCD_X_R_PORT GPIOF
#define LCD_X_R_PIN GPIO_PIN_12
#define LCD_X_L_PORT GPIOF
#define LCD_X_L_PIN GPIO_PIN_11
#define LCD_Y_D_PORT GPIOA
#define LCD_Y_D_PIN GPIO_PIN_7
#define LCD_Y_U_PORT GPIOC
#define LCD_Y_U_PIN GPIO_PIN_5
#define LCD_ADC_PORT ADC1
#define LCD_X_R_ADC_CHAN ADC_CHANNEL_2
#define LCD_Y_D_ADC_CHAN ADC_CHANNEL_7

typedef void lv_disp_drv_t;
typedef struct {
    uint16_t x1;
    uint16_t x2;
    uint16_t y1;
    uint16_t y2;
} lv_area_t;
typedef uint8_t lv_color_t;
enum lv_indev_state_t { LV_INDEV_STATE_PR, LV_INDEV_STATE_REL };
typedef struct {
    struct {
        uint16_t x;
        uint16_t y;
    } point;
    int state;
} lv_indev_data_t;

extern uint8_t fb[LCD_RENDER_WIDTH * LCD_RENDER_HEIGHT];
extern uint32_t CLUT[256];

extern ADC_ChannelConfTypeDef adc_conf;

void init_display();
void flush_cb(lv_disp_drv_t *, const lv_area_t *, lv_color_t *);
void clean_dcache_cb(lv_disp_drv_t *);

void test_display();

void init_touch();
int read_touch(lv_disp_drv_t *, lv_indev_data_t *);

#endif