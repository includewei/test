#include "FreeRTOS.h"
#include "task.h"
#include "basic_types.h"
#include "platform_opts.h"

#include "usb.h"
#include "cdc/inc/usbd_cdc.h"

static int acm_receive(void *buf, u16 length);
usbd_cdc_acm_usr_cb_t cdc_acm_usr_cb = {
	.init = NULL,
	.deinit = NULL,
	.receive = acm_receive,
#if (CONFIG_USDB_CDC_ACM_APP == ACM_APP_ECHO_ASYNC)
	.transmit_complete = NULL,//acm_transmit_complete,
#endif
};
//Loop back mode
static int acm_receive(void *buf, u16 length)
{
	int ret = 0;
	u16 len = length;
	ret = usbd_cdc_acm_sync_transmit_data(buf, len);
	if (ret != 0) {
		printf("\nFail to transmit data: %d\n", ret);
	}

	return ret;
}

void get_cdc_status(void) //Please
{
	printf("usb_insert %d\r\n", usb_insert_status()); //Check usb connetc
	printf("cdc_port_status %d\r\n", cdc_port_status()); //Check com port connect
}

void example_cdc_thread(void *param)
{
	int status = 0;

	_usb_init();

	status = wait_usb_ready();
	if (status != USB_INIT_OK) {
		if (status == USB_NOT_ATTACHED) {
			printf("\r\n NO USB device attached\n");
		} else {
			printf("\r\n USB init fail\n");
		}
		goto exit;
	}

	status = usbd_cdc_acm_init(0, 0, &cdc_acm_usr_cb);
	if (status) {
		printf("USB CDC driver load fail.\n");
	} else {
		printf("USB CDC driver load done, Available heap [0x%x]\n", xPortGetFreeHeapSize());
	}
	vTaskDelay(2000);
	get_cdc_status();
exit:
	vTaskDelete(NULL);
}


void example_usb_cdc(void)
{
	if (xTaskCreate(example_cdc_thread, ((const char *)"example_cdc_thread"), 2048, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
		printf("\n\r%s xTaskCreate(example_cdc_thread) failed", __FUNCTION__);
	}
}