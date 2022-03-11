#ifndef _WEBSOCKET_CLIENT_H_
#define _WEBSOCKET_CLIENT_H_

#define WEBSOCKET_BUF_SIZE (2048)
EventGroupHandle_t websocket_client_event_group;
#define CONNECT_CLIENT 		BIT0
#define DISCONNECT_CLIENT 	BIT1
#define CLIENT_STATUS		BIT2
#define SET_SERVER_URI 		BIT3
#define SEND_DATA 	   		BIT4
#define SEND_DATA_BIN  		BIT5
#define SEND_DATA_TXT  		BIT6
#define DESTROY_CLIENT		BIT7

#define CLIENT_STARTED	BIT8

void websocket_client_task( void *pvParameters );
#endif
