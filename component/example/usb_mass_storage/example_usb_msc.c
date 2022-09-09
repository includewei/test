#include "FreeRTOS.h"
#include "task.h"
//#include <platform/platform_stdlib.h>
#include "basic_types.h"
#include "platform_opts.h"

#include "usb.h"
#include "msc/inc/usbd_msc_config.h"
#include "msc/inc/usbd_msc.h"
#include "fatfs_ramdisk_api.h"
#include "fatfs_sdcard_api.h"

#if defined(CONFIG_PLATFORM_8195BHP) || defined(CONFIG_PLATFORM_8735B)
#include "sdio_combine.h"
#endif

//#define USB_RAM
#define USB_SD

void example_mass_storage_thread(void *param)
{
	int status = 0;
	struct msc_opts *disk_operation = NULL;
#if defined(configENABLE_TRUSTZONE) && (configENABLE_TRUSTZONE == 1)
	rtw_create_secure_context(configMINIMAL_SECURE_STACK_SIZE);
#endif
	_usb_init();
#if defined(CONFIG_PLATFORM_8195BHP) || defined(CONFIG_PLATFORM_8735B)
#ifdef USB_SD
	sd_gpio_init();
	sdio_driver_init();
#endif
#ifdef USB_RAM
	fatfs_ram_init();
#endif
#endif
	status = wait_usb_ready();
	if (status != USB_INIT_OK) {
		if (status == USB_NOT_ATTACHED) {
			printf("\r\n NO USB device attached\n");
		} else {
			printf("\r\n USB init fail\n");
		}
		goto exit;
	}

	disk_operation = malloc(sizeof(struct msc_opts));
	if (disk_operation == NULL) {
		printf("\r\n disk_operation malloc fail\n");
		goto exit;
	}
#ifdef USB_SD
	disk_operation->disk_init = usb_sd_init;
	disk_operation->disk_deinit = usb_sd_deinit;
	disk_operation->disk_getcapacity = usb_sd_getcapacity;
	disk_operation->disk_read = usb_sd_readblocks;
	disk_operation->disk_write = usb_sd_writeblocks;
#endif
#ifdef USB_RAM
	disk_operation->disk_init = usb_ram_init;
	disk_operation->disk_deinit = usb_ram_deinit;
	disk_operation->disk_getcapacity = usb_ram_getcapacity;
	disk_operation->disk_read = usb_ram_readblocks;
	disk_operation->disk_write = usb_ram_writeblocks;
#endif
	// load usb mass storage driver
	status = usbd_msc_init(MSC_NBR_BUFHD, MSC_BUFLEN, disk_operation);

	if (status) {
		printf("USB MSC driver load fail.\n");
	} else {
		printf("USB MSC driver load done, Available heap [0x%x]\n", xPortGetFreeHeapSize());
	}
exit:
	vTaskDelete(NULL);
}


void example_mass_storage(void)
{
	if (xTaskCreate(example_mass_storage_thread, ((const char *)"example_fatfs_thread"), 2048, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
		printf("\n\r%s xTaskCreate(example_fatfs_thread) failed", __FUNCTION__);
	}
}

