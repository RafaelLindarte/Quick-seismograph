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

#include "wifi_controller.h"

char *TAG2 = "WIFI CONTROLLER TASK";
//------------- TASK FORMATION -------------//
extern EventGroupHandle_t task_formation_event_group;
#define MANAGER_SYNC_BIT 			(1 << 0)
#define WIFI_CONTROLLER_INIT_BIT 	(1 << 1)
#define LOCAL_SERVER_INIT_BIT 		(1 << 2)
#define ALL_SYNC_BITS 				( MANAGER_SYNC_BIT | WIFI_CONTROLLER_INIT_BIT | LOCAL_SERVER_INIT_BIT)


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
	IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
	IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
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
			.max_connection = 2,
			.authmode = WIFI_AUTH_WPA_WPA2_PSK
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_wifi_config) );
	//---------------WIFI events---------//
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

//-----------MAIN TASK -----------//
void wifi_controller_task( void *pvParameters){
	wifi_init_softap();
	vTaskDelay(pdMS_TO_TICKS(10));
    ESP_LOGI(TAG2,"-----------------------");
	ESP_LOGI(TAG2,"WIFI CONTROLLER INITIALIZED");
	ESP_LOGI(TAG2,"-----------------------");
	xEventGroupSync(task_formation_event_group,(EventBits_t) pvParameters,ALL_SYNC_BITS,portMAX_DELAY);
	wifi_controller_event_group = xEventGroupCreate();
    while(true){
    	vTaskDelay(pdMS_TO_TICKS(100));
    }
}
