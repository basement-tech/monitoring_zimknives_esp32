/* 
 * mqtt_local.h   provide interface between esp-idf esp_mqtt calls and application
 * based on MQTT (over TCP) Example from esp-idf
 *
 * This code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */
#ifndef __MQTT_LOCAL_H__

#include "mqtt_client.h"

void mqtt_app_start(void);
esp_mqtt_client_handle_t get_mqtt_handle(void);

#define __MQTT_LOCAL_H__
#endif