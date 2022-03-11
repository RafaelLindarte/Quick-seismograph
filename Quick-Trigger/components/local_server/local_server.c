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
#include "esp_spiffs.h"
#include "http_parser.h"
#include "cJSON.h"
#include "keep_alive.h"

#include "local_server.h"
#include "wifi_controller.h"
#include "websocket_client.h"
#include "uart_controller.h"
char *TAG3 = "LOCAL SERVER TASK";

//------------- TASK FORMATION -------------//
extern EventGroupHandle_t task_formation_event_group;
#define MANAGER_SYNC_BIT 			(1 << 0)
#define WIFI_CONTROLLER_INIT_BIT 	(1 << 1)
#define LOCAL_SERVER_INIT_BIT 		(1 << 2)
#define WEBSOCKET_CLIENT_INIT_BIT 	(1 << 3)
#define WEBSOCKET_CLIENT_END_BIT 	(1 << 4)
#define UART_CONTROLLER_INIT_BIT	(1 << 5)
#define ALL_SYNC_BITS 				( MANAGER_SYNC_BIT | WIFI_CONTROLLER_INIT_BIT | LOCAL_SERVER_INIT_BIT | UART_CONTROLLER_INIT_BIT)

//-------------WIFI CONTROLLER QUEUE's-----------//

extern xQueueHandle networksQueue;
extern xQueueHandle connectQueue;
extern xQueueHandle websocketCommandQueue;
extern xQueueHandle websocketDataQueue;
extern xQueueHandle uartQueue;
extern xQueueHandle uartCommandQueue;
//************************** INITIALIZE FILE SYSTEM PARTITION *************//
esp_err_t init_fs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = CONFIG_HTTP_SERVER_WEB_MOUNT_POINT,
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = false
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG3, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG3, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG3, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG3, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG3, "Partition size: total: %d, used: %d", total, used);
    }
    return ESP_OK;
}


//***************************** WEBSOCKET CONFIGURATION ****************//

static const size_t max_clients = CONFIG_WS_MAX_CONN;

/*
 * Structure holding server handle
 * and internal socket fd in order
 * to use out of request send
 */
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
    char * data;
};


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

//
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
//    if(resp == ESP_OK){
//    	free(resp_arg->data);
//    	free(resp_arg);
//    }
//}


