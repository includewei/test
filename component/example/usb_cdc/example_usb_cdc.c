#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>
#include "platform_opts.h"
#include "usb.h"
#include "cdc/inc/usbd_cdc.h"

#define USB_CONSOLE_LOG //The default mode is loopback

extern void console_stdio_init(void *read_cb, void *write_cb);

static int acm_receive(void *buf, uint16_t length);
usbd_cdc_acm_usr_cb_t cdc_acm_usr_cb = {
	.init = NULL,
	.deinit = NULL,
	.receive = acm_receive,
#if (CONFIG_USDB_CDC_ACM_APP == ACM_APP_ECHO_ASYNC)
	.transmit_complete = NULL,//acm_transmit_complete,
#endif
};

#ifdef USB_CONSOLE_LOG
#define USBCDC_BUF_SIZE 2048
static char cdc_buffer[2][USBCDC_BUF_SIZE];
static int cdc_buffer_idx[2] = {0};
static int cdc_buffer_slot = 0;

static unsigned cdc_acm_write_buffer(unsigned fd, const void *buf, unsigned len)
{
	int ret = 0;
	if (cdc_port_status()) {
		ret = usbd_cdc_acm_sync_transmit_data((void *)buf, len);
		if (ret != 0) {
			vTaskDelay(10);
			ret = usbd_cdc_acm_sync_transmit_data((void *)buf, len); //Try again
		}
	}
	return len;
}

static unsigned cdc_acm_read_buffer(unsigned fd, void *buf, unsigned len)
{
	int curr_slot;
	unsigned cnt;
	__disable_irq();
	curr_slot = cdc_buffer_slot;
	cdc_buffer_slot ^= 1;
	__enable_irq();
	cnt = (unsigned)cdc_buffer_idx[curr_slot];
	memcpy(buf, cdc_buffer[curr_slot], cdc_buffer_idx[curr_slot]);
	cdc_buffer_idx[curr_slot] = 0;

	return cnt;
}
#endif

static int acm_receive(void *buf, uint16_t length)
{
#ifndef USB_CONSOLE_LOG
	int ret = 0;
	uint16_t len = length;
	ret = usbd_cdc_acm_sync_transmit_data(buf, len);
	if (ret != 0) {
		printf("\nFail to transmit data: %d\n", ret);
	}

	return ret;
#else
	int i = 0;
	uint16_t len = length;
	int *idx = &cdc_buffer_idx[cdc_buffer_slot];
	char *ptr = (char *)buf;
	for (i = 0; i < len; i++) {
		idx = &cdc_buffer_idx[cdc_buffer_slot];
		if ((*idx) < USBCDC_BUF_SIZE - 1) {
			cdc_buffer[cdc_buffer_slot][*idx] = ptr[i];
			(*idx)++;
		}
	}
	return 0;
#endif
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
#ifdef USB_CONSOLE_LOG
	console_stdio_init((void *)cdc_acm_read_buffer, (void *)cdc_acm_write_buffer);
#endif
exit:
	vTaskDelete(NULL);
}


void example_usb_cdc(void)
{
	if (xTaskCreate(example_cdc_thread, ((const char *)"example_cdc_thread"), 2048, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
		printf("\n\r%s xTaskCreate(example_cdc_thread) failed", __FUNCTION__);
	}
}