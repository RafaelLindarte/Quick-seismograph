#ifndef _LOCAL_SERVER_H_
#define _LOCAL_SERVER_H_
//-------------- WS ASYNC DATA ---------------//
TaskHandle_t ws_async_data_task_handler;
void ws_async_data_task(void *pvParameters);
//------------- LOCAL SERVER -----------------//
EventGroupHandle_t local_server_event_group;
#define START_WS_ASYNC_DATA 		BIT0
#define STOP_WS_ASYNC_DATA 			BIT1

void local_server_task( void *params );
#endif