static esp_err_t trigger_ws_connect_handler(httpd_req_t *req)
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
		if (ws_pkt.type == HTTPD_WS_TYPE_TEXT && strcmp(command,"newShotPoint") == 0) {
			cJSON *parameters = cJSON_GetObjectItem(root, "params");
			char *record_length = cJSON_GetArrayItem(parameters, 0)->valuestring;
			char *sensitivity = cJSON_GetArrayItem(parameters, 1)->valuestring;
			char *shot_point = cJSON_GetArrayItem(parameters, 2)->valuestring;
			xEventGroupSetBits(uart_controller_event_group,SEND_COMMAND_SHOT_POINT);
			char command[1][9];
			sprintf(command[0],"p%s,%s,%s",record_length,sensitivity,shot_point);
			xQueueSend(uartCommandQueue,(void *)&command[0], pdMS_TO_TICKS(10));
			xEventGroupSetBits(uart_controller_event_group,START_RECIEVING_DATA);
		}
		else if(ws_pkt.type == HTTPD_WS_TYPE_TEXT && strcmp(command,"finishShotPoint") == 0){
			xEventGroupSetBits(uart_controller_event_group,SEND_COMMAND_SHOT_POINT);
			char command[1][9];
			strcpy(command[0],"r\0");
			xQueueSend(uartCommandQueue,(void *)&command[0], pdMS_TO_TICKS(10));
			xEventGroupSetBits(uart_controller_event_group,STOP_RECIEVING_DATA);
		}
		else if (ws_pkt.type == HTTPD_WS_TYPE_TEXT && strcmp(command,"enableAsync") == 0) {
			ESP_LOGI(TAG3, "enabled");
			xEventGroupSetBits(local_server_event_group,START_TRIGGER_WS_ASYNC_DATA);
		}
		else if (ws_pkt.type == HTTPD_WS_TYPE_TEXT && strcmp(command,"disableAsync") == 0) {
			ESP_LOGI(TAG3, "Disabled");
			xEventGroupSetBits(uart_controller_event_group,STOP_RECIEVING_DATA);
			xEventGroupSetBits(local_server_event_group,STOP_TRIGGER_WS_ASYNC_DATA);
		}
	}
	free(buf);
	return ret;
}
static esp_err_t node_ws_connect_handler(httpd_req_t *req)
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
			xEventGroupSetBits(websocket_client_event_group, SEND_DATA_TXT);
			char command[1][150];
			strcpy(command[0],buf);
			xQueueSend(websocketCommandQueue,(void *)&command[0], pdMS_TO_TICKS(10));
		}
		else if (ws_pkt.type == HTTPD_WS_TYPE_TEXT && strcmp(command,"testMode") == 0) {
			ESP_LOGI(TAG3, "Received COMMAND: %s",(char*)ws_pkt.payload);
			xEventGroupSetBits(websocket_client_event_group, SEND_DATA_TXT);
			char command[1][20];
			strncpy(command[0],"testNode",20);
			xQueueSend(websocketCommandQueue,(void *)&command[0], pdMS_TO_TICKS(10));

		}
		else if (ws_pkt.type == HTTPD_WS_TYPE_TEXT && strcmp(command,"enableAsync") == 0) {
			ESP_LOGI(TAG3, "Received COMMAND: %s",(char*)ws_pkt.payload);
			xEventGroupSetBits(websocket_client_event_group, SEND_DATA_TXT);
			char command[1][150];
			strcpy(command[0],buf);
			xQueueSend(websocketCommandQueue,(void *)&command[0], pdMS_TO_TICKS(100));
			ret = httpd_ws_send_frame(req, &ws_pkt);
		}
		else if (ws_pkt.type == HTTPD_WS_TYPE_TEXT && strcmp(command,"disableAsync") == 0) {
			ESP_LOGI(TAG3, "Received COMMAND: %s",(char*)ws_pkt.payload);
			xEventGroupSetBits(websocket_client_event_group, SEND_DATA_TXT);
			char command[1][150];
			strcpy(command[0],buf);
			xQueueSend(websocketCommandQueue,(void *)&command[0], pdMS_TO_TICKS(10));
			ret = httpd_ws_send_frame(req, &ws_pkt);
		}
		else{
			ret = httpd_ws_send_frame(req, &ws_pkt);
		}
	}
	free(buf);
	return ret;
}
// Get all clients and send async message
void trigger_ws_async_data_task(void *pvParameters)
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
					resp_arg->data = (char*) malloc(UART_BUF_SIZE+1);
					memset(resp_arg->data,0,UART_BUF_SIZE+1);
					if( ( xQueueReceive(uartQueue,(void*)resp_arg->data,portMAX_DELAY) == pdPASS )){
						ESP_LOGI(TAG3, "TRIGGER  -> %s", resp_arg->data);
//						if (httpd_queue_work(resp_arg->hd, data_frame, resp_arg) != ESP_OK) {
//							ESP_LOGE(TAG3, "httpd_queue_work failed!");
//							break;
//						}
						httpd_ws_frame_t ws_pkt;
						memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
						ws_pkt.payload = (uint8_t *)resp_arg->data;
						ws_pkt.len = strlen(resp_arg->data);
						ws_pkt.type = HTTPD_WS_TYPE_TEXT;

						esp_err_t resp = httpd_ws_send_frame_async(resp_arg->hd, resp_arg->fd, &ws_pkt);
						if(resp == ESP_OK){
							free(resp_arg->data);
							free(resp_arg);
						}
					}
					vTaskDelay(pdMS_TO_TICKS(10));
				}
			}
		}else {
			ESP_LOGE(TAG3, "No client connected");
			vTaskDelete(trigger_ws_async_data_task_handler);
			trigger_ws_async_data_task_handler = NULL;
		}
		vTaskDelay(pdMS_TO_TICKS(10));
    }
}
void node_ws_async_data_task(void *pvParameters)
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
					resp_arg->data = (char*) malloc(WEBSOCKET_BUF_SIZE+1);
					memset(resp_arg->data,0,WEBSOCKET_BUF_SIZE+1);
					if(( xQueueReceive(websocketDataQueue,(void*)resp_arg->data,portMAX_DELAY) == pdPASS )){
						ESP_LOGI(TAG3, "TRIGGER  -> %s", resp_arg->data);
//						if (httpd_queue_work(resp_arg->hd, data_frame, resp_arg) != ESP_OK) {
//							ESP_LOGE(TAG3, "httpd_queue_work failed!");
//							break;
//						}
						httpd_ws_frame_t ws_pkt;
						memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
						ws_pkt.payload = (uint8_t *)resp_arg->data;
						ws_pkt.len = strlen(resp_arg->data);
						ws_pkt.type = HTTPD_WS_TYPE_TEXT;

						esp_err_t resp = httpd_ws_send_frame_async(resp_arg->hd, resp_arg->fd, &ws_pkt);
						if(resp == ESP_OK){
							free(resp_arg->data);
							free(resp_arg);
						}
					}
					vTaskDelay(pdMS_TO_TICKS(10));
				}
			}
		}else {
			ESP_LOGE(TAG3, "No client connected");
			vTaskDelete(node_ws_async_data_task_handler);
			node_ws_async_data_task_handler = NULL;
		}
		vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static httpd_handle_t start_ws_server()
{
    // Prepare keep-alive engine
//	wss_keep_alive_config_t keep_alive_config = KEEP_ALIVE_CONFIG_DEFAULT();
//	keep_alive_config.task_stack_size = 4*1024;
//	keep_alive_config.max_clients = max_clients;
//	keep_alive_config.client_not_alive_cb = client_not_alive_cb;
//	keep_alive_config.check_client_alive_cb = check_client_alive_cb;
//	wss_keep_alive_t keep_alive = wss_keep_alive_start(&keep_alive_config);

	// Start the httpd server
    httpd_handle_t ws_server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 94;
    config.max_uri_handlers = 2;
	config.max_open_sockets = 1;
//	config.open_fn = ws_open_fd;
//	config.close_fn = ws_close_fd;

    ESP_LOGI(TAG3, "Starting WS server");
    esp_err_t ret = httpd_start(&ws_server, &config);
	if (ret != ESP_OK) {
		ESP_LOGI(TAG3, "Error starting server!");
		return NULL;
	}
	//------------ WEBSOCKET ENDPOINTS -----------//
	static const httpd_uri_t trigger_ws_connect = {
	   .uri        = "/ws/connectTrigger",
	   .method     = HTTP_GET,
	   .handler    = trigger_ws_connect_handler,
	   .user_ctx   = NULL,
	   .is_websocket = true
	};
	httpd_register_uri_handler(ws_server, &trigger_ws_connect);

	static const httpd_uri_t node_ws_connect = {
		   .uri        = "/ws/connectNode",
		   .method     = HTTP_GET,
		   .handler    = node_ws_connect_handler,
		   .user_ctx   = NULL,
		   .is_websocket = true
		};
		httpd_register_uri_handler(ws_server, &node_ws_connect);

//	wss_keep_alive_set_user_ctx(keep_alive, ws_server);
	ESP_LOGI(TAG3, "Registered WS server URI handlers.");
	return ws_server;

}

