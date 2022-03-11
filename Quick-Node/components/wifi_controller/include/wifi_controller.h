#ifndef _WIFI_CONTROLLER_H_
#define _WIFI_CONTROLLER_H_

EventGroupHandle_t wifi_controller_event_group;
#define WIFI_SCAN 					 BIT0
#define WIFI_CONNECT_TO_NETWORK 	 BIT1
#define WIFI_DISCONNECT_FROM_NETWORK BIT2
#define WIFI_CONNECTED				 BIT3
#define WIFI_DISCONNECTED			 BIT4
//--------------Queue's---------------//
#define scanQueue_size 1
//--------------Function Prototypes-----------//
void wifi_init_softap(void);
void wifi_connect_sta(char sta_ssid[32]);
void wifi_scan(void);
void wifi_disconnect_sta(void);
//-------------- MAIN TASK --------------//
void wifi_controller_task( void *params );
#endif
