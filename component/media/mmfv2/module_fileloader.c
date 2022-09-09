/******************************************************************************
*
* Copyright(c) 2007 - 2018 Realtek Corporation. All rights reserved.
*
******************************************************************************/
#include <stdint.h>
#include "platform_stdlib.h"
#include "osdep_service.h"
#include "avcodec.h"
#include "mmf2_module.h"
#include "module_fileloader.h"

#include "ff.h"
#include <fatfs_ext/inc/ff_driver.h>
#include "sdio_combine.h"
#include "sdio_host.h"
#include <disk_if/inc/sdcard.h>
#include "fatfs_sdcard_api.h"

static FIL  m_file;
static fatfs_sd_params_t    fatfs_sd;

static int SD_file_loader_init(void);
static void SD_file_loader_deinit(void);
static int SD_file_load_file(uint32_t *pFrame, uint32_t *pSize, char *frameFilePath);

/*-----------------------------------------------------------------------------------*/

static int load_file_count = 0;

void fileloader_handler(void *p)
{
	fileloader_ctx_t *ctx = (fileloader_ctx_t *)p;

	while (load_file_count < ctx->load_file_num) {
		mm_context_t *mctx = (mm_context_t *)ctx->parent;
		mm_queue_item_t *output_item;
		int is_output_ready = xQueueReceive(mctx->output_recycle, &output_item, 0xFFFFFFFF) == pdTRUE;
		if (is_output_ready) {
			uint32_t test_data_addr, test_data_len;
			char sd_fn_in[64];
			memset(sd_fn_in, 0x00, sizeof(sd_fn_in));

			if (ctx->params.codec_id == AV_CODEC_ID_BMP || ctx->params.codec_id == AV_CODEC_ID_JPEG) {
				if (ctx->params.codec_id == AV_CODEC_ID_BMP) {
					snprintf(sd_fn_in, sizeof(sd_fn_in), "%s-%04d.bmp", ctx->sd_dataset_file_path_in, load_file_count + 1);
				} else if (ctx->params.codec_id == AV_CODEC_ID_JPEG) {
					snprintf(sd_fn_in, sizeof(sd_fn_in), "%s-%04d.jpg", ctx->sd_dataset_file_path_in, load_file_count + 1);
				}

				SD_file_load_file(&test_data_addr, &test_data_len, sd_fn_in);

				if (ctx->decode_in_place) {
					ctx->decode_in_place((char *)test_data_addr, &test_data_len); /* BMP24toRGB888planar_ConvertInPlace */
				}

				memcpy(output_item->data_addr, test_data_addr, test_data_len);
				output_item->size = test_data_len;
				output_item->timestamp = xTaskGetTickCount();
				output_item->type = AV_CODEC_ID_RGB888;

			}
			/*else if(ctx->params.codec_id == AV_CODEC_ID_XXXXX) {
			*    snprintf(sd_fn_in, sizeof(sd_fn_in), "filename", ctx->sd_dataset_file_path_in, load_file_count+1);
			*    SD_file_load_file(&test_data_addr, &test_data_len, sd_fn_in);
			*
			*    memcpy(output_item->data_addr, test_data_addr, test_data_len);
			*    output_item->size = test_data_len;
			*    output_item->timestamp = xTaskGetTickCount();
			*    output_item->type = AV_CODEC_ID_XXXXX;
			*
			*}*/

			xQueueSend(mctx->output_ready, (void *)&output_item, 0xFFFFFFFF);
			if (test_data_addr != NULL) {
				free(test_data_addr);
			}
		}
		load_file_count++;
	}
	printf("\r\nFiles loading done...\r\n");
	while (1) {
		vTaskDelay(500);
	}
}

void fileloader_task_enable(void *parm)
{
	fileloader_ctx_t *ctx = (fileloader_ctx_t *)parm;
	if (xTaskCreate(fileloader_handler, ((const char *)"fileloader_handler"), 2048, parm, tskIDLE_PRIORITY + 1, &ctx->task) != pdPASS) {
		printf("\n\r%s xTaskCreate failed", __FUNCTION__);
	}
}

/*-----------------------------------------------------------------------------------*/