static void stop_ws_server(httpd_handle_t ws_server)
{
	// Stop keep alive thread
//	wss_keep_alive_stop(httpd_get_global_user_ctx(ws_server));
	httpd_stop(ws_server);
}


//************************** HTTP REST SERVER CONFIGURATION **************//

static const char *REST_TAG = "HTTP REST";
#define REST_CHECK(a, str, goto_tag, ...)                                              \
    do                                                                                 \
    {                                                                                  \
        if (!(a))                                                                      \
        {                                                                              \
            ESP_LOGE(REST_TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto goto_tag;                                                             \
        }                                                                              \
    } while (0)

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 512)
#define SCRATCH_BUFSIZE (102400)

typedef struct rest_server_context {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

rest_server_context_t *rest_context;
#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    const char *type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html")) {
        type = "text/html";
    } else if (CHECK_FILE_EXTENSION(filepath, ".js")) {
        type = "application/javascript";
    } else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
        type = "text/css";
    } else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
        type = "image/png";
    } else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
        type = "image/x-icon";
    } else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
        type = "text/xml";
    }
    return httpd_resp_set_type(req, type);
}



/* Simple handler for getting available nodes */
static esp_err_t scan_nodes_get_handler(httpd_req_t *req)
{
	xEventGroupSetBits(wifi_controller_event_group,WIFI_SCAN);
	char RxBuffer[1][256];
	xQueueReceive(networksQueue,(void*)&RxBuffer[0],pdMS_TO_TICKS(3000));
	if(RxBuffer == NULL){
		httpd_resp_set_status(req,HTTPD_500);
		httpd_resp_set_type(req,HTTPD_TYPE_TEXT);
		httpd_resp_sendstr(req, "fail");
		return ESP_OK;
	}
	httpd_resp_set_status(req,HTTPD_200);
	httpd_resp_set_type(req,HTTPD_TYPE_TEXT);
    httpd_resp_sendstr(req,RxBuffer[0]);
    return ESP_OK;
}

