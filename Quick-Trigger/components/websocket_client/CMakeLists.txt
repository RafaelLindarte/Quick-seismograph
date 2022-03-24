idf_component_register(SRCS "websocket_client.c"
                    INCLUDE_DIRS "include"
                    REQUIRES nvs_flash esp_websocket_client wifi_controller local_server json)

