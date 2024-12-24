/*
 * display_neopixel.c
 *
 * implementation of neopixel based display of data
 * TBD: determine how fast the display can be updated
 * TBD: add timer/interrupt based update of display
 */
/* 
 * NEOPIXEL SECTION
 * 
 * Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
 * or you can edit the following line and set a number here.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"

#include "display_neopixel.h"

#define BLINK_GPIO CONFIG_BLINK_GPIO  // set the gpio line for neopixel data output

/*
 * logging
 */
static const char *TAG = "display_neopixel";

static uint8_t s_led_state = 0;

/*
 * create the instance of the neopixel strip (i.e. "array")
 */
static led_strip_handle_t led_strip;

/*
 * simple OG example to blink a single neopixel
 * (code not used in chase example)
 */
static void blink_led(void)
{
    /* If the addressable LED is enabled */
    if (s_led_state) {
        /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
        led_strip_set_pixel(led_strip, 0, 16, 16, 16);
        /* Refresh the strip to send data */
        led_strip_refresh(led_strip);
    } else {
        /* Set all LED off to clear all pixels */
        led_strip_clear(led_strip);
    }
}


/*
 * update the led (neo_pixel) display based on the mode
 */
void display_neopixel_update(uint8_t display_neopixel_mode, int32_t value)  {
    switch(display_neopixel_mode)  {
        case PONG_EXAMPLE:
            led_next_pong();
        break;

        case SIM_REG_EXAMPLE:
            led_next_reg();
        break;

        case EXCEL_COLOR_VALUE:
            led_bargraph_update(value);
        break;

        case BANDED_COLOR_VALUE:
            ESP_LOGI(TAG, "display_neopixel mode %d not yet implemented", display_neopixel_mode);
        break;

        default:
            ESP_LOGI(TAG, "WARNING: invalid display_neopixel mode %d", display_neopixel_mode);
        break;
    }
}
/*
 * move one led along the strip and back each time it's called
 * (i.e. create a chase by calling repeatedly)
 */
static int8_t cur_led = -1;  // the led that is currently lit
static uint8_t led_dir = 1;  // 1 = fwd, 0 = rev
static uint8_t r = 16, g = 0, b = 0;
#define NUM_LEDS 20  // temporary hack TODO
void led_next_pong(void)
{
    if(led_dir == 1)
    {
        cur_led++;
        if(cur_led >= NUM_LEDS)
        {
            led_dir = 0;
            cur_led--;
        }
    }
    else
    {
        cur_led--;
        if (cur_led < 0)
        {
            led_dir = 1;
            cur_led++;
        }
        
    }

    led_strip_clear(led_strip);
    led_strip_set_pixel(led_strip, cur_led, r, g, b);
    led_strip_refresh(led_strip);
}

/*
 * SIM_REG_EXAMPLE mode

 * play out the led_reg_message[] values on the lower 8 bits,
 * while indicating the index thereof in the upper 8 bits
 */
#define LED_REG_WIDTH 8  // number of leds to involve in this mode 0 - (this-1)
#define LED_REG_MSG_SIZE 7
#define LED_REG_IDX_START 8 // index lsb: where the led_idx is shown

static uint8_t led_reg_message[] = {0x4e, 0x43, 0x43, 0x31, 0x37, 0x30, 0x31};
static int8_t led_idx = -1;  // to step through the message

void led_next_reg(void)  {
    int8_t end_idx = LED_REG_MSG_SIZE - 1;  // to know we are at end of message
    uint8_t mask = 0x01;  // anded with ascii value to assign bit values

    led_strip_clear(led_strip);

    led_idx++;  // move to the next character in the message
    if(led_idx > end_idx)
        led_idx = 0;
    
    /*
     * set the led_idx value in the upper 8 bits of the neo_pixel display
     */
    led_strip_set_pixel(led_strip, (led_idx + LED_REG_IDX_START), r, g, b);

    /*
     * fill in the bit field from the led_reg_message[] values
     * i.e. map the ascii bits of the message to the led bit state
     */
    for(uint8_t i = 0; i < (uint8_t)LED_REG_WIDTH; i++)  {
        if((led_reg_message[led_idx] & mask) == (uint8_t)0)
            led_strip_set_pixel(led_strip, i, 0, 0, 0);   // change the in-memory version (doesn't write physical led strip)
        else
            led_strip_set_pixel(led_strip, i, r, g, b);
        mask = mask << 1;
    }

    led_strip_refresh(led_strip);

}


/*
 * EXCEL_COLOR_VALUE mode
 * load a baseline/dim graduated scale and brighten 
 * from the bottom (bar graph) based on an integer value.
 * the min and max range is also set.
 */

/*
 * define the background colors (i.e. off) color
 * of each led
 * NOTE: you have to manually manage the length of this array
 */
