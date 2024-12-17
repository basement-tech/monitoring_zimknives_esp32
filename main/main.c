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

#include "wifi_station.h"

#include "mqtt_local.h"




void app_main(void)
{
  char wifi_key[16];

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
    ESP_LOGI("main", "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    //esp_wifi_connect();  /* apparently not required */
    /* TODO: Implement retry logic on lost wifi connection */

    /*
     * if a key other than the default is used, need to set
     * set it in the config structure.  We're using the default, 
     * but will leave this breadcrumb here.
     */
    get_wifi_key(wifi_key, sizeof(wifi_key));
    ESP_LOGI("main", "wifi key: <%s>\n", wifi_key);

    mqtt_app_start();

    while(1)  {
        wifi_connect_status(true);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}