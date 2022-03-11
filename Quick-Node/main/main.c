#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

char *TAG1 = "MANAGER TASK";

//--------CUSTOM MODULES-------------//
#include "wifi_controller.h"
#include "local_server.h"
#include "dns_server.h"
#include "uart_controller.h"
//-------Task Create flags ------//
BaseType_t wifi_controller_create_flag;
//-------Task Handlers-----------//
TaskHandle_t wifi_controller_handler;
TaskHandle_t local_server_handler;
TaskHandle_t dns_server_handler;
TaskHandle_t uart_controller_handler;
//-------Task Status-------------//
TaskStatus_t wifi_controller_status;
TaskStatus_t local_server_status;
TaskStatus_t dns_server_status;
TaskStatus_t uart_controller_status;
//-------Tasks Formation Event Group -------//
EventGroupHandle_t task_formation_event_group;
#define MANAGER_SYNC_BIT 			(1 << 0)
#define WIFI_CONTROLLER_INIT_BIT 	(1 << 1)
#define LOCAL_SERVER_INIT_BIT 		(1 << 2)
#define UART_CONTROLLER_INIT_BIT	(1 << 3)
#define ALL_SYNC_BITS 				( MANAGER_SYNC_BIT | WIFI_CONTROLLER_INIT_BIT | LOCAL_SERVER_INIT_BIT | UART_CONTROLLER_INIT_BIT)

UBaseType_t number_of_tasks;
BaseType_t scheduler_state;
void app_main(void)
{
    //----------- Initialize NVS ------------//
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    //----------- State of Scheduler ------------//
    scheduler_state = xTaskGetSchedulerState();
    number_of_tasks = uxTaskGetNumberOfTasks();
    ESP_LOGW(TAG1,"Scheduler state: %d\n",scheduler_state);
    ESP_LOGW(TAG1,"Active TASKS: %d\n",number_of_tasks);
    //----------- Heap Memory Available ------------//
    ESP_LOGW(TAG1, "xPortGetFreeHeapSize %dk = DRAM", xPortGetFreeHeapSize());
    int DRam = heap_caps_get_free_size(MALLOC_CAP_8BIT);
	int IRam = heap_caps_get_free_size(MALLOC_CAP_32BIT) - heap_caps_get_free_size(MALLOC_CAP_8BIT);

	ESP_LOGW(TAG1, "DRAM \t\t %d", DRam);
	ESP_LOGW(TAG1, "IRam \t\t %d\n", IRam);
    int available_block_for_new_task = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    ESP_LOGW(TAG1, "Available Memory For Tasks = %d\n", available_block_for_new_task);

    char cBuffer[(number_of_tasks+2)*sizeof(TaskStatus_t)];
    vTaskList(cBuffer);
    printf("%s",cBuffer);


    task_formation_event_group = xEventGroupCreate();

    ESP_LOGI(TAG1,"-----------------------");
    ESP_LOGI(TAG1,"MAIN TASK INITIALIZED");
    ESP_LOGI(TAG1,"-----------------------\n");
    xTaskCreatePinnedToCore(&wifi_controller_task, "wifi_controller_task", 1024 * 4, (void *)WIFI_CONTROLLER_INIT_BIT, 2,&wifi_controller_handler,0);
    xTaskCreatePinnedToCore(&local_server_task, "local_server_task", 1024 * 6, (void *)LOCAL_SERVER_INIT_BIT, 5,&local_server_handler,1);
    xTaskCreatePinnedToCore(&uart_controller_task, "uart_controller_task", 1024 * 4, (void *)UART_CONTROLLER_INIT_BIT, 2,&uart_controller_handler,1);
    EventBits_t uxReturn = xEventGroupSync(task_formation_event_group,MANAGER_SYNC_BIT,ALL_SYNC_BITS,portMAX_DELAY);

    if( ( uxReturn & ALL_SYNC_BITS ) == ALL_SYNC_BITS )
	{
    	ESP_LOGW(TAG1,"SYNC DONE!");
    	if( (wifi_controller_handler != NULL) && (local_server_handler != NULL) ){
    			vTaskGetInfo(wifi_controller_handler,&wifi_controller_status,pdTRUE,eInvalid);
    			ESP_LOGW(TAG1,"Wifi Controller Task: %d\n",(int)wifi_controller_status.eCurrentState);
    			vTaskGetInfo(local_server_handler,&local_server_status,pdTRUE,eInvalid);
    			ESP_LOGW(TAG1,"Local Server Task: %d\n",(int)local_server_status.eCurrentState);
    	}
	}
    while(true){

//    	scheduler_state = xTaskGetSchedulerState();
//		number_of_tasks = uxTaskGetNumberOfTasks();
//		ESP_LOGW(TAG1,"Scheduler state: %d , Active TASKS: %d\n",scheduler_state,number_of_tasks);
//
//		vTaskList(cBuffer);
//		ESP_LOGI(TAG1,"-----------------------");
//		printf("%s\n",cBuffer);
//		ESP_LOGI(TAG1,"-----------------------");
        vTaskDelay(pdMS_TO_TICKS(10000));
//        memset(cBuffer,0,sizeof(cBuffer));
    }

}

