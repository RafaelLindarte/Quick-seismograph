#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#include "uart_controller.h"
#include "local_server.h"
static const char *TAG5 = "UART CONTROLLER TASK";

//-------Tasks Formation Event Group -------//
extern EventGroupHandle_t task_formation_event_group;
#define MANAGER_SYNC_BIT 			(1 << 0)
#define WIFI_CONTROLLER_INIT_BIT 	(1 << 1)
#define LOCAL_SERVER_INIT_BIT 		(1 << 2)
#define WEBSOCKET_CLIENT_INIT_BIT 	(1 << 3)
#define WEBSOCKET_CLIENT_END_BIT 	(1 << 4)
#define UART_CONTROLLER_INIT_BIT	(1 << 5)
#define ALL_SYNC_BITS 				( MANAGER_SYNC_BIT | WIFI_CONTROLLER_INIT_BIT | LOCAL_SERVER_INIT_BIT | UART_CONTROLLER_INIT_BIT)


//------------- UART CONTROLLER TASK -----------------//
xQueueHandle uartCommandQueue;
xQueueHandle uartQueue;

void rx_task(void *pvParameters)
{

    while (true) {
    	char* data = (char*) malloc(UART_BUF_SIZE);
    	int length = 0;
		uart_get_buffered_data_len(CONFIG_UART_PORT_NUM, (size_t*)&length);
		if(length > 0){
			const int rxBytes = uart_read_bytes(CONFIG_UART_PORT_NUM, data, UART_BUF_SIZE,pdMS_TO_TICKS(UART_BUF_SIZE));
			if (rxBytes > 1) {
				data = (char *) realloc(data,UART_BUF_SIZE+1);
				data[rxBytes] = '\0';
				ESP_LOGI(TAG5, "Read2 %d bytes: '%s'", rxBytes, data);
				xQueueSend(uartQueue,(void *)data, pdMS_TO_TICKS(100));
			}
		}
		free(data);
		vTaskDelay(pdMS_TO_TICKS(10));
    }

}

//------------ MAIN TASK ----------//
void uart_controller_task(void *pvParameters)
{
	/* Configure parameters of an UART driver,
	 * communication pins and install the driver */
	uart_config_t uart_config = {
		.baud_rate = CONFIG_UART_BAUD_RATE,
		.data_bits = UART_DATA_8_BITS,
		.parity    = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_APB,
	};
	int intr_alloc_flags = 0;

	ESP_ERROR_CHECK(uart_driver_install(CONFIG_UART_PORT_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
	ESP_ERROR_CHECK(uart_param_config(CONFIG_UART_PORT_NUM, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin(CONFIG_UART_PORT_NUM, CONFIG_UART_TXD, CONFIG_UART_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
	uartCommandQueue = xQueueCreate(1,20);
	uartQueue = xQueueCreate(10,UART_BUF_SIZE+1);
    ESP_LOGI(TAG5,"----------------------------");
	ESP_LOGI(TAG5,"UART CONTROLLER INITIALIZED");
	ESP_LOGI(TAG5,"----------------------------");
	xEventGroupSync(task_formation_event_group,(EventBits_t) pvParameters,ALL_SYNC_BITS,portMAX_DELAY);
    uart_controller_event_group = xEventGroupCreate();
	while (true)
	{
		EventBits_t bits = xEventGroupWaitBits(uart_controller_event_group,
												SEND_COMMAND_SHOT_POINT | START_RECIEVING_DATA | STOP_RECIEVING_DATA,
												pdFALSE,
												pdFALSE,
												portMAX_DELAY);

		if (bits & SEND_COMMAND_SHOT_POINT) {
			//----- Clear flag -------//
			xEventGroupClearBits(uart_controller_event_group,SEND_COMMAND_SHOT_POINT);
			//----- Code-------//
			char command[9];
			xQueueReceive(uartCommandQueue,(void*)command,portMAX_DELAY);
			ESP_LOGW(TAG5,"%s",command);
			uart_write_bytes(CONFIG_UART_PORT_NUM, (const char *) command, strlen(command));
		}
		else if(bits & START_RECIEVING_DATA){
			//----- Clear flag -------//
			xEventGroupClearBits(uart_controller_event_group,START_RECIEVING_DATA);
			//----- Code-------//
			if(rx_task_handler == NULL){
			//				uartQueue = xQueueCreate(1,2);
				xTaskCreatePinnedToCore(rx_task, "uart_rx_task", 1024*4, NULL, 3, &rx_task_handler,0);
			}
		}
		else if(bits & STOP_RECIEVING_DATA){
			//----- Clear flag -------//
			xEventGroupClearBits(uart_controller_event_group,STOP_RECIEVING_DATA);
			//----- Code-------//
			if(rx_task_handler != NULL){
				vTaskDelete(rx_task_handler);
				rx_task_handler = NULL;
			}
		}
	}
}

