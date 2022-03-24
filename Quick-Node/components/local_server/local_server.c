#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_vfs.h"
#include "esp_err.h"
#include "esp_log.h"
#include "http_parser.h"
#include "cJSON.h"

#include "keep_alive.h"

#include "local_server.h"
#include "uart_controller.h"

char *TAG3 = "LOCAL SERVER TASK";

//------------- TASK FORMATION -------------//
extern EventGroupHandle_t task_formation_event_group;
#define MANAGER_SYNC_BIT 			(1 << 0)
#define WIFI_CONTROLLER_INIT_BIT 	(1 << 1)
#define LOCAL_SERVER_INIT_BIT 		(1 << 2)
#define UART_CONTROLLER_INIT_BIT	(1 << 3)
#define ALL_SYNC_BITS 				( MANAGER_SYNC_BIT | WIFI_CONTROLLER_INIT_BIT | LOCAL_SERVER_INIT_BIT | UART_CONTROLLER_INIT_BIT)

//------------- UART QUEUE -----------------//
extern xQueueHandle uartQueue;
extern xQueueHandle uartCommandQueue;
char* ReceiveBuffer;
//***************************** WEBSOCKET CONFIGURATION ****************//
TaskHandle_t keep_alive_task_handler;
static const size_t max_clients = CONFIG_WS_MAX_CONN;

/*
 * Structure holding server handle
 * and internal socket fd in order
 * to use out of request send
 */
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};


//static void data_frame(void *arg)
//{
//    struct async_resp_arg *resp_arg = arg;
//    httpd_handle_t hd = resp_arg->hd;
//    int fd = resp_arg->fd;
//    httpd_ws_frame_t ws_pkt;
//    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
//    ws_pkt.payload = (uint8_t *)resp_arg->data;
//    ws_pkt.len = strlen(resp_arg->data);
//    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
//
//    esp_err_t resp = httpd_ws_send_frame_async(hd, fd, &ws_pkt);
//	if(resp == ESP_OK){
//
//		free(resp_arg->data);
//		free(resp_arg);
//	}
//}

//esp_err_t ws_open_fd(httpd_handle_t hd, int sockfd)
//{
//    ESP_LOGI(TAG3, "New client connected %d", sockfd);
//    xEventGroupSetBits(local_server_event_group,START_WS_ASYNC_DATA);
//    return ESP_OK;
//}
//
//void ws_close_fd(httpd_handle_t hd, int sockfd)
//{
//    ESP_LOGI(TAG3, "Client disconnected %d", sockfd);
//    xEventGroupSetBits(local_server_event_group,STOP_WS_ASYNC_DATA);
//}

