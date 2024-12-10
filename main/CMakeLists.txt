idf_component_register(SRCS "wifi_station.c" "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi
                    REQUIRES nvs_flash)
