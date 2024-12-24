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
    BANDED_COLOR_VALUE  // bar graph with fixed range colors
};

void configure_led(void);  // called once to initialize the led_strip

void display_neopixel_update(uint8_t display_neopixel_mode);  // call the appropriate update function based on mode

void ping_led(void);  // PONG_EXAMPLE: light up the next pixel and check for direction change
void next_reg_led(void); // SIM_REG_EXAMPLE: display simulated register values


#define __DISPLAY_NEOPIXEL_H__
#endif