/* Simple handler for connecting to a node */
static esp_err_t connect_node_post_handler(httpd_req_t *req)
{
	int total_len = req->content_len;
	int cur_len = 0;
	char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
	int received = 0;
	if (total_len >= SCRATCH_BUFSIZE) {
		/* Respond with 500 Internal Server Error */
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
		return ESP_FAIL;
	}
	while (cur_len < total_len) {
		received = httpd_req_recv(req, buf + cur_len, total_len);
		if (received <= 0) {
			/* Respond with 500 Internal Server Error */
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
			return ESP_FAIL;
		}
		cur_len += received;
	}
	buf[total_len] = '\0';
	cJSON *root = cJSON_Parse(buf);
	char* node_station_ssid = cJSON_GetObjectItem(root, "ssid")->valuestring;
	xEventGroupSetBits(wifi_controller_event_group,WIFI_CONNECT_TO_NETWORK);
	char send_string[1][32];
	strncpy(send_string[0],node_station_ssid,32);
	cJSON_Delete(root);
	xQueueSend(connectQueue,(void *)&send_string[0], pdMS_TO_TICKS(1000));
	EventBits_t wifi_bits = xEventGroupWaitBits(wifi_controller_event_group,WIFI_CONNECTED,pdFALSE,pdFALSE,pdMS_TO_TICKS(3000));
	EventBits_t websocket_bits = xEventGroupWaitBits(websocket_client_event_group,CLIENT_STARTED,pdFALSE,pdFALSE,pdMS_TO_TICKS(3000));
	if ((wifi_bits & WIFI_CONNECTED)^(websocket_bits & CLIENT_STARTED)) {
		//----- Clear flag -------//
		xEventGroupClearBits(wifi_controller_event_group,WIFI_CONNECTED);
		xEventGroupClearBits(websocket_client_event_group,CLIENT_STARTED);
		//----- Code ------//
		httpd_resp_set_status(req,HTTPD_200);
		httpd_resp_set_type(req,HTTPD_TYPE_TEXT);
		httpd_resp_sendstr(req, "ok");
		return ESP_OK;
	}
	else {
		httpd_resp_set_status(req,HTTPD_500);
		httpd_resp_set_type(req,HTTPD_TYPE_TEXT);
		httpd_resp_sendstr(req, "fail");
		return ESP_OK;
	}
}
/* Simple handler for disconnecting from a node */
static esp_err_t disconnect_node_get_handler(httpd_req_t *req)
{
//	xEventGroupSetBits(websocket_client_event_group, SEND_DATA_TXT);
//	char command[1][20];
//	strncpy(command[0],"Deactivate",20);
//	xQueueSend(websocketCommandQueue,(void *)&command[0],pdMS_TO_TICKS(10));
//	vTaskDelay(pdMS_TO_TICKS(500));
	xEventGroupSetBits(websocket_client_event_group, DISCONNECT_CLIENT);
	vTaskDelay(pdMS_TO_TICKS(1000));
	xEventGroupSetBits(wifi_controller_event_group,WIFI_DISCONNECT_FROM_NETWORK);
	EventBits_t bits = xEventGroupWaitBits(wifi_controller_event_group,WIFI_DISCONNECTED,pdFALSE,pdFALSE,pdMS_TO_TICKS(5000));
	if (bits & WIFI_DISCONNECTED) {
		//----- Clear flag
		xEventGroupClearBits(wifi_controller_event_group,WIFI_DISCONNECTED);
		//----- Code ------//
		httpd_resp_set_status(req,HTTPD_200);
		httpd_resp_set_type(req,HTTPD_TYPE_TEXT);
		httpd_resp_sendstr(req, "ok");
		return ESP_OK;
	}
	else {
		httpd_resp_set_status(req,HTTPD_500);
		httpd_resp_set_type(req,HTTPD_TYPE_TEXT);
		httpd_resp_sendstr(req, "fail");
		return ESP_OK;
	}
}

