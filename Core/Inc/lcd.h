#ifndef LCD_H
#define LCD_H

#include "ltdc.h"

#define LCD_RENDER_WIDTH 800
#define LCD_RENDER_HEIGHT 400

typedef void lv_disp_drv_t;
typedef struct {
    uint16_t x1;
    uint16_t x2;
    uint16_t y1;
    uint16_t y2;
} lv_area_t;
typedef uint8_t lv_color_t;

extern uint8_t fb[LCD_RENDER_WIDTH * LCD_RENDER_HEIGHT];
extern uint32_t CLUT[256];

void init_display();
void flush_cb(lv_disp_drv_t *, const lv_area_t *, lv_color_t *);
void clean_dcache_cb(lv_disp_drv_t *);

void test_display();

#endif