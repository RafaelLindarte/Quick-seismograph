#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "cJSON.h"

#include "wifi_controller.h"
#include "websocket_client.h"
char *TAG2 = "WIFI CONTROLLER TASK";
//------------- TASK FORMATION -------------//
extern EventGroupHandle_t task_formation_event_group;
#define MANAGER_SYNC_BIT 			(1 << 0)
#define WIFI_CONTROLLER_INIT_BIT 	(1 << 1)
#define LOCAL_SERVER_INIT_BIT 		(1 << 2)
#define WEBSOCKET_CLIENT_INIT_BIT   (1 << 3)
#define WEBSOCKET_CLIENT_END_BIT    (1 << 4)
#define UART_CONTROLLER_INIT_BIT	(1 << 5)
#define ALL_SYNC_BITS 				( MANAGER_SYNC_BIT | WIFI_CONTROLLER_INIT_BIT | LOCAL_SERVER_INIT_BIT | UART_CONTROLLER_INIT_BIT)


uint8_t Network_ssid[32];
uint8_t Network_pass[64]="12345678";
xQueueHandle networksQueue;
xQueueHandle connectQueue;
//extern xQueueHandle websocketCommandQueue;
//-------------------Wifi Event Handler ----------------------//
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
	//************************** AP EVENTS ******************************
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
		ESP_LOGI(TAG2, "AP Ready");
	}
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STOP) {
		ESP_LOGI(TAG2, "AP Not Available");
	}
	else if (event_id == WIFI_EVENT_AP_STACONNECTED) {
		wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
		ESP_LOGI(TAG2, "station "MACSTR" join, AID=%d",MAC2STR(event->mac), event->aid);
	}
	else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
		wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
		ESP_LOGI(TAG2, "station "MACSTR" leave, AID=%d",MAC2STR(event->mac), event->aid);
	}
	//***************************** STATION EVENTS ****************************
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		ESP_LOGI(TAG2, "STA Ready");
	}
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
		ESP_LOGI(TAG2, "Scan Done!");
	}
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
		ESP_LOGI(TAG2, "connected to ap SSID:%s",Network_ssid);
		xEventGroupSetBits(wifi_controller_event_group,WIFI_CONNECTED);
	}
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		ESP_LOGI(TAG2, "disconnected from ap SSID:%s",Network_ssid);
		xEventGroupSetBits(wifi_controller_event_group,WIFI_DISCONNECTED);
	}
	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		ESP_LOGI(TAG2, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		xEventGroupSetBits(websocket_client_event_group, CONNECT_CLIENT);
	}else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP) {
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		ESP_LOGI(TAG2, "lost ip:" IPSTR, IP2STR(&event->ip_info.ip));
	}
}
//--------------------------Wifi Custom Functions--------------//
void wifi_init_softap(void)
{
	//--------------Network Interface--------------//
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
	esp_netif_create_default_wifi_sta();
	assert(ap_netif);

	esp_netif_ip_info_t ip_info;
	IP4_ADDR(&ip_info.ip, 124, 213, 16, 29);
	IP4_ADDR(&ip_info.gw, 124, 213, 16, 29);
	IP4_ADDR(&ip_info.netmask, 255, 0, 0, 0);
	esp_netif_dhcps_stop(ap_netif);
	esp_netif_set_ip_info(ap_netif, &ip_info);
	esp_netif_dhcps_start(ap_netif);


	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	//---------------- WIFI MODE ----------------//
	wifi_config_t ap_wifi_config = {
		.ap = {
			.ssid = CONFIG_AP_WIFI_SSID,
			.ssid_len = strlen(CONFIG_AP_WIFI_SSID),
			.channel = 1,
			.password = CONFIG_AP_WIFI_PASSWORD,
			.max_connection = 1,
			.authmode = WIFI_AUTH_WPA_WPA2_PSK
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA) );
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_wifi_config));
	//---------------WIFI events---------//
	esp_wifi_set_default_wifi_sta_handlers();
	esp_event_handler_instance_t wifi_instance_any_id;
	esp_event_handler_instance_t ip_instance_any_id;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
														ESP_EVENT_ANY_ID,
														&wifi_event_handler,
														NULL,
														&wifi_instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
														ESP_EVENT_ANY_ID,
														&wifi_event_handler,
														NULL,
														&ip_instance_any_id));

	ESP_ERROR_CHECK(esp_wifi_start());
}

