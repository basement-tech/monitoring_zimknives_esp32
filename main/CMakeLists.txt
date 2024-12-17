idf_component_register(SRCS "mqtt_local.c" "wifi_station.c" "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi
                    REQUIRES nvs_flash
                    REQUIRES mqtt)
