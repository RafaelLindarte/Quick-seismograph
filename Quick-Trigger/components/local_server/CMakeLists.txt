idf_component_register(SRCS "local_server.c" "keep_alive.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_http_server spiffs json wifi_controller websocket_client uart_controller)
set(WEB_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../public")
if(EXISTS ${WEB_SRC_DIR})
	spiffs_create_partition_image(storage ${WEB_SRC_DIR} FLASH_IN_PROJECT)
else()
	message(FATAL_ERROR "${WEB_SRC_DIR}/dist doesn't exit.")
endif()