static esp_err_t ws_connect_handler(httpd_req_t *req)
{
	//----------- INITIAL HANDSHAKE ----------//
	if (req->method == HTTP_GET) {
	        ESP_LOGI(TAG3, "Handshake done, the new connection was opened");
	        return ESP_OK;
	}
	//----------- BUFFER CONFIGURATION FOR RECIEVING INCOMING DATA --------------//
	httpd_ws_frame_t ws_pkt;
	uint8_t *buf = NULL;
	memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
	ws_pkt.type = HTTPD_WS_TYPE_TEXT;

	/* Set max_len = 0 to get the frame len */
	esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG3, "httpd_ws_recv_frame failed to get frame len with %d", ret);
		return ret;
	}
	ESP_LOGI(TAG3, "frame len is %d", ws_pkt.len);

	if (ws_pkt.len) {
		/* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
		buf = calloc(1, ws_pkt.len + 1);
		if (buf == NULL) {
			ESP_LOGE(TAG3, "Failed to calloc memory for buf");
			return ESP_ERR_NO_MEM;
		}
		ws_pkt.payload = buf;
		/* Set max_len = ws_pkt.len to get the frame payload */
		//---------- RECIEVING THE INCOMING DATA -----------//
		ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
		if (ret != ESP_OK) {
			ESP_LOGE(TAG3, "httpd_ws_recv_frame failed with %d", ret);
			free(buf);
			return ret;
		}
		ESP_LOGI(TAG3, "Got packet with message: %s", ws_pkt.payload);
		char buf[(size_t)ws_pkt.len];
		memcpy(buf,ws_pkt.payload,ws_pkt.len);
		cJSON *root = cJSON_Parse(buf);
		char* command = cJSON_GetObjectItem(root, "command")->valuestring;
		ESP_LOGI(TAG3, "Got command: %s", command);
		//------------- HANDLING COMMANDS -----------//
		ESP_LOGI(TAG3, "Packet type: %d", ws_pkt.type);
		if (ws_pkt.type == HTTPD_WS_TYPE_TEXT && strcmp(command,"harvestShotPoint") == 0) {
			cJSON *parameters = cJSON_GetObjectItem(root, "params");
			char * time = cJSON_GetArrayItem(parameters, 0)->valuestring;
			char *sec_counter = cJSON_GetArrayItem(parameters, 1)->valuestring;
			char *record_length = cJSON_GetArrayItem(parameters, 2)->valuestring;
			xEventGroupSetBits(uart_controller_event_group,SEND_COMMAND);
			char command[1][50];
			sprintf(command[0],"p%s,%s,%s,3,-10000,0,2.0,3,-20000,0,2.0,3",time,sec_counter,record_length);
			ESP_LOGI(TAG3, "shot command: %s",command[0]);
			xQueueSend(uartCommandQueue,(void *)&command[0], pdMS_TO_TICKS(10));
		}
		else if (ws_pkt.type == HTTPD_WS_TYPE_TEXT && strcmp(command,"testMode") == 0) {
			xEventGroupSetBits(uart_controller_event_group,SEND_COMMAND);
			char command[1][50];
			strcpy(command[0],"t");
			ESP_LOGI(TAG3, "command: %s",command[0]);
			xQueueSend(uartCommandQueue,(void *)&command[0], pdMS_TO_TICKS(10));
		}
		else if (ws_pkt.type == HTTPD_WS_TYPE_TEXT && strcmp(command,"enableAsync") == 0) {
			ESP_LOGI(TAG3, "Received COMMAND: %s",(char*)ws_pkt.payload);
			xEventGroupSetBits(uart_controller_event_group,START_RECIEVING_DATA);
			xEventGroupSetBits(local_server_event_group,START_WS_ASYNC_DATA);
		}
		else if (ws_pkt.type == HTTPD_WS_TYPE_TEXT && strcmp(command,"disableAsync") == 0) {
			ESP_LOGI(TAG3, "Received COMMAND: %s",(char*)ws_pkt.payload);
			xEventGroupSetBits(uart_controller_event_group,SEND_COMMAND);
			char command[1][50];
			strcpy(command[0],"r");
			ESP_LOGI(TAG3, "command: %s",command[0]);
			xQueueSend(uartCommandQueue,(void *)&command[0], pdMS_TO_TICKS(10));
			xEventGroupSetBits(uart_controller_event_group,STOP_RECIEVING_DATA);
			xEventGroupSetBits(local_server_event_group,STOP_WS_ASYNC_DATA);
		}
	}
	free(buf);
	return ret;
}

