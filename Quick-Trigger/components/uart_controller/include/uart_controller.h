#ifndef _UART_CONTROLLER_H_
#define _UART_CONTROLLER_H_


#define UART_BUF_SIZE (128)
EventGroupHandle_t uart_controller_event_group;
#define SEND_COMMAND_SHOT_POINT			(1 << 0)
#define START_RECIEVING_DATA			(1 << 1)
#define STOP_RECIEVING_DATA				(1 << 2)
TaskHandle_t rx_task_handler;
void uart_controller_task( void *pvParameters );
#endif
