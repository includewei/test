#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>
#include "platform_opts.h"

#define _USB_ERRNO_H //Prevent redefine
#include "usb.h"
#include "dfu/inc/usbd_dfu.h"

#include "fwfs.h"
#include "sys_api.h"

#include "ota_8735b.h"

#define USE_CHECKSUM
typedef struct {
	int read_bytes;
	int update_size;
	_file_checksum file_checksum;
	uint32_t cur_fw_idx;
	uint32_t target_fw_idx;
	FILE *fp;
	struct dfu_opts dfu_cb;
	_sema dfu_rest_sema;
} dfu_operate;

static dfu_operate usb_ota_dfu;

int ota_upgrade_from_usb(unsigned char *buf, unsigned int size, int index)
{
	dfu_operate *dfu = &usb_ota_dfu;
	int ret = 0;
	int wr_status = 0;
	dfu->read_bytes = size;
	dfu->update_size += dfu->read_bytes;
#ifdef USE_CHECKSUM
	dfu->file_checksum.c[0] = buf[dfu->read_bytes - 4];
	dfu->file_checksum.c[1] = buf[dfu->read_bytes - 3];
	dfu->file_checksum.c[2] = buf[dfu->read_bytes - 2];
	dfu->file_checksum.c[3] = buf[dfu->read_bytes - 1];
#endif
	wr_status = pfw_write(dfu->fp, buf, dfu->read_bytes);
	if (wr_status < 0) {
		printf("\n\r[%s] ota flash failed", __FUNCTION__);
		goto update_ota_exit;
	}
	return 0;
update_ota_exit:
	return -1;
}

int ota_checksum_from_usb(void *parm)
{
	unsigned char *buf = malloc(4096);
	dfu_operate *dfu = &usb_ota_dfu;
	pfw_close(dfu->fp);
	int chksum = 0;
	int chklen = dfu->update_size - 4; //ota_len - 4;		// skip 4byte ota length
	void *chkfp = NULL;

	if (dfu->cur_fw_idx == 1) {
		chkfp = pfw_open("FW2", M_RAW | M_RDWR);
		printf("FW2\r\n");
	} else {
		chkfp = pfw_open("FW1", M_RAW | M_RDWR);
		printf("FW1\r\n");
	}

	if (!chkfp) {
		goto update_ota_exit;
	}
	printf("Checksum start\r\n");
	printf("chklen %d\r\n", chklen);
	while (chklen > 0) {
		int rdlen = chklen > 4096 ? 4096 : chklen;
		pfw_read(chkfp, buf, rdlen);
		for (int i = 0; i < rdlen; i++) {
			chksum += buf[i];
		}
		chklen -= rdlen;
	}

	printf("checksum Remote %x, Flash %x\n\r", dfu->file_checksum.u, chksum);
	if (dfu->file_checksum.u != chksum) {
		pfw_seek(chkfp, 0, SEEK_SET);
		memset(buf, 0, 4096);
		pfw_write(chkfp, buf, 4096);
		pfw_close(chkfp);
		printf("Check sum is fail\r\n");
		goto update_ota_exit;
	}
	pfw_close(chkfp);
	printf("\n\r[%s] Ready to reboot\r\n", __FUNCTION__);
	osDelay(100);
	if (buf) {
		free(buf);
	}
	return 0;
update_ota_exit:
	if (buf) {
		free(buf);
	}
	return -1;
}

int ota_reset_from_usb(void *parm)
{
	printf("ota_reset_from_usb\r\n");
	dfu_operate *dfu = &usb_ota_dfu;
	rtw_up_sema(&dfu->dfu_rest_sema);
	return 0;
}

void example_usb_dfu_ota_reset_thread(void *param)
{
#if defined(configENABLE_TRUSTZONE) && (configENABLE_TRUSTZONE == 1)
	rtw_create_secure_context(2048);
#endif
	dfu_operate *dfu = &usb_ota_dfu;
	while (1) {
		if (rtw_down_sema(&dfu->dfu_rest_sema)) {
			printf("dfu reset\r\n");
			ota_platform_reset();
			while (1) {
				vTaskDelay(1000);
			}
		}
	}
}

void example_usb_dfu_ota_thread(void *param)
{

	int status = 0;

	printf("%s %s %s\r\n", __TIME__, __DATE__, __FUNCTION__);

	dfu_operate *dfu = &usb_ota_dfu;

	memset(dfu, 0, sizeof(dfu_operate));

	dfu->dfu_cb.write = ota_upgrade_from_usb;
	dfu->dfu_cb.checksum = ota_checksum_from_usb;
	dfu->dfu_cb.reset = ota_reset_from_usb;

	dfu->cur_fw_idx = hal_sys_get_ld_fw_idx();

	printf("fw index %d\r\n", dfu->cur_fw_idx);

	rtw_init_sema(&dfu->dfu_rest_sema, 0);

	if (1 == dfu->cur_fw_idx) {
		dfu->target_fw_idx = 2;
		dfu->fp = pfw_open("FW2", M_RAW | M_CREATE);
		printf("Update fw2\r\n");
	} else if (2 == dfu->cur_fw_idx) {
		dfu->target_fw_idx = 1;
		dfu->fp = pfw_open("FW1", M_RAW | M_CREATE);
		printf("Update fw1\r\n");
	}

	if (!dfu->fp) {
		printf("Can't open the file\r\n");
		goto exit;
	}

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

	status = usbd_dfu_init(&dfu->dfu_cb);

	if (status) {
		printf("USB DFU driver load fail.\n");
	} else {
		printf("USB DFU driver load done, Available heap [0x%x]\n", xPortGetFreeHeapSize());
	}

	if (xTaskCreate(example_usb_dfu_ota_reset_thread, ((const char *)"example_usb_dfu_ota_reset_thread"), 2048, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
		printf("\n\r%s xTaskCreate(example_usb_dfu_ota_reset_thread) failed", __FUNCTION__);
	}
exit:
	vTaskDelete(NULL);
}


void example_usb_dfu_ota(void)
{
	if (xTaskCreate(example_usb_dfu_ota_thread, ((const char *)"example_usb_dfu_ota_thread"), 2048, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
		printf("\n\r%s xTaskCreate(example_usb_dfu_ota_thread) failed", __FUNCTION__);
	}
}