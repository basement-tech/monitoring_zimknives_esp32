#ifndef __MQTT_LOCAL_H__

#include "mqtt_client.h"

void mqtt_app_start(void);
esp_mqtt_client_handle_t get_mqtt_handle(void);

#define __MQTT_LOCAL_H__
#endif