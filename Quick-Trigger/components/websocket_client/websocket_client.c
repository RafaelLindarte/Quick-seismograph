#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "cJSON.h"

#include "local_server.h"
#include "wifi_controller.h"
#include "websocket_client.h"

static const char *TAG4 = "WEBSOCKET CLIENT TASK";

//------------- TASK FORMATION -------------//
extern EventGroupHandle_t task_formation_event_group;
#define MANAGER_SYNC_BIT 			(1 << 0)
#define WIFI_CONTROLLER_INIT_BIT 	(1 << 1)
#define LOCAL_SERVER_INIT_BIT 		(1 << 2)
#define WEBSOCKET_CLIENT_INIT_BIT   (1 << 3)
#define WEBSOCKET_CLIENT_END_BIT    (1 << 4)
#define UART_CONTROLLER_INIT_BIT	(1 << 5)
#define ALL_SYNC_BITS 				( MANAGER_SYNC_BIT | WIFI_CONTROLLER_INIT_BIT | LOCAL_SERVER_INIT_BIT | UART_CONTROLLER_INIT_BIT)

//------------- WEBSOCKET CLIENT -----------------//
xQueueHandle websocketDataQueue;
xQueueHandle websocketCommandQueue;
static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG4, "WEBSOCKET_EVENT_CONNECTED");
        vTaskDelay(pdMS_TO_TICKS(1000));
        xEventGroupSetBits(local_server_event_group,START_NODE_WS_ASYNC_DATA);
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG4, "WEBSOCKET_EVENT_DISCONNECTED");
        xEventGroupSetBits(websocket_client_event_group, DESTROY_CLIENT);
        break;
    case WEBSOCKET_EVENT_DATA:
        ESP_LOGI(TAG4, "WEBSOCKET_EVENT_DATA");
        ESP_LOGI(TAG4, "Received opcode=%d", data->op_code);
        if (data->op_code == 0x01) {
//            ESP_LOGW(TAG4, "Received: Len= %d, Data=%s",data->data_len,data->data_ptr);
            char* dataRecievedFromClient = (char*) malloc(data->data_len);//+1
            memcpy(dataRecievedFromClient,data->data_ptr,data->data_len);
            dataRecievedFromClient = (char *) realloc(dataRecievedFromClient,data->data_len+1);
            dataRecievedFromClient[data->data_len]='\0';
//            ESP_LOGI(TAG4, "Data 2: %s %d",dataRecievedFromClient,data->data_len);//+1
            xQueueSend(websocketDataQueue,(void *)dataRecievedFromClient, pdMS_TO_TICKS(10));
//            vTaskDelay(pdMS_TO_TICKS(100));
            free(dataRecievedFromClient);
        }
        if (data->op_code == 0x0A) {
        	cJSON *pong_commando_obj = cJSON_CreateObject();
        	cJSON_AddItemToObject(pong_commando_obj,"command",cJSON_CreateString("idle"));
			char * pong_flag = cJSON_Print(pong_commando_obj);
//            ESP_LOGI(TAG4, "Data 2: %s %d",dataRecievedFromClient,data->data_len);//+1
			xQueueSend(websocketDataQueue,(void *)pong_flag, pdMS_TO_TICKS(10));
//            vTaskDelay(pdMS_TO_TICKS(100));
		}
        break;
    case WEBSOCKET_EVENT_CLOSED:
    	ESP_LOGI(TAG4, "WEBSOCKET_EVENT_CLOSED");
    	if(node_ws_async_data_task_handler != NULL){
        	xEventGroupSetBits(local_server_event_group,STOP_NODE_WS_ASYNC_DATA);
    	}
    	break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGI(TAG4, "WEBSOCKET_EVENT_ERROR");
        break;
    }
}