// Get all clients and send async message
void ws_async_data_task(void *pvParameters)
{
	httpd_handle_t* server = (httpd_handle_t *)pvParameters;
    // Send async message to all connected clients that use websocket protocol every 10 seconds
    while (true) {
    	size_t clients = max_clients;
		int    client_fds[max_clients];

		httpd_get_client_list(*server, &clients, client_fds);
		if (clients > 0) {
			for (size_t i=0; i < clients; ++i) {
				int sock = client_fds[i];
				if (httpd_ws_get_fd_info(*server, sock) == HTTPD_WS_CLIENT_WEBSOCKET) {
					ESP_LOGI(TAG3, "Active client  -> (fd=%d)", sock);
					struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
					resp_arg->hd = *server;
					resp_arg->fd = sock;
					httpd_ws_frame_t ws_pkt;
					memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
					char * rxBuffer = (char*) malloc(BUF_SIZE+1);
					if(( xQueueReceive(uartQueue,(void*)rxBuffer,portMAX_DELAY) == pdPASS )){
						ESP_LOGI(TAG3,"NODE ---<<<< %s",rxBuffer);
						ws_pkt.payload = (uint8_t *)rxBuffer;
						ws_pkt.len = strlen(rxBuffer);
						ws_pkt.type = HTTPD_WS_TYPE_TEXT;
						esp_err_t resp = httpd_ws_send_frame_async(resp_arg->hd, resp_arg->fd, &ws_pkt);
						if(resp == ESP_OK){
							free(rxBuffer);
							free(resp_arg);
						}
					}
					vTaskDelay(pdMS_TO_TICKS(10));
				}
			}
		}
//		else {
//			ESP_LOGE(TAG3, "No client connected");
//			vTaskDelete(ws_async_data_task_handler);
//			ws_async_data_task_handler = NULL;
//		}
		vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static httpd_handle_t start_server()
{
	// Start the httpd server
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.max_open_sockets = 1;
	config.lru_purge_enable = true;
//	config.open_fn = ws_open_fd;
//	config.close_fn = ws_close_fd;

    //------------ WEBSOCKET ENDPOINTS -----------//
	static const httpd_uri_t connect = {
	   .uri        = "/connect",
	   .method     = HTTP_GET,
	   .handler    = ws_connect_handler,
	   .user_ctx   = NULL,
	   .is_websocket = true
	};
	if (httpd_start(&server, &config) != ESP_OK) {
		ESP_LOGI(TAG3, "Error starting server!");
		return NULL;
	}
	httpd_register_uri_handler(server, &connect);
	ESP_LOGI(TAG3, "Registered WS server URI handlers.");
	return server;

}

static void stop_server(httpd_handle_t server)
{
	// Stop keep alive thread
	httpd_stop(server);
}


//**************** LOCAL SERVER CONFIGURATION ******************//
static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG3, "Stopping webserver");
        stop_server(*server);
        *server = NULL;
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG3, "Starting webserver");
        *server = start_server();
    }

}

void local_server_task(void *pvParameters)
{
	static httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &disconnect_handler, &server));
    ESP_LOGI(TAG3,"-----------------------");
    ESP_LOGI(TAG3,"LOCAL SERVER INITIALIZED");
    ESP_LOGI(TAG3,"-----------------------");
    xEventGroupSync(task_formation_event_group,(EventBits_t) pvParameters,ALL_SYNC_BITS,portMAX_DELAY);
    local_server_event_group = xEventGroupCreate();
    while (true)
    {
        EventBits_t bits = xEventGroupWaitBits(local_server_event_group,
        										START_WS_ASYNC_DATA | STOP_WS_ASYNC_DATA,
                                                pdFALSE,
                                                pdFALSE,
                                                portMAX_DELAY);

        if (bits & START_WS_ASYNC_DATA) {
			//----- Clear flag-------//
			xEventGroupClearBits(local_server_event_group,START_WS_ASYNC_DATA);
			//----- Code-----//
			if(ws_async_data_task_handler == NULL){
				xTaskCreatePinnedToCore(ws_async_data_task, "ws_async_data_task", 1024*4, (void *)&server, 3, &ws_async_data_task_handler,1);
			}
		}
		else if (bits & STOP_WS_ASYNC_DATA) {
			//----- Clear flag-------//
			xEventGroupClearBits(local_server_event_group,STOP_WS_ASYNC_DATA);
			//----- Code-----//
			if(ws_async_data_task_handler != NULL){
				vTaskDelete(ws_async_data_task_handler);
				ws_async_data_task_handler = NULL;
			}
		}
        else {
            ESP_LOGE(TAG3, "UNEXPECTED COMMAND");
        }
    }
}
