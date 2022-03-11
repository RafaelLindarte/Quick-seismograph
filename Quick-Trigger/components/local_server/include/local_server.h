#ifndef _LOCAL_SERVER_H_
#define _LOCAL_SERVER_H_

//-------------- WS ASYNC DATA ---------------//
TaskHandle_t trigger_ws_async_data_task_handler;
TaskHandle_t node_ws_async_data_task_handler;
void trigger_ws_async_data_task(void *pvParameters);
void node_ws_async_data_task(void *pvParameters);
//------------- LOCAL SERVER -----------------//
EventGroupHandle_t local_server_event_group;
#define START_TRIGGER_WS_ASYNC_DATA 		BIT0
#define STOP_TRIGGER_WS_ASYNC_DATA 			BIT1
#define START_NODE_WS_ASYNC_DATA 			BIT2
#define STOP_NODE_WS_ASYNC_DATA 			BIT3

void local_server_task( void *pvParameters );
#endif