void websocket_client_task(void *pvParameters)
{
	esp_websocket_client_config_t websocket_cfg = {};
	websocket_cfg.buffer_size = WEBSOCKET_BUF_SIZE;
    websocket_cfg.uri = CONFIG_WEBSOCKET_URI;
    websocket_cfg.disable_auto_reconnect = true;
    websocket_cfg.ping_interval_sec = 3;
    esp_websocket_client_handle_t client  = NULL;
    websocketCommandQueue = xQueueCreate(1,150);
    websocketDataQueue = xQueueCreate(20,WEBSOCKET_BUF_SIZE+1);
    ESP_LOGI(TAG4,"----------------------------");
	ESP_LOGI(TAG4,"WEBSOCKET CLIENT INITIALIZED");
	ESP_LOGI(TAG4,"----------------------------");
	xEventGroupSync(task_formation_event_group,(EventBits_t) pvParameters,ALL_SYNC_BITS,portMAX_DELAY);
    websocket_client_event_group = xEventGroupCreate();
	while (true)
	{
		EventBits_t bits = xEventGroupWaitBits(websocket_client_event_group,
												CONNECT_CLIENT | DISCONNECT_CLIENT | CLIENT_STATUS | SET_SERVER_URI | SEND_DATA | SEND_DATA_BIN | SEND_DATA_TXT | DESTROY_CLIENT,
												pdFALSE,
												pdFALSE,
												portMAX_DELAY);

		if (bits & CONNECT_CLIENT) {
			//----- Clear flag -------//
			xEventGroupClearBits(websocket_client_event_group,CONNECT_CLIENT);
			//----- Code-------//
			client = esp_websocket_client_init(&websocket_cfg);
			esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)client);
			ESP_LOGI(TAG4, "Connecting to %s...", websocket_cfg.uri);
			esp_err_t web_socket_status = esp_websocket_client_start(client);
			if(web_socket_status == ESP_OK){
				ESP_LOGI(TAG4, "Websocket Started Success");
		        xEventGroupSetBits(websocket_client_event_group,CLIENT_STARTED);
			}
		}
		else if (bits & DISCONNECT_CLIENT) {
			//----- Clear flag -------//
			xEventGroupClearBits(websocket_client_event_group,DISCONNECT_CLIENT);
			//----- Code-------//
			esp_websocket_client_close(client, portMAX_DELAY);
			ESP_LOGI(TAG4, "Websocket Stopped");
			esp_websocket_client_destroy(client);
			client = NULL;
		}
		if (bits & CLIENT_STATUS) {
			//----- Clear flag -------//
			xEventGroupClearBits(websocket_client_event_group,CLIENT_STATUS);
			//----- Code-------//
			esp_websocket_client_is_connected(client);
		}
		else if (bits & SET_SERVER_URI) {
			//----- Clear flag -------//
			xEventGroupClearBits(websocket_client_event_group,SET_SERVER_URI);
			//----- Code-------//
			esp_websocket_client_set_uri(client,CONFIG_WEBSOCKET_URI);
		}
		else if (bits & SEND_DATA) {
			//----- Clear flag -------//
			xEventGroupClearBits(websocket_client_event_group,SEND_DATA);
			//----- Code-------//
			char data[32];
			if (esp_websocket_client_is_connected(client)) {
				int len = sprintf(data, "hello");
				ESP_LOGI(TAG4, "Sending %s", data);
				esp_websocket_client_send(client, data, len, portMAX_DELAY);
			}
		}
		else if (bits & SEND_DATA_BIN) {
			//----- Clear flag -------//
			xEventGroupClearBits(websocket_client_event_group,SEND_DATA_BIN);
			//----- Code-------//
			char data[32];
			if (esp_websocket_client_is_connected(client)) {
				int len = sprintf(data, "hello bin");
				ESP_LOGI(TAG4, "Sending binary %s", data);
				esp_websocket_client_send_bin(client, data, len, portMAX_DELAY);
			}
		}
		else if (bits & SEND_DATA_TXT) {
			//----- Clear flag -------//
			xEventGroupClearBits(websocket_client_event_group,SEND_DATA_TXT);
			//----- Code-------//
			char command[150];
			size_t n = sizeof(command)/sizeof(command[0]);
			xQueueReceive(websocketCommandQueue,(void*)&command,portMAX_DELAY);
			ESP_LOGI(TAG4, "Sending text %s", command);
			esp_websocket_client_send_text(client, command, n, portMAX_DELAY);
		}
		else if (bits & DESTROY_CLIENT) {
			//----- Clear flag -------//
			xEventGroupClearBits(websocket_client_event_group,DESTROY_CLIENT);
			//----- Code-------//
			esp_websocket_client_destroy(client);
		}
	}
}

