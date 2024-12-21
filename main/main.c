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

#include "monitoring_zimknives.h"

/*
 * sensor includes
 */
#include "htu21d.h"

static const char *TAG = "main";

#define LTAG "slow_acq"
/*
 * structure to manage acquisition and storage of sensor values
 *
 * sensor api's must provide a simple function (acq_fcn) that acquires their
 * data value and places it in the pointer provided as an argument.
 * acq_fcn must return a simple 1 for success, 0 for failure.
 */
typedef int (*acquisition_function_t)(void *data);

/*
 * types of sensor data
 */
#define PARM_UND   -1  /* undefined */
#define PARM_INT    0
#define PARM_FLOAT  1
#define PARM_BOOL   2
#define PARM_STRING 3
typedef struct {
  acquisition_function_t  acq_fcn;    // pointer to function to cause data acquisition
  void *data;     // pointer to actual data value
  uint8_t  data_type; // type of data
  char *label;    // human readable label
  char *topic;    // topic for mqtt publish
  bool slow_acq;  // whether to acquire on the slow loop
  bool publish;   // whether to publish this sensors result
  bool display;   // whether to display for actions that care
  bool valid;     // set true if data acquisition is successful
} sensor_data_t;

static float humidity = 0;
static float temperature = 0;

sensor_data_t sensors[] =  {
  { ht21d_acquire_humidity, (void *)(&humidity), PARM_FLOAT, "HTU21D humidity", "esp32/humidity", true, false, false, false },
  { ht21d_acquire_temperature, (void *)(&temperature), PARM_FLOAT, "HTU21D temperature", "esp32/temperature", true, false, false, false },
  { NULL, (void *)(0), PARM_UND, "end of sensors", "", false, false, false, false },
};

void acquire_sensors(void)  {
  int i = 0;
  int ret = 0;

  while(sensors[i].acq_fcn != NULL)  {
    if(sensors[i].slow_acq == true)  {
      ret = sensors[i].acq_fcn(sensors[i].data);
      ESP_LOGI(LTAG, "%s acquire returned %s", sensors[i].label, (ret ? "success" : "fail" ));
      i++;
    }
  }

}

void display_sensors(void)  {
  int i = 0;

  while(sensors[i].acq_fcn != NULL)  {
    switch(sensors[i].data_type) {

      case PARM_INT:
          ESP_LOGI(LTAG, "%s =  %d", sensors[i].label, *((int *)(sensors[i].data)));
      break;

      case PARM_FLOAT:
          ESP_LOGI(LTAG, "%s =  %f", sensors[i].label, *((float *)(sensors[i].data)));
      break;

      case PARM_BOOL:
          ESP_LOGI(LTAG, "%s =  %d", sensors[i].label, *((bool *)(sensors[i].data)));
      break;

      case PARM_STRING:
          ESP_LOGI(LTAG, "%s =  %s", sensors[i].label, (char *)(sensors[i].data));
      break;

      default:
          ESP_LOGI(LTAG, "Error, can't display undefined sensor");
      break;

    }
    i++;
  }
}

/*
 * slow acquisition task
 * acquires data from slower (~1s) sensors
 * espect an include file for each sensor here
 * 
 * sensor_init_slow() : initialize the sensors that will use the slow acq loop
 * sensor_acq_slow()  : read the slow acq loop sensors
 */



#define STACK_SIZE 4096
TaskHandle_t xHandle_1 = NULL;

void sensor_init_slow(void)  {
    int   reterr = HTU21D_ERR_OK;

    if((reterr = htu21d_init(I2C_NUM_0, 21, 22,  GPIO_PULLUP_ONLY,  GPIO_PULLUP_ONLY)) == HTU21D_ERR_OK)
        ESP_LOGI(LTAG, "HTU21D init OK\n");
    else
        ESP_LOGI(LTAG, "HTU21D init returned error code %d\n", reterr);
}
void sensor_acq_slow(void *pvParameters)  {
  float temp;

  sensor_init_slow();

  while(1)  {
    ESP_LOGI(LTAG, "slow acquisition initiated\n");
    ESP_LOGI(LTAG, "sensor_acq_slow(): executing on core %d\n", xPortGetCoreID());

    acquire_sensors();

    display_sensors();

    vTaskDelay(SLOW_LOOP_INTERVAL / portTICK_PERIOD_MS);
  }
}


/*
 * main task:
 * start wifi
 * start mqtt
 * loop and publish on slower cycle
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