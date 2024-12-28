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
#include "driver/gptimer.h"

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

// start of display mode functions

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
 * fast display - timer/interrupt driven display of waveform values
 *                on neopixel display.  the waveform is expected to be an array
 *                of integers and the isr increments the index to the next value.
 *                if the display task can run, it will update the physical neopixel
 *                strand.  If not, it'll get it next time.
 * NOTE: demo plays out an ekg waveform at a consistent rate of 1000 samples/s
 * need:
 *   timer
 *   semaphore
 *   waveform storage/conversion
 *   
 */
int32_t led_bargraph_fast_value[] = {0};

/*
 * ekg waveform  (543 samples) 
 */
#define EKG_NUM_SAMPLES 543
const short  ekg_data[] = {
939, 940, 941, 942, 944, 945, 946, 947, 951, 956, 
962, 967, 973, 978, 983, 989, 994, 1000, 1005, 1015, 
1024, 1034, 1043, 1053, 1062, 1075, 1087, 1100, 1112, 1121, 
1126, 1131, 1136, 1141, 1146, 1151, 1156, 1164, 1172, 1179, 
1187, 1194, 1202, 1209, 1216, 1222, 1229, 1235, 1241, 1248, 
1254, 1260, 1264, 1268, 1271, 1275, 1279, 1283, 1287, 1286, 
1284, 1281, 1279, 1276, 1274, 1271, 1268, 1266, 1263, 1261, 
1258, 1256, 1253, 1251, 1246, 1242, 1237, 1232, 1227, 1222, 
1218, 1215, 1211, 1207, 1203, 1199, 1195, 1191, 1184, 1178, 
1171, 1165, 1159, 1152, 1146, 1141, 1136, 1130, 1125, 1120, 
1115, 1110, 1103, 1096, 1088, 1080, 1073, 1065, 1057, 1049, 
1040, 1030, 1021, 1012, 1004, 995, 987, 982, 978, 974, 
970, 966, 963, 959, 955, 952, 949, 945, 942, 939, 
938, 939, 940, 941, 943, 944, 945, 946, 946, 946, 
946, 946, 946, 946, 946, 947, 950, 952, 954, 956, 
958, 960, 962, 964, 965, 965, 965, 965, 965, 965, 
963, 960, 957, 954, 951, 947, 944, 941, 938, 932, 
926, 920, 913, 907, 901, 894, 885, 865, 820, 733,
606, 555, 507, 632, 697, 752, 807, 896, 977, 1023, 
1069, 1127, 1237, 1347, 1457, 2085, 2246, 2474, 2549, 2595, 
2641, 2695, 3083, 3135, 3187, 3217, 3315, 3403, 3492, 3581, 
3804, 3847, 3890, 3798, 3443, 3453, 3297, 3053, 2819, 2810, 
2225, 2258, 1892, 1734, 1625, 998, 903, 355, 376, 203, 
30, 33, 61, 90, 119, 160, 238, 275, 292, 309, 
325, 343, 371, 399, 429, 484, 542, 602, 652, 703, 
758, 802, 838, 856, 875, 895, 917, 938, 967, 1016, 
1035, 1041, 1047, 1054, 1060, 1066, 1066, 1064, 1061, 1058, 
1056, 1053, 1051, 1048, 1046, 1043, 1041, 1038, 1035, 1033, 
1030, 1028, 1025, 1022, 1019, 1017, 1014, 1011, 1008, 1006, 
1003, 1001, 999, 998, 996, 994, 993, 991, 990, 988, 
986, 985, 983, 981, 978, 976, 973, 971, 968, 966, 
963, 963, 963, 963, 963, 963, 963, 963, 963, 963, 
963, 963, 963, 963, 963, 963, 963, 963, 963, 963, 
964, 965, 966, 967, 968, 969, 970, 971, 972, 974, 
976, 978, 980, 983, 985, 987, 989, 991, 993, 995, 
997, 999, 1002, 1006, 1011, 1015, 1019, 1023, 1028, 1032, 
1036, 1040, 1045, 1050, 1055, 1059, 1064, 1069, 1076, 1082, 
1088, 1095, 1101, 1107, 1114, 1120, 1126, 1132, 1141, 1149, 
1158, 1166, 1173, 1178, 1183, 1188, 1193, 1198, 1203, 1208, 
1214, 1221, 1227, 1233, 1240, 1246, 1250, 1254, 1259, 1263, 
1269, 1278, 1286, 1294, 1303, 1309, 1315, 1322, 1328, 1334, 
1341, 1343, 1345, 1347, 1349, 1351, 1353, 1355, 1357, 1359, 
1359, 1359, 1359, 1359, 1358, 1356, 1354, 1352, 1350, 1347, 
1345, 1343, 1341, 1339, 1336, 1334, 1332, 1329, 1327, 1324, 
1322, 1320, 1317, 1315, 1312, 1307, 1301, 1294, 1288, 1281, 
1275, 1270, 1265, 1260, 1256, 1251, 1246, 1240, 1233, 1227, 
1221, 1214, 1208, 1201, 1194, 1186, 1178, 1170, 1162, 1154, 
1148, 1144, 1140, 1136, 1131, 1127, 1123, 1118, 1114, 1107, 
1099, 1090, 1082, 1074, 1069, 1064, 1058, 1053, 1048, 1043, 
1038, 1034, 1029, 1025, 1021, 1017, 1013, 1009, 1005, 1001, 
997, 994, 990, 991, 992, 994, 996, 997, 999, 998, 
997, 996, 995, 994, 993, 991, 990, 989, 989, 989, 
989, 989, 989, 989, 988, 986, 984, 983, 981, 980, 
982, 984, 986, 988, 990, 993, 995, 997, 999, 1002, 
1005, 1008, 1012};
int32_t led_bargraph_fast_index = 0;

/*
 * callback to increment the waveform index
 */
static bool IRAM_ATTR fast_bg_cbs(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data)  {
    BaseType_t high_task_awoken = pdFALSE;

    if(++led_bargraph_fast_index >= EKG_NUM_SAMPLES)
        led_bargraph_fast_index = 0; // to the next data value

    ESP_ERROR_CHECK(gptimer_set_raw_count(timer, 0));

    return (high_task_awoken == pdTRUE);
}

/*
 * set up led_bargraph_fast_timer
 */
void led_bargraph_fast_timer_init(void)  {

    int32_t my_data = 0; // not sure if I'll need this; passed to isr

    /*
     * set up the timer
     */
    ESP_LOGI(TAG, "Create led fast display timer handle");
    gptimer_handle_t fast_bg_gptimer = NULL;
    gptimer_config_t fast_bg_timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000, // 1kHz, 1 tick=1us
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&fast_bg_timer_config, &fast_bg_gptimer));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = fast_bg_cbs,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(fast_bg_gptimer, &cbs, &my_data));

    ESP_LOGI(TAG, "Enable fast_bg_timer");
    ESP_ERROR_CHECK(gptimer_enable(fast_bg_gptimer));

    /*
     * attach the isr to the timer
     */

    /*
     * start the timer
     */
}


/*
 * update the value to be displayed in shared memory
 * (this will be the isr)
 */
void led_bargraph_update_fast_value(void)  {

}

/*
 * check if the value has changed and update the neopixel
 * strip if so
 * 
 * this function will block on the data semaphore
 * 
 * this function checks to see if, given the resolution of 
 * the display (i.e. number of leds), the display would actually
 * change given the size of the change in input value.  Doesn't do
 * anything if it doesn't need to.
 */
void led_bargraph_update_fast_display (void)  {

}

// end of display mode functions


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
