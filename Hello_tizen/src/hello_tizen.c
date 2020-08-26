#include <tizen.h>
#include <service_app.h>
#include "hello_tizen.h"
#include "hello.h"

#include <unistd.h>

bool service_app_create(void *data)
{
    // Todo: add your code here.
    return true;
}

void service_app_terminate(void *data)
{
    // Todo: add your code here.
    return;
}

void service_app_control(app_control_h app_control, void *data)
{
    // Todo: mdm(BG96) Test code here.
//	mdm_powerOFF();
//	mdm_powerON();

	char buffer[32];
	char rbuffer[32];

//	while(true)
//	{
		/* Check for BG96 initializtion */
        if(!mdm_init())
			LOGE("BG96 Initialized");

        /* Check for Cellular station registration */
//		if(!mdm_IsRegistred())
//			LOGE("BG96 Network Registred");

        /* Get Device IMEI Number */
//		if( !mdm_getIMEI(buffer, sizeof(buffer)) )
//			LOGE("BG96 IMEI : %s", buffer);

    	/* Modem PDP Context Activation */
        if(!mdm_pdpAct(true))
			LOGE("BG96 PDP Activation");

        if( !mdm_socketOpen("echo.mbedcloudtesting.com", 7, true) )
        	LOGE("socket Open Success");

        strcpy(buffer, "Hello World");

        if( !mdm_socketSend(buffer, strlen(buffer)) )
            LOGE("socket Send Success");

        if( !mdm_socketRecv(rbuffer, sizeof(rbuffer)) )
            LOGE("socket Recv : %s ",rbuffer);

		mdm_socketClose();

//      usleep(1000*1000);
        if(!mdm_pdpAct(false))
			LOGE("BG96 PDP DeActivation");
//        usleep(1000*1000);
//	}

    return;
}

static void
service_app_lang_changed(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_LANGUAGE_CHANGED*/
	return;
}

static void
service_app_region_changed(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_REGION_FORMAT_CHANGED*/
}

static void
service_app_low_battery(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_LOW_BATTERY*/
}

static void
service_app_low_memory(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_LOW_MEMORY*/
}

int main(int argc, char* argv[])
{
    char ad[50] = {0,};
	service_app_lifecycle_callback_s event_callback;
	app_event_handler_h handlers[5] = {NULL, };

	event_callback.create = service_app_create;
	event_callback.terminate = service_app_terminate;
	event_callback.app_control = service_app_control;

	service_app_add_event_handler(&handlers[APP_EVENT_LOW_BATTERY], APP_EVENT_LOW_BATTERY, service_app_low_battery, &ad);
	service_app_add_event_handler(&handlers[APP_EVENT_LOW_MEMORY], APP_EVENT_LOW_MEMORY, service_app_low_memory, &ad);
	service_app_add_event_handler(&handlers[APP_EVENT_LANGUAGE_CHANGED], APP_EVENT_LANGUAGE_CHANGED, service_app_lang_changed, &ad);
	service_app_add_event_handler(&handlers[APP_EVENT_REGION_FORMAT_CHANGED], APP_EVENT_REGION_FORMAT_CHANGED, service_app_region_changed, &ad);

	LOGI("Hello Anchor ..............");

	return service_app_main(argc, argv, &event_callback, ad);
}
