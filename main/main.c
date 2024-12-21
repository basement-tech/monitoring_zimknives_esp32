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
    ESP_LOGI(TAG, "slow acquisition initiated\n");
    ESP_LOGI(TAG, "sensor_acq_slow(): executing on core %d\n", xPortGetCoreID());

    acquire_sensors();
    display_sensors();

    vTaskDelay(SLOW_LOOP_INTERVAL / portTICK_PERIOD_MS);
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

    while(1)  {
        wifi_connect_status(true);
        msg_id = esp_mqtt_client_publish(get_mqtt_handle(), "esp32/alive", "ping", 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}