idf_component_register(SRCS "htu21d.c" "mqtt_local.c" "wifi_station.c" "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi
                    REQUIRES nvs_flash
                    REQUIRES mqtt
                    REQUIRES driver)
