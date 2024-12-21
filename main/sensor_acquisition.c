/*
 * sensor_acquisition.c
 * confluence of all api's for access to sensor acquisition and data
 * 
 */

#include "esp_log.h"

#include "sensor_acquisition.h"

static const char *TAG = "sensor_acquisition";  // for logging

/*
 * sensor includes

 * also, provide the storage location for data (exposed through sensors[].data)
 */
#include "htu21d.h"
static float humidity = 0;
static float temperature = 0;

/*
 * initialize all of the sensors.
 * since all are a bit unique in their initialization requirements,
 * just brute force code them serially here.
 */
void sensor_init_slow(void)  {
    int   reterr = HTU21D_ERR_OK;

    if((reterr = htu21d_init(I2C_NUM_0, 21, 22,  GPIO_PULLUP_ONLY,  GPIO_PULLUP_ONLY)) == HTU21D_ERR_OK)
        ESP_LOGI(TAG, "HTU21D init OK\n");
    else
        ESP_LOGI(TAG, "HTU21D init returned error code %d\n", reterr);
}

/*
 * structure to manage acquisition and storage of sensor value
 *  acq function               data storage            data type   label                 mqtt topic           acq?  pub?   disp?  valid?
 */
sensor_data_t sensors[] =  {
  { ht21d_acquire_humidity,    (void *)(&humidity),    PARM_FLOAT, "HTU21D humidity",    "esp32/humidity",    true, false, false, false },
  { ht21d_acquire_temperature, (void *)(&temperature), PARM_FLOAT, "HTU21D temperature", "esp32/temperature", true, false, false, false },
  { NULL, (void *)(0), PARM_UND, "end of sensors", "", false, false, false, false },
};

void acquire_sensors(void)  {
  int i = 0;
  int ret = 0;

  while(sensors[i].acq_fcn != NULL)  {
    if(sensors[i].slow_acq == true)  {
      ret = sensors[i].acq_fcn(sensors[i].data);
      ESP_LOGI(TAG, "%s acquire returned %s", sensors[i].label, (ret ? "success" : "fail" ));
      i++;
    }
  }

}

void display_sensors(void)  {
  int i = 0;

  while(sensors[i].acq_fcn != NULL)  {
    switch(sensors[i].data_type) {

      case PARM_INT:
          ESP_LOGI(TAG, "%s =  %d", sensors[i].label, *((int *)(sensors[i].data)));
      break;

      case PARM_FLOAT:
          ESP_LOGI(TAG, "%s =  %f", sensors[i].label, *((float *)(sensors[i].data)));
      break;

      case PARM_BOOL:
          ESP_LOGI(TAG, "%s =  %d", sensors[i].label, *((bool *)(sensors[i].data)));
      break;

      case PARM_STRING:
          ESP_LOGI(TAG, "%s =  %s", sensors[i].label, (char *)(sensors[i].data));
      break;

      default:
          ESP_LOGI(TAG, "Error, can't display undefined sensor");
      break;

    }
    i++;
  }
}
