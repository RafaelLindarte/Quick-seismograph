idf_component_register(SRCS "local_server.c" "keep_alive.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_http_server uart_controller json)