/* Send HTTP response with the contents of the requested file */
static esp_err_t rest_common_get_handler(httpd_req_t *req)
{
	char filepath[FILE_PATH_MAX];
	rest_server_context_t *rest_context = (rest_server_context_t *)req->user_ctx;
	strlcpy(filepath, rest_context->base_path, sizeof(filepath));
	if (req->uri[strlen(req->uri) - 1] == '/') {
		strlcat(filepath, "/index.html", sizeof(filepath));
	} else {
		strlcat(filepath, req->uri, sizeof(filepath));
	}
	int fd = open(filepath, O_RDONLY, 0);
	if (fd == -1) {
		ESP_LOGE(REST_TAG, "Failed to open file : %s", filepath);
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
		return ESP_FAIL;
	}
	set_content_type_from_file(req, filepath);
	char *chunk = rest_context->scratch;
	ssize_t read_bytes;
	do {
		/* Read file in chunks into the scratch buffer */
		read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
		if (read_bytes == -1) {
			ESP_LOGE(REST_TAG, "Failed to read file : %s", filepath);
		} else if (read_bytes > 0) {
			/* Send the buffer contents as HTTP response chunk */
			if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
				close(fd);
				ESP_LOGE(REST_TAG, "File sending failed!");
				/* Abort sending file */
				httpd_resp_sendstr_chunk(req, NULL);
				/* Respond with 500 Internal Server Error */
				httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
				return ESP_FAIL;
			}
		}
	} while (read_bytes > 0);
	/* Close file after sending complete */
	close(fd);
	ESP_LOGI(REST_TAG, "File sending complete");
	/* Respond with an empty chunk to signal HTTP response completion */
	httpd_resp_set_status(req,HTTPD_200);
	httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
	// Set status
	httpd_resp_set_status(req, "302 Temporary Redirect");
	// Redirect to the "/" root directory
	httpd_resp_set_hdr(req, "Location", "/");
	// iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
	httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

	ESP_LOGI(TAG3, "Redirecting to root");
	return ESP_OK;
}

static httpd_handle_t start_rest_server(const char *base_path)
{
    REST_CHECK(base_path, "wrong base path", err);
    rest_context = calloc(1, sizeof(rest_server_context_t));
    REST_CHECK(rest_context, "No memory for rest context", err);
    strlcpy(rest_context->base_path, base_path, sizeof(rest_context->base_path));

	// Start the httpd server
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.ctrl_port = 81;
    config.max_uri_handlers = 5;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.lru_purge_enable = true;

    ESP_LOGI(REST_TAG, "Starting HTTP Server");
    REST_CHECK(httpd_start(&server, &config) == ESP_OK, "Start server failed", err_start);

    //---------- HTTP ENDPOINTS---------//
    /* URI handler for fetching available nodes */
	httpd_uri_t scan_nodes_get_uri = {
		.uri = "/api/v1/scanNodes",
		.method = HTTP_GET,
		.handler = scan_nodes_get_handler,
		.user_ctx = rest_context
	};
	httpd_register_uri_handler(server, &scan_nodes_get_uri);

	/* URI handler for connecting to a node */
	httpd_uri_t connect_node_post_uri = {
		.uri = "/api/v1/connectNode",
		.method = HTTP_POST,
		.handler = connect_node_post_handler,
		.user_ctx = rest_context
	};
	httpd_register_uri_handler(server, &connect_node_post_uri);
	/* URI handler for disconnecting to a node */
	httpd_uri_t disconnect_node_get_uri = {
		.uri = "/api/v1/disconnectNode",
		.method = HTTP_GET,
		.handler = disconnect_node_get_handler,
		.user_ctx = rest_context
	};
	httpd_register_uri_handler(server, &disconnect_node_get_uri);

    /* URI handler for getting web server files */
    httpd_uri_t common_get_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = rest_common_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &common_get_uri);
    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
    ESP_LOGI(REST_TAG, "Registered HTTP server URI handlers.");
    return server;
err_start:
    free(rest_context);
err:
    return NULL;
}

static void stop_rest_server(httpd_handle_t server)
{
    httpd_stop(server);
    free(rest_context);
}


//**************** LOCAL SERVER CONFIGURATION ******************//

//---------------- HTTP handlers --------------//
static void disconnect_http_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* http_server = (httpd_handle_t*) arg;
    if (*http_server) {
        ESP_LOGI(TAG3, "Stopping http server");
        stop_rest_server(*http_server);
        *http_server = NULL;
    }
}