void wifi_scan(void){

	wifi_scan_config_t scan_config = {
		.show_hidden = true,
		.scan_type = WIFI_SCAN_TYPE_ACTIVE,
	};
    uint16_t number = CONFIG_NETWORK_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[CONFIG_NETWORK_SCAN_LIST_SIZE];
    char TxBuffer[1][256];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));
    ESP_LOGI(TAG2,"Scanning..");
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG2,"--------------Available Networks--------------");
    ESP_LOGI(TAG2, "Total APs scanned ===> %u", ap_count);
    cJSON *scan_list_obj = cJSON_CreateObject();
    cJSON *scan_list_array = cJSON_CreateArray();
    for (int i = 0; (i < CONFIG_NETWORK_SCAN_LIST_SIZE) && (i < ap_count); i++) {
        ESP_LOGI(TAG2, "SSID \t\t%s\n", ap_info[i].ssid);
        char ssid[33];
        memcpy(ssid,ap_info[i].ssid,sizeof(ap_info[i].ssid));
        if(strstr(ssid,"Quick-Node") != NULL){
        	cJSON_AddItemToArray(scan_list_array,cJSON_CreateString(ssid));
        }
    }
    cJSON_AddItemToObject(scan_list_obj,"nodes",scan_list_array);
    char *scanned_nodes_str;
    scanned_nodes_str = cJSON_Print(scan_list_obj);
    strcpy(TxBuffer[0],scanned_nodes_str);
    xQueueSend(networksQueue,TxBuffer[0],1000 / portTICK_PERIOD_MS);
    free(scanned_nodes_str);
    cJSON_Delete(scan_list_obj);
}

esp_err_t wifi_connect_sta(char sta_ssid[32]){

	wifi_config_t sta_wifi_config = {
	        .sta = {
				.threshold.authmode = WIFI_AUTH_WPA2_PSK,
	            .pmf_cfg = {
	                .capable = true,
	                .required = false
	            },
	        },
	    };
	memcpy(sta_wifi_config.sta.ssid,sta_ssid,33);
	memcpy(sta_wifi_config.sta.password,Network_pass,sizeof(Network_pass));
	memcpy(Network_ssid,sta_wifi_config.sta.ssid,32);
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_wifi_config));
	return esp_wifi_connect();
}

esp_err_t wifi_disconnect_sta(void){
    return esp_wifi_disconnect();
}


//-----------MAIN TASK -----------//
void wifi_controller_task( void *pvParameters){
	wifi_init_softap();
	vTaskDelay(pdMS_TO_TICKS(10));
    ESP_LOGI(TAG2,"-----------------------");
	ESP_LOGI(TAG2,"WIFI CONTROLLER INITIALIZED");
	ESP_LOGI(TAG2,"-----------------------");
	xEventGroupSync(task_formation_event_group,(EventBits_t) pvParameters,ALL_SYNC_BITS,portMAX_DELAY);
	networksQueue = xQueueCreate(1,256);
	connectQueue = xQueueCreate(1,33);
	esp_err_t connect_sta_status = ESP_OK;
	wifi_controller_event_group = xEventGroupCreate();
    while(true){
        EventBits_t bits = xEventGroupWaitBits(wifi_controller_event_group,
                                                WIFI_SCAN | WIFI_CONNECT_TO_NETWORK | WIFI_DISCONNECT_FROM_NETWORK,
                                                pdFALSE,
                                                pdFALSE,
                                                portMAX_DELAY);
        if(bits & WIFI_SCAN){
            ESP_LOGW(TAG2,"------------------ Wifi Scan for Ap's -------------- ");
            xEventGroupClearBits(wifi_controller_event_group,WIFI_SCAN);
            wifi_scan();
        }
        else if(bits & WIFI_CONNECT_TO_NETWORK){
            ESP_LOGW(TAG2,"------------------ Wifi Connecting to Selected Network -------------- ");
            xEventGroupClearBits(wifi_controller_event_group,WIFI_CONNECT_TO_NETWORK);
            char RxBuffer[32];
            xQueueReceive(connectQueue,(void*)&RxBuffer,portMAX_DELAY);
            if(connect_sta_status == ESP_OK){
            	connect_sta_status = wifi_connect_sta(RxBuffer);
            }
        }
        else if(bits & WIFI_DISCONNECT_FROM_NETWORK){
			ESP_LOGW(TAG2,"------------------ Wifi Disconnecting to Selected Network -------------- ");
			xEventGroupClearBits(wifi_controller_event_group,WIFI_DISCONNECT_FROM_NETWORK);
			vTaskDelay(pdMS_TO_TICKS(2500));
			if(connect_sta_status == ESP_OK){
				connect_sta_status = wifi_disconnect_sta();
			}
		}
    }
}
