/*
 * display_neopixel.h
 *
 * implementation of neopixel based display of data
 * TBD: migrate kconfig based example configurtion to #defines here
 */

#ifndef __DISPLAY_NEOPIXEL_H__  

/*
 * which mode is being played out by the neo_pixel display
 */
enum Neo_pixel_disp_mode  {
    PONG_EXAMPLE,       // single color led moves left-right
    SIM_REG_EXAMPLE,    // like an old school register display
    EXCEL_COLOR_VALUE,  // bar graph with graduated color scale
    NEO_FLASHLIGHT,     // all on white
    BANDED_COLOR_VALUE,  // bar graph with fixed range colors
    FAST_WAVEFORM,      // fast waveform display with timer/interrupt
};

void configure_led(void);  // called once to initialize the led_strip

void display_neopixel_update(uint8_t display_neopixel_mode, int32_t value);  // call the appropriate update function based on mode

void led_next_pong(void);  // PONG_EXAMPLE: light up the next pixel and check for direction change
void led_next_reg(void); // SIM_REG_EXAMPLE: display simulated register values
void led_bargraph_fast_timer_init(void); // FAST_WAVEFORM: start automomous/timer based display of waveform

void led_bargraph_max_set(int32_t max_value);
void led_bargraph_min_set(int32_t min_value);
void led_bargraph_update(int32_t value);
int32_t led_bargraph_map(float value, float min_value, float max_value);


#define __DISPLAY_NEOPIXEL_H__
#endif