static void connect_http_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* http_server = (httpd_handle_t*) arg;
    if (*http_server == NULL) {
        ESP_LOGI(TAG3, "Starting http server");
        *http_server = start_rest_server(CONFIG_HTTP_SERVER_WEB_MOUNT_POINT);
    }
}

//------------ WebSocket Handlers --------//

static void disconnect_ws_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* ws_server = (httpd_handle_t*) arg;
    if (*ws_server) {
        ESP_LOGI(TAG3, "Stopping ws server");
        stop_ws_server(*ws_server);
        *ws_server = NULL;
    }
}

static void connect_ws_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* ws_server = (httpd_handle_t*) arg;
    if (*ws_server == NULL) {
        ESP_LOGI(TAG3, "Starting ws server");
        *ws_server = start_ws_server();
    }
}




//------------- MAIN TASK -----------//
void local_server_task(void *pvParameters)
{
	ESP_ERROR_CHECK(init_fs());

	esp_event_handler_instance_t wifi_instance_any_id;
	esp_event_handler_instance_t ip_instance_any_id;
	//------------------ HTTP SERVER EVENTS ----------------//
	static httpd_handle_t http_server = NULL;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &connect_http_handler, &http_server,&ip_instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &disconnect_http_handler, &http_server,&wifi_instance_any_id));

    //------------------ WebSocket SERVER EVENTS ----------------//
	static httpd_handle_t ws_server = NULL;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &connect_ws_handler, &ws_server,&ip_instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &disconnect_ws_handler, &ws_server,&wifi_instance_any_id));

    ESP_LOGI(TAG3,"-----------------------");
    ESP_LOGI(TAG3,"LOCAL SERVER INITIALIZED");
    ESP_LOGI(TAG3,"-----------------------");
    xEventGroupSync(task_formation_event_group,(EventBits_t) pvParameters,ALL_SYNC_BITS,portMAX_DELAY);
    local_server_event_group = xEventGroupCreate();
    while (true)
    {
        EventBits_t bits = xEventGroupWaitBits(local_server_event_group,
        										START_TRIGGER_WS_ASYNC_DATA | STOP_TRIGGER_WS_ASYNC_DATA | START_NODE_WS_ASYNC_DATA | STOP_NODE_WS_ASYNC_DATA,
                                                pdFALSE,
                                                pdFALSE,
                                                portMAX_DELAY);

        if (bits & START_TRIGGER_WS_ASYNC_DATA) {
			//----- Clear flag-------//
			xEventGroupClearBits(local_server_event_group,START_TRIGGER_WS_ASYNC_DATA);
			//----- Code-----//
			if(trigger_ws_async_data_task_handler == NULL){
				xTaskCreatePinnedToCore(trigger_ws_async_data_task, "ws_async_data_task", 1024*4, (void *)&ws_server, 3, &trigger_ws_async_data_task_handler,1);
			}
		}
		else if (bits & STOP_TRIGGER_WS_ASYNC_DATA) {
			//----- Clear flag-------//
			xEventGroupClearBits(local_server_event_group,STOP_TRIGGER_WS_ASYNC_DATA);
			//----- Code-----//
			if(trigger_ws_async_data_task_handler != NULL){
				vTaskDelete(trigger_ws_async_data_task_handler);
				trigger_ws_async_data_task_handler = NULL;
			}
		}else if (bits & START_NODE_WS_ASYNC_DATA) {
			//----- Clear flag-------//
			xEventGroupClearBits(local_server_event_group,START_NODE_WS_ASYNC_DATA);
			//----- Code-----//
			if(node_ws_async_data_task_handler == NULL){
				xTaskCreatePinnedToCore(node_ws_async_data_task, "ws_async_data_task", 1024*4, (void *)&ws_server, 3, &node_ws_async_data_task_handler,1);
			}
		}
		else if (bits & STOP_NODE_WS_ASYNC_DATA) {
			//----- Clear flag-------//
			xEventGroupClearBits(local_server_event_group,STOP_NODE_WS_ASYNC_DATA);
			//----- Code-----//
			if(node_ws_async_data_task_handler != NULL){
				vTaskDelete(node_ws_async_data_task_handler);
				node_ws_async_data_task_handler = NULL;
			}
		}
		else {
			ESP_LOGE(TAG3, "UNEXPECTED COMMAND");
		}
    }
}