#define LED_R 0
#define LED_G 1
#define LED_B 2
static const uint8_t led_bargraph_off_colors[NUM_LEDS][3] = {
    {0, 5, 0},
    {0, 5, 0},
    {0, 5, 0},
    {0, 5, 0},
    {0, 5, 0},
    {0, 5, 0},
    {5, 5, 0},
    {5, 5, 0},
    {5, 5, 0},
    {5, 5, 0},
    {5, 5, 0},
    {5, 5, 0},
    {5, 5, 0},
    {5, 0, 0},
    {5, 0, 0},
    {5, 0, 0},
    {5, 0, 0},
    {5, 0, 0},
    {5, 0, 0},
    {5, 0, 0},
};

static const uint8_t led_bargraph_on_colors[NUM_LEDS][3] = {
    {0, 32, 0},
    {0, 32, 0},
    {0, 32, 0},
    {0, 32, 0},
    {0, 32, 0},
    {0, 32, 0},
    {16, 16, 0},
    {16, 16, 0},
    {16, 16, 0},
    {16, 16, 0},
    {16, 16, 0},
    {16, 16, 0},
    {16, 16, 0},
    {32, 0, 0},
    {32, 0, 0},
    {32, 0, 0},
    {32, 0, 0},
    {32, 0, 0},
    {32, 0, 0},
    {32, 0, 0},
};

static int32_t led_bargraph_max = 0;
static int32_t led_bargraph_min = 0;
void led_bargraph_max_set(int32_t max_value)  {
    led_bargraph_max = max_value;
}

void led_bargraph_min_set(int32_t min_value)  {
    led_bargraph_min = min_value;
}

/*
 * led_bargraph_update()
 *
 * clear, set background color, light leds based on passed in argument value
 * 
 * NOTE: you have to manage converting your parameter to an positive integer range/value
 *       (the value is confirmed to be between min/max)
 * TODO: make this work for negative numbers
 */
void led_bargraph_update(int32_t value)  {
    uint8_t top_on_pixel = 0;  // on this pixel and below
    int32_t led_segment = 0;

    if(value <= 0)  value = 0;

    led_strip_clear(led_strip);

    /*
     * overwrite the ones that should be on based on the input value
     */
    if((led_segment = value - led_bargraph_min) < led_bargraph_min)
        led_segment = 0;
    if((led_segment = value - led_bargraph_min) > led_bargraph_max)
        led_segment = led_bargraph_max;

    /*
     * what's the top pixel that should be turned on
     * then index through the pixels turning on those below the top_on_pixel,
     * and turning off those above (on means on color, ditto off)
     */
    top_on_pixel = led_segment / ((led_bargraph_max - led_bargraph_min)/NUM_LEDS);
    for(uint8_t i = 0; i < NUM_LEDS; i++)  {
        if(i < top_on_pixel)
            led_strip_set_pixel(led_strip, i, led_bargraph_on_colors[i][LED_R], 
                                led_bargraph_on_colors[i][LED_G],
                                led_bargraph_on_colors[i][LED_B]);
        else
            led_strip_set_pixel(led_strip, i, led_bargraph_off_colors[i][LED_R], 
                                led_bargraph_off_colors[i][LED_G],
                                led_bargraph_off_colors[i][LED_B]);
    }

    led_strip_refresh(led_strip);
}

/*
 * float range to integer range helper
 */
int32_t led_bargraph_map(float value, float min_value, float max_value)  {
    return value * ((led_bargraph_max - led_bargraph_min)/(max_value - min_value));
}

/*
 * configure_led()
 *
 * configure the neopixel strip (common for all modes, called once)
 * and the hardware path to create the data stream:
 * CONFIG_BLINK_LED_STRIP_BACKEND_RMT: use RMT hardware (tested)
 * CONFIG_BLINK_LED_STRIP_BACKEND_SPI: use SPI hardware
 * NOTE: both use dedicated hardware to play out the sequence
 * once loaded (i.e. doesn't require software to refresh)
 *
 */
void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink addressable LED!");
    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
        .strip_gpio_num = BLINK_GPIO,
        .max_leds = 20, // at least one LED on board
    };
#if CONFIG_BLINK_LED_STRIP_BACKEND_RMT
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
#elif CONFIG_BLINK_LED_STRIP_BACKEND_SPI
    led_strip_spi_config_t spi_config = {
        .spi_bus = SPI2_HOST,
        .flags.with_dma = true,
    };
    ESP_ERROR_CHECK(led_strip_new_spi_device(&strip_config, &spi_config, &led_strip));
#else
#error "unsupported LED strip backend"
#endif
    /* Set all LED off to clear all pixels */
    led_strip_clear(led_strip);
}