static int SD_file_loader_init(void)
{
	FRESULT res;
	res = fatfs_sd_init();
	if (res < 0) {
		printf("fatfs_sd_init fail (%d)\n", res);
		return 0;
	}
	fatfs_sd_get_param(&fatfs_sd);

	return 1;
}

static void SD_file_loader_deinit(void)
{
	fatfs_sd_close();
}

static int SD_file_load_file(uint32_t *pFrame, uint32_t *pSize, char *frameFilePath)
{
	FRESULT res;
	int br;

	char path_all[64];
	memset(path_all, 0, sizeof(path_all));
	snprintf(path_all, sizeof(path_all), "%s%s", fatfs_sd.drv, frameFilePath);

	res = f_open(&m_file, path_all, FA_OPEN_ALWAYS | FA_READ | FA_WRITE);
	int file_size = f_size(&m_file);
	printf("\r\nfile_size: %d bytes.\r\n", file_size);
	char *file_buf = malloc(DEFAULT_NN_INPUT_LEN);
	if (file_buf == NULL) {
		printf("\r\nfile malloc fail!\r\n");
	}
	do {
		res = f_read(&m_file, file_buf, file_size, (u32 *)&br);
		if (res) {
			f_lseek(&m_file, 0);
			printf("\r\nRead error.\n");
		}
		printf("Read %d bytes.\n", br);
	} while (br < file_size);
	f_close(&m_file);

	*pFrame = (uint32_t) file_buf;
	*pSize = (uint32_t) file_size;

	return 1;
}

/*-----------------------------------------------------------------------------------*/

int fileloader_control(void *p, int cmd, int arg)
{
	fileloader_ctx_t *ctx = (fileloader_ctx_t *)p;

	switch (cmd) {
	case CMD_FILELOADER_SET_PARAMS:
		memcpy(&ctx->params, (void *)arg, sizeof(fileloader_params_t));
		break;
	case CMD_FILELOADER_GET_PARAMS:
		memcpy((void *)arg, &ctx->params, sizeof(fileloader_params_t));
		break;
	case CMD_FILELOADER_APPLY:
		fileloader_task_enable(ctx);
		break;
	case CMD_FILELOADER_SET_TEST_FILE_PATH:
		memset(ctx->sd_dataset_file_path_in, 0x00, sizeof(ctx->sd_dataset_file_path_in));
		memcpy((char *)ctx->sd_dataset_file_path_in, (char *)arg, strlen((char *)arg));
		break;
	case CMD_FILELOADER_SET_FILE_NUM:
		ctx->load_file_num = (int)arg;
		break;
	case CMD_FILELOADER_SET_DECODE_PROCESS:
		ctx->decode_in_place = (decode_in_place_t *)arg;
		break;
	}
	return 0;
}

int fileloader_handle(void *ctx, void *input, void *output)
{
	return 0;
}

void *fileloader_destroy(void *p)
{
	fileloader_ctx_t *ctx = (fileloader_ctx_t *)p;

	if (ctx && ctx->task) {
		vTaskDelete(ctx->task);
	}
	if (ctx) {
		free(ctx);
	}

	SD_file_loader_deinit();

	return NULL;
}

void *fileloader_create(void *parent)
{
	fileloader_ctx_t *ctx = malloc(sizeof(fileloader_ctx_t));
	if (!ctx) {
		return NULL;
	}
	memset(ctx, 0, sizeof(fileloader_ctx_t));
	ctx->parent = parent;

	SD_file_loader_init();

	return ctx;
}

void *fileloader_new_item(void *p)
{
	fileloader_ctx_t *ctx = (fileloader_ctx_t *)p;
	return (void *)malloc(DEFAULT_FILE_LEN);
}

void *fileloader_del_item(void *p, void *d)
{
	(void)p;
	if (d) {
		free(d);
	}
	return NULL;
}

mm_module_t fileloader_module = {
	.create = fileloader_create,
	.destroy = fileloader_destroy,
	.control = fileloader_control,
	.handle = fileloader_handle,

	.new_item = fileloader_new_item,
	.del_item = fileloader_del_item,

	.output_type = MM_TYPE_ASINK | MM_TYPE_ADSP | MM_TYPE_VSINK | MM_TYPE_VDSP,
	.module_type = MM_TYPE_ASRC | MM_TYPE_VSRC,
	.name = "FILE_LOADER"
};

