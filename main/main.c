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

#define DISPLAY_NEOPIXEL_MODE EXCEL_COLOR_VALUE
#define DISPLAY_NEOPIXEL_SPEED (1000 / portTICK_PERIOD_MS)  // mS between strip updates

#define LED_BARGRAPH_MAX 100
#define LED_BARGRAPH_MIN 0
int32_t led_bargraph_value = LED_BARGRAPH_MIN;
static void led_bargraph_incr(void)  {
  led_bargraph_value += 10;
  if(led_bargraph_value > LED_BARGRAPH_MAX)
    led_bargraph_value = LED_BARGRAPH_MIN;
}

static void neopixel_example(void *pvParameters)
{
    /* Configure the peripheral according to the LED type */
    configure_led();

    if(DISPLAY_NEOPIXEL_MODE == EXCEL_COLOR_VALUE)  {
      led_bargraph_min_set(0);
      led_bargraph_max_set(100);
    }
    while(1)
    {
        if(DISPLAY_NEOPIXEL_MODE == EXCEL_COLOR_VALUE)
          led_bargraph_incr();
        display_neopixel_update(DISPLAY_NEOPIXEL_MODE, led_bargraph_value);
        vTaskDelay(DISPLAY_NEOPIXEL_SPEED);  // set speed of neopixel chase here and give IDLE() time to run
    }
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
     * create a task to play out the neopixel example
     * TODO: why was the priority 10 ?
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