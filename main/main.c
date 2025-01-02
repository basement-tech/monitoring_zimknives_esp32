/*
 * monitoring_zimknives_esp32
 *
 * General: this is a port of the monolithic monitoring_zimknives project to
 * a multi-core, multi-thread, esp-idf project.
 * 
 * main():
 * - this filel will serve as the "setup" file/function like arduino setup()
 * - in addition, main() will start all of the separate threads/processes
 * 
 * DJZ - December 8, 2024 (c)
 *  
 */
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "monitoring_zimknives.h"

#include "wifi_station.h"
#include "mqtt_local.h"

#include "sensor_acquisition.h"

#include "display_neopixel.h"

static const char *TAG = "main";  // for logging

/*
 * slow acquisition task
 * acquires data from slower (~1s) sensors
 * espect an include file for each sensor here
 * 
 * sensor_init_slow() : initialize the sensors that will use the slow acq loop
 * sensor_acq_slow()  : read the slow acq loop sensors
 * 
 * Note on STACK_SIZE: used value of 2048 from example and after adding more code
 * got irrational error relating to "i2c driver not loaded".  Increased to 4096 and
 * the error was resolved.  was red hering : not related to stack 
 * (was new parameter in i2c conf struct).  I'll leave at 8096.  TODO: research
 */

#define STACK_SIZE 8096
TaskHandle_t xHandle_1 = NULL;

void sensor_acq_slow(void *pvParameters)  {

  sensor_init_slow();

  while(1)  {
    ESP_LOGI(TAG, "sensor_acq_slow(): executing on core %d", xPortGetCoreID());

    ESP_LOGI(TAG, "slow acquisition initiated");
    acquire_sensors();
    display_sensors();

    vTaskDelay(SLOW_LOOP_INTERVAL / portTICK_PERIOD_MS);
  }
}

/*
 * the neopixel function itself
 * (i.e. started as a task in main)
 * 
 */
//#define DISPLAY_NEOPIXEL_MODE PONG_EXAMPLE
//#define DISPLAY_NEOPIXEL_SPEED (50 / portTICK_PERIOD_MS)  // mS between strip updates

//#define DISPLAY_NEOPIXEL_MODE SIM_REG_EXAMPLE
//#define DISPLAY_NEOPIXEL_SPEED (1000 / portTICK_PERIOD_MS)  // mS between strip updates

//#define DISPLAY_NEOPIXEL_MODE EXCEL_COLOR_VALUE
//#define DISPLAY_NEOPIXEL_SPEED (2000 / portTICK_PERIOD_MS)  // mS between strip update

#define DISPLAY_NEOPIXEL_MODE FAST_WAVEFORM
#define FAST_ACQ_PRIO tskIDLE_PRIORITY

/*
 * throttle the display to ~100 ups
 * the detail of the EKG simulated waveform emerges when this is about 100 ups
 * also, seems that the display update time becomes more consistent with this
 * delay included.
 */
#define DISPLAY_NEOPIXEL_SPEED (5 / portTICK_PERIOD_MS)
/*
 * throttle the display to ~100 ups
 */
//#define DISPLAY_NEOPIXEL_SPEED (0 / portTICK_PERIOD_MS)  // let the scheduler decide

// which data value to display for EXCEL_COLOR_VALUE mode
#define DATA_VALUE_SINE 0  // canned sin wave
#define DATA_VALUE_INT  1  // incremented integer
#define DATA_VALUE_HUM  3  // live humidity data from HTU21D
#define DISPLAY_NEOPIXEL_VALUE  DATA_VALUE_HUM

/*
 * used for a simple integer simulation
 */
#define LED_BARGRAPH_MAX 100
#define LED_BARGRAPH_MIN 0
int32_t led_bargraph_value = LED_BARGRAPH_MIN;
static void led_bargraph_incr(void)  {
  led_bargraph_value += 5;
  if(led_bargraph_value > LED_BARGRAPH_MAX)
    led_bargraph_value = LED_BARGRAPH_MIN;
}

/*
 * used for a floating point sinewave simulation
 */
static uint8_t sine_idx = -1;
#define SINE_WAVE_ELE 20
#define SINE_WAVE_MIN (float)0.0
#define SINE_WAVE_MAX (float)2.0
static const float sine_wave_data[SINE_WAVE_ELE] = {
  0.24, 0.01, 0.44, 1.25, 1.89, 1.93, 1.33, 0.51, 0.02, 0.20, 0.92, 1.70, 2.00, 1.62, 0.82, 0.14, 0.05, 0.60, 1.42
};

/*
 * increment and return the next value in the sinewave data array
 */
static float sine_wave_incr()  {
  sine_idx++;

  if(sine_idx >= SINE_WAVE_ELE)
    sine_idx = 0;
  
  return(sine_wave_data[sine_idx]);
}

static void neopixel_example(void *pvParameters)
{
    float sine_value = 0;
    /*
     * Configure the peripheral according to the LED type
     */
    configure_led();

    /*
     * configure/start the neo_pixel demo mode
     */
    if(DISPLAY_NEOPIXEL_MODE == EXCEL_COLOR_VALUE)  {
      led_bargraph_min_set(0);
      led_bargraph_max_set(255);
    }
    else if (DISPLAY_NEOPIXEL_MODE == FAST_WAVEFORM)  {
      led_bargraph_min_set(0);
      led_bargraph_max_set(4096);
//      led_bargraph_fast_timer_init(); // initialize and start the time, intr, display
    }

    /*
     * some slightly badly structured code to try out
     * a few different ways of displaying things on the neopixel strip
     * 
     * NOTE: for the fast waveform display, nothing needs to be done
     * in the loop since movement is driven from a timer
     */
    while(1)
    {
        if(DISPLAY_NEOPIXEL_MODE == EXCEL_COLOR_VALUE)  {
          ;
#if DISPLAY_NEOPIXEL_VALUE == DATA_VALUE_INT
            led_bargraph_incr();  // simple integer simulation
#elif DISPLAY_NEOPIXEL_VALUE == DATA_VALUE_SINE
            sine_value = sine_wave_incr();  // sine wave simulation
            display_neopixel_update(DISPLAY_NEOPIXEL_MODE, led_bargraph_map(sine_value, SINE_WAVE_MIN, SINE_WAVE_MAX));
#elif DISPLAY_NEOPIXEL_VALUE == DATA_VALUE_HUM
          /*
           * display the humidity sensor (sensor[0]) data across the
           * neopixel array with 0% at the bottom and 50% at that top
           */
            ESP_LOGI(TAG, "displaying %s on neo_pixels, value = %f", sensors[0].label, *((float *)(sensors[0].data)));
            display_neopixel_update(DISPLAY_NEOPIXEL_MODE, led_bargraph_map(*((float *)(sensors[0].data)), 0, 50));
        }
#endif
        else if (DISPLAY_NEOPIXEL_MODE == SIM_REG_EXAMPLE)
          display_neopixel_update(DISPLAY_NEOPIXEL_MODE, 0);

        else if (DISPLAY_NEOPIXEL_MODE == FAST_WAVEFORM)
          led_bargraph_update_fast_display();

        else
          ESP_LOGI(TAG, "nothing to do in loop");

        if(DISPLAY_NEOPIXEL_SPEED > 0)
          vTaskDelay(DISPLAY_NEOPIXEL_SPEED);  // set speed of neopixel chase here and give IDLE() time to run
    }
}

/*
 * split out the fast acquisition from the neopixel display task
 * so separate priorities can be set
 * this task increments the index into the simulated data array
 * on a hardware timer
 */
static void fast_acq_sim_task(void *pvParameters)  {

   led_bargraph_fast_timer_init();

   while(1);
}

/*
 * main task:
 * start wifi
 * start mqtt
 * start other tasks
 * loop and publish health/welfare ping
 */
void app_main(void)
{
  char wifi_key[16];  // key to wifi instance
  int msg_id;  // mqtt message id from broker

  instru_gpio_init();

    /*
     * Initialize NVS - apparently saves last successful connect credentials
     */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /*
     * attempt to connect to wifi
     */
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    //esp_wifi_connect();  /* apparently not required */
    /* TODO: Implement retry logic on lost wifi connection */

    /*
     * if a key other than the default is used, need to set
     * set it in the config structure.  We're using the default, 
     * but will leave this breadcrumb here.
     */
    get_wifi_key(wifi_key, sizeof(wifi_key));
    ESP_LOGI(TAG, "wifi key: <%s>\n", wifi_key);

    mqtt_app_start();

    /*
     * create the slow acquitision task
     */
    xTaskCreate( sensor_acq_slow, "sensor_acq_slow", STACK_SIZE, NULL, tskIDLE_PRIORITY, &xHandle_1 );
    configASSERT( xHandle_1 ); /* check whether the returned handle is NULL */

    /*
     * create the fast acquisition simulation task
     */
    xTaskCreate(fast_acq_sim_task, "fast_acq_sim_task", STACK_SIZE, NULL, FAST_ACQ_PRIO, NULL);
    //xTaskCreatePinnedToCore(fast_acq_sim_task, "fast_acq_sim_task", STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL, 1);

    /*
     * create a task to play out the neopixel example
     * TODO: why was the priority 10 ?
     * Enabled the gpio instrumentation to play out a square wave at 500 Hz.
     * Measured on ucounter was 499.9937.
     * Tried a prio of 10 to see if it might go to exactly 500, but stayed where it was.
     * 
     * ^^^^ABOVE^^^^^
     * Separated the fast acquisition task into a separate task so that the prio could
     * be adjusted separately from the display.  Set the task delay at the bottom of the display
     * task to 0 and the data acquisition square wave instrumentation had more breaks in it.
     * Bumped up the acquisition task prio to 10 to see of a steady square wave could be restored:
     * no change in jitter on acq square wave.  Tried pinned to core and both, still some jitter.
     * 
     * Add back the vTaskDelay at the bottom of the display process (16 mS) reduces the acq jitter significantly.
     * So, I think the bottom line is that the display task, being less critical, needs to be throttled
     * either by a timer or a delay.  Note that the display task has nothing to do a lot (NUMLEDS = 20)
     * due to the low resolution of the display (the code doesn't write a change if the value hasn't
     * changed enough to require it.)
     * 
     */
    xTaskCreate(neopixel_example, "neopixel_example", STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);

#ifdef NOT_YET
    /*
     * experimenting with time of day
     */
    time_t current_time;
    char strftime_buf[64];
    struct tm time_settings;
    
    setenv("TZ", "CST+6", 1);
    tzset();
    localtime_r(&current_time, &time_settings);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &time_settings);
    ESP_LOGI(TAG, "The current date/time is: %s", strftime_buf);
#endif

    while(1)  {
        wifi_connect_status(true);
        msg_id = esp_mqtt_client_publish(get_mqtt_handle(), "esp32/alive", "ping", 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}