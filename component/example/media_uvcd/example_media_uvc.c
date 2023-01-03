/******************************************************************************
*
* Copyright(c) 2007 - 2018 Realtek Corporation. All rights reserved.
*
******************************************************************************/
#include "mmf2_module.h"
#include "module_uvcd.h"
#include "module_array.h"
#include "sample_h264.h"

#include "mmf2_link.h"
#include "mmf2_siso.h"
#include "mmf2_simo.h"
#include "mmf2_miso.h"
#include "mmf2_mimo.h"
//#include "example_media_dual_uvcd.h"


#include "module_video.h"
#include "mmf2_pro2_video_config.h"
#include "example_media_uvcd.h"
#include "platform_opts.h"
#include "flash_api.h"
#include "isp_ctrl_api.h"
#include "sensor.h"

static int wdr_mode = 2;

/*****************************************************************************
* ISP channel : 0
* Video type  : H264/HEVC
*****************************************************************************/

//#define UVC_MD
#define V1_CHANNEL 0
-#define V1_RESOLUTION VIDEO_FHD//VIDEO_HD//VIDEO_FHD 
-#define V1_FPS 20
-#define V1_GOP 80
#define V1_RESOLUTION VIDEO_1936P	//VIDEO_HD//VIDEO_FHD 
#define V1_FPS 10
#define V1_GOP 40
#define V1_BPS 1024*1024
#define V1_RCMODE 1 // 1: CBR, 2: VBR

#define V1_TYPE VIDEO_H264//VIDEO_JPEG//VIDEO_NV12//VIDEO_JPEG//VIDEO_H264//VIDEO_HEVC//VIDEO_NV16
#define VIDEO_CODEC AV_CODEC_ID_H264


/* enum encode_type {
	VIDEO_HEVC = 0,
	VIDEO_H264,
	VIDEO_JPEG,
	VIDEO_NV12,
	VIDEO_RGB,
	VIDEO_NV16,
	VIDEO_HEVC_JPEG,
	VIDEO_H264_JPEG
}; */


#if V1_RESOLUTION == VIDEO_VGA
#define V1_WIDTH	640
#define V1_HEIGHT	480
#elif V1_RESOLUTION == VIDEO_HD
#define V1_WIDTH	1280
#define V1_HEIGHT	720
#elif V1_RESOLUTION == VIDEO_1936P
#define V1_WIDTH	1936
#define V1_HEIGHT	1936
#elif V1_RESOLUTION == VIDEO_FHD

#if USE_SENSOR == SENSOR_GC4653 || USE_SENSOR == SENSOR_GC4023
#define V1_WIDTH	2560
#define V1_HEIGHT	1440
#else
#if USE_SENSOR == SENSOR_JXF51
#define V1_WIDTH	1536
#define V1_HEIGHT	1536
#else
#define V1_WIDTH	1920
#define V1_HEIGHT	1080
#endif
#endif

#endif

static mm_context_t *video_v1_ctx			= NULL;
static mm_context_t *rtsp2_v1_ctx			= NULL;
static mm_siso_t *siso_video_rtsp_v1			= NULL;

static video_params_t video_v1_params = {
	.stream_id = V1_CHANNEL,
	.type = V1_TYPE,
	.resolution = V1_RESOLUTION,
	.width = V1_WIDTH,
	.height = V1_HEIGHT,
	.fps = V1_FPS,
	.gop = V1_GOP,
	.bps = V1_BPS,
	.rc_mode = V1_RCMODE,
	.use_static_addr = 1
};

/* static video_params_t video_v3_params = {
	.stream_id = VIDEO_CHANNEL,
	.type = VIDEO_JPEG,
	.resolution = VIDEO_RESOLUTION,
	.fps = VIDEO_FPS
}; */


mm_context_t *uvcd_ctx         = NULL;
mm_siso_t *siso_array_uvcd     = NULL;
mm_context_t *array_h264_ctx   = NULL;


extern struct uvc_format *uvc_format_ptr;

struct uvc_format *uvc_format_local = NULL;;

typedef void (*shell_function_t)(void);
typedef struct uvc_shell_cmd_s {
	uint8_t *name;
	shell_function_t function;
	uint8_t *desc;
} uvc_shell_cmd_t;
static char log_buf[64] = {0};
static char msg_buf[64] = {0};//For the command to return the buffer.
void shelltest1(void)
{
	printf("%s\r\n", __FUNCTION__);
	for (int i = 0; i < 64; i++) { //Copy the memory content to the uvc buffer
		msg_buf[i] = i;
	}
}

void shelltest2(void)
{
	printf("%s\r\n", __FUNCTION__);
}

uvc_shell_cmd_t shellCommandList[] = {
	/*command               function                description*/
	{(uint8_t *)"test",   shelltest1, (uint8_t *)"test1 shell"},
	{(uint8_t *)"test2",  shelltest2, (uint8_t *)"test2 shell"},

};
_sema usbd_uvc_sema;
void example_shell_cmd_thread(void *param)
{
	int flag = 0;
	rtw_init_sema(&usbd_uvc_sema, 0);
	while (1) {
		flag = 0;
		rtw_down_sema(&usbd_uvc_sema);
		for (int i = sizeof(shellCommandList) / sizeof(uvc_shell_cmd_t) - 1; i >=  0; i--) {
			if (strcmp((const char *)log_buf, (const char *)shellCommandList[i].name) == 0) {
				flag = 1;
				shellCommandList[i].function();
				break;
			}
		}
		if (!flag) {
			printf("The command is not support\r\n");
		}
	}
}

void uvcd_set_extention_cb(void *buf, unsigned int len) //Host set the data to device(Node A)
{
	int i = 0;
	unsigned char *ptr = (unsigned char *)buf;
	memcpy(log_buf, buf, len);
	rtw_up_sema(&usbd_uvc_sema);
}
void uvcd_get_extention_cb(void *buf, unsigned int len) //Host get the data from device(Node B)
{
	memcpy(buf, (void *)msg_buf, sizeof(msg_buf));
}


#define AVMEDIA_TYPE_VIDEO 0
#define AV_CODEC_ID_H264  1
#define ARRAY_MODE_LOOP		1
static array_params_t h264usb_array_params = {
	.type = AVMEDIA_TYPE_VIDEO,
	.codec_id = AV_CODEC_ID_H264,
	.mode = ARRAY_MODE_LOOP,
	.u = {
		.v = {
			.fps    = 25,
			.h264_nal_size = 4,
		}
	}
};

#if CONFIG_TUNING

#include "mmf2_link.h"
#include "mmf2_siso.h"
#include "module_video.h"
#include "module_rtsp2.h"
#include "mmf2_pro2_video_config.h"
#include "video_example_media_framework.h"
#include "log_service.h"

#define V2_CHANNEL 1
#define V2_RESOLUTION VIDEO_VGA //max resolution: FHD
#define V2_FPS 5
#define V2_GOP 5
#define V2_BPS 1*1024*1024
#define V2_RCMODE 2 // 1: CBR, 2: VBR

#include "sample_h264.h"
#define VIDEO_TYPE VIDEO_H264
#define VIDEO_CODEC AV_CODEC_ID_H264

#if V2_RESOLUTION == VIDEO_VGA
#define V2_WIDTH	640
#define V2_HEIGHT	480
#elif V2_RESOLUTION == VIDEO_HD
#define V2_WIDTH	1280
#define V2_HEIGHT	720
#elif V2_RESOLUTION == VIDEO_FHD
#define V2_WIDTH	1920
#define V2_HEIGHT	1080
#endif

static mm_context_t *video_v2_ctx			= NULL;
static mm_context_t *rtsp2_v2_ctx			= NULL;
static mm_siso_t *siso_video_rtsp_v2			= NULL;

static video_params_t video_v2_params = {
	.stream_id = V2_CHANNEL,
	.type = VIDEO_TYPE,
	.resolution = V2_RESOLUTION,
	.width = V2_WIDTH,
	.height = V2_HEIGHT,
	.bps = V2_BPS,
	.fps = V2_FPS,
	.gop = V2_GOP,
	.rc_mode = V2_RCMODE,
	.use_static_addr = 1
};


static rtsp2_params_t rtsp2_v2_params = {
	.type = AVMEDIA_TYPE_VIDEO,
	.u = {
		.v = {
			.codec_id = VIDEO_CODEC,
			.fps      = V2_FPS,
			.bps      = V2_BPS
		}
	}
};


void example_tuning_rtsp_video_v2_init(void)
{
	video_v2_ctx = mm_module_open(&video_module);
	if (video_v2_ctx) {
		mm_module_ctrl(video_v2_ctx, CMD_VIDEO_SET_PARAMS, (int)&video_v2_params);
		mm_module_ctrl(video_v2_ctx, MM_CMD_SET_QUEUE_LEN, V2_FPS * 3);
		mm_module_ctrl(video_v2_ctx, MM_CMD_INIT_QUEUE_ITEMS, MMQI_FLAG_DYNAMIC);
	} else {
		rt_printf("video open fail\n\r");
		goto mmf2_video_exmaple_v2_fail;
	}

	rtsp2_v2_ctx = mm_module_open(&rtsp2_module);
	if (rtsp2_v2_ctx) {
		mm_module_ctrl(rtsp2_v2_ctx, CMD_RTSP2_SELECT_STREAM, 0);
		mm_module_ctrl(rtsp2_v2_ctx, CMD_RTSP2_SET_PARAMS, (int)&rtsp2_v2_params);
		mm_module_ctrl(rtsp2_v2_ctx, CMD_RTSP2_SET_APPLY, 0);
		mm_module_ctrl(rtsp2_v2_ctx, CMD_RTSP2_SET_STREAMMING, ON);
	} else {
		rt_printf("RTSP2 open fail\n\r");
		goto mmf2_video_exmaple_v2_fail;
	}

	siso_video_rtsp_v2 = siso_create();
	if (siso_video_rtsp_v2) {
#if defined(configENABLE_TRUSTZONE) && (configENABLE_TRUSTZONE == 1)
		siso_ctrl(siso_video_rtsp_v2, MMIC_CMD_SET_SECURE_CONTEXT, 1, 0);
#endif
		siso_ctrl(siso_video_rtsp_v2, MMIC_CMD_ADD_INPUT, (uint32_t)video_v2_ctx, 0);
		siso_ctrl(siso_video_rtsp_v2, MMIC_CMD_ADD_OUTPUT, (uint32_t)rtsp2_v2_ctx, 0);
		siso_start(siso_video_rtsp_v2);
	} else {
		rt_printf("siso2 open fail\n\r");
		goto mmf2_video_exmaple_v2_fail;
	}

	mm_module_ctrl(video_v2_ctx, CMD_VIDEO_APPLY, V2_CHANNEL);

	return;
mmf2_video_exmaple_v2_fail:

	return;
}


static unsigned char *g_uvcd_iq = NULL;
#include "ftl_common_api.h"
#include "../../usb/usb_class/device/class/uvc/tuning-server.h"

void detect_iq_fw(int *iq_bin_start)
{
	int fw_size = 0;
	tuning_set_iq_heap(iq_bin_start);

	ftl_common_read(TUNING_IQ_FW, (u8 *) &fw_size, sizeof(int));
	video_set_uvcd_iq((unsigned int)g_uvcd_iq);
	if (fw_size > 15 * 1024 && fw_size < TUNING_IQ_MAX_SIZE) {
		ftl_common_read(TUNING_IQ_FW, (u8 *) iq_bin_start, sizeof(int) + fw_size);

		printf("[%s] fw_size: %d.\r\n", __FUNCTION__, fw_size);
	} else {
		printf("IQ is not in 0x%06X\r\n", TUNING_IQ_FW);
	}
}
#endif
void example_media_uvcd_init(void)
{
#ifdef UVC_MD
	int md_roi[6] = {0, 0, 320, 180, 6, 6};
#endif

#if 0//def JXF51_CSTM
	static gpio_t gpio_power_enable;
	gpio_init(&gpio_power_enable, PIN_F10);
	gpio_dir(&gpio_power_enable, PIN_OUTPUT);
	gpio_mode(&gpio_power_enable, PullNone);
	gpio_write(&gpio_power_enable, 0);

#endif
	int voe_heap_size = video_voe_presetting(1, V1_WIDTH, V1_HEIGHT, V1_BPS, 0,
#if CONFIG_TUNING
						1, 1280, 720, 1 * 1024 * 1024, 0,
#else
						0, 0, 0, 0, 0,
#endif
						0, 0, 0, 0, 0,
						0, 0, 0);
	printf("\r\n voe heap size = %d\r\n", voe_heap_size);

#if CONFIG_TUNING
	usbd_ext_init();
	tuning_init();
	if (g_uvcd_iq == NULL) {
		g_uvcd_iq = malloc(TUNING_IQ_MAX_SIZE);
		memset(g_uvcd_iq, 0, TUNING_IQ_MAX_SIZE);
		detect_iq_fw((int *)g_uvcd_iq);
	}

	tuning_set_max_resolution(MAX_W, MAX_H);
#else
	printf("[uvcd] not CONFIG_TUNING.\r\n");
#endif

	uvc_format_ptr = (struct uvc_format *)malloc(sizeof(struct uvc_format));
	memset(uvc_format_ptr, 0, sizeof(struct uvc_format));

	uvc_format_local = (struct uvc_format *)malloc(sizeof(struct uvc_format));
	memset(uvc_format_local, 0, sizeof(struct uvc_format));

	rtw_init_sema(&uvc_format_ptr->uvcd_change_sema, 0);

	printf("type = %d\r\n", V1_TYPE);

	uvcd_ctx = mm_module_open(&uvcd_module);
	//  struct uvc_dev *uvc_ctx = (struct uvc_dev *)uvcd_ctx->priv;

	if (uvcd_ctx) {
		mm_module_ctrl(uvcd_ctx, CMD_UVCD_CALLBACK_SET, (int)uvcd_set_extention_cb);
		mm_module_ctrl(uvcd_ctx, CMD_UVCD_CALLBACK_GET, (int)uvcd_get_extention_cb);
	} else {
		rt_printf("uvcd open fail\n\r");
		goto mmf2_example_uvcd_fail;
	}
	//

	//vTaskDelay(2000);

	uvc_format_ptr->format = FORMAT_TYPE_H264;
	uvc_format_ptr->height = MAX_H;//video_v1_params.height;
	uvc_format_ptr->width = MAX_W;//video_v1_params.width;

	uvc_format_local->format = FORMAT_TYPE_H264;
	uvc_format_local->height = MAX_H;//video_v1_params.height;
	uvc_format_local->width = MAX_W;//video_v1_params.width;

	printf("foramr %d height %d width %d\r\n", uvc_format_local->format, uvc_format_local->height, uvc_format_local->width);

	video_v1_ctx = mm_module_open(&video_module);
	if (video_v1_ctx) {
		mm_module_ctrl(video_v1_ctx, CMD_VIDEO_SET_VOE_HEAP, voe_heap_size);
		mm_module_ctrl(video_v1_ctx, CMD_VIDEO_SET_PARAMS, (int)&video_v1_params);
		mm_module_ctrl(video_v1_ctx, MM_CMD_SET_QUEUE_LEN, 1);//Default 30
		mm_module_ctrl(video_v1_ctx, MM_CMD_INIT_QUEUE_ITEMS, MMQI_FLAG_DYNAMIC);
		mm_module_ctrl(video_v1_ctx, CMD_VIDEO_APPLY, V1_CHANNEL);	// start channel 0
#ifdef UVC_MD
		mm_module_ctrl(video_v1_ctx, CMD_VIDEO_MD_SET_ROI, md_roi);
		mm_module_ctrl(video_v1_ctx, CMD_VIDEO_MD_SET_SENSITIVITY, 7);
		mm_module_ctrl(video_v1_ctx, CMD_VIDEO_MD_START, 0);
#endif
	} else {
		rt_printf("video open fail\n\r");
		//goto mmf2_video_exmaple_v1_fail;
	}

	//vTaskDelay(2000);

	siso_array_uvcd = siso_create();
	if (siso_array_uvcd) {
#if defined(configENABLE_TRUSTZONE) && (configENABLE_TRUSTZONE == 1)
		siso_ctrl(siso_array_uvcd, MMIC_CMD_SET_SECURE_CONTEXT, 1, 0);
#endif
		siso_ctrl(siso_array_uvcd, MMIC_CMD_ADD_INPUT, (uint32_t)video_v1_ctx, 0);
		siso_ctrl(siso_array_uvcd, MMIC_CMD_ADD_OUTPUT, (uint32_t)uvcd_ctx, 0);
		siso_start(siso_array_uvcd);
	} else {
		rt_printf("siso_array_uvcd open fail\n\r");
		//goto mmf2_example_h264_array_rtsp_fail;
	}
	rt_printf("siso_array_uvcd started\n\r");
	printf("VIDEO_TYPE %d\r\n", V1_TYPE);
	if (V1_TYPE == VIDEO_JPEG) {
		printf("VIDEO_JPEG\r\n");
		mm_module_ctrl(video_v1_ctx, CMD_VIDEO_SNAPSHOT, 2);
	} else if (V1_TYPE == VIDEO_NV12) {
		printf("VIDEO_NV12\r\n");
		mm_module_ctrl(video_v1_ctx, CMD_VIDEO_YUV, 2);
	} else if (V1_TYPE == VIDEO_NV12) {
		printf("VIDEO_NV16\r\n");
		mm_module_ctrl(video_v1_ctx, CMD_VIDEO_YUV, 2);
	}

	wdr_mode = 2;
	printf("[uvcd]set wdr_mode:%d.\r\n", wdr_mode);
	isp_set_wdr_mode(wdr_mode);
	isp_get_wdr_mode(&wdr_mode);
	printf("[uvcd]get wdr_mode:%d.\r\n", wdr_mode);

	while (1) {
		rtw_down_sema(&uvc_format_ptr->uvcd_change_sema);

		printf("f:%d h:%d s:%d w:%d\r\n", uvc_format_ptr->format, uvc_format_ptr->height, uvc_format_ptr->state, uvc_format_ptr->width);

		if ((uvc_format_local->format != uvc_format_ptr->format) || (uvc_format_local->width != uvc_format_ptr->width) ||
			(uvc_format_local->height != uvc_format_ptr->height)) {
			printf("change f:%d h:%d s:%d w:%d\r\n", uvc_format_ptr->format, uvc_format_ptr->height, uvc_format_ptr->state, uvc_format_ptr->width);

			if (uvc_format_ptr->format == FORMAT_TYPE_YUY2) {
#ifdef UVC_MD
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_MD_STOP, 0);
#endif
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_STREAM_STOP, 0);
				vTaskDelay(1000);
				siso_pause(siso_array_uvcd);
				vTaskDelay(1000);
				video_v1_params.type = VIDEO_NV16;
				video_v1_params.use_static_addr = 1;
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_SET_PARAMS, (int)&video_v1_params);
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_APPLY, V1_CHANNEL);	// start channel 0
#ifdef UVC_MD
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_MD_SET_ROI, md_roi);
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_MD_SET_SENSITIVITY, 7);
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_MD_START, 0);
#endif
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_YUV, 2);
				siso_resume(siso_array_uvcd);
			} else if (uvc_format_ptr->format == FORMAT_TYPE_NV12) {
#ifdef UVC_MD
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_MD_STOP, 0);
#endif
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_STREAM_STOP, 0);
				vTaskDelay(1000);
				siso_pause(siso_array_uvcd);
				vTaskDelay(1000);
				video_v1_params.type = VIDEO_NV12;
				video_v1_params.use_static_addr = 1;
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_SET_PARAMS, (int)&video_v1_params);
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_APPLY, V1_CHANNEL);	// start channel 0
#ifdef UVC_MD
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_MD_SET_ROI, md_roi);
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_MD_SET_SENSITIVITY, 7);
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_MD_START, 0);
#endif
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_YUV, 2);
				siso_resume(siso_array_uvcd);
			} else if (uvc_format_ptr->format == FORMAT_TYPE_H264) {
#ifdef UVC_MD
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_MD_STOP, 0);
#endif
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_STREAM_STOP, 0);
				vTaskDelay(1000);
				printf("siso pause\r\n");
				siso_pause(siso_array_uvcd);
				vTaskDelay(100);
				video_v1_params.type = VIDEO_H264;
				video_v1_params.use_static_addr = 1;
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_SET_PARAMS, (int)&video_v1_params);
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_APPLY, V1_CHANNEL);	// start channel 0
#ifdef UVC_MD
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_MD_SET_ROI, md_roi);
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_MD_SET_SENSITIVITY, 7);
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_MD_START, 0);
#endif
				siso_resume(siso_array_uvcd);
			} else if (uvc_format_ptr->format == FORMAT_TYPE_MJPEG) {
#ifdef UVC_MD
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_MD_STOP, 0);
#endif
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_STREAM_STOP, 0);
				vTaskDelay(1000);
				siso_pause(siso_array_uvcd);
				vTaskDelay(1000);
				video_v1_params.type = VIDEO_JPEG;
				video_v1_params.use_static_addr = 1;
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_SET_PARAMS, (int)&video_v1_params);
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_APPLY, V1_CHANNEL);	// start channel 0
#ifdef UVC_MD
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_MD_SET_ROI, md_roi);
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_MD_SET_SENSITIVITY, 7);
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_MD_START, 0);
#endif
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_SNAPSHOT, 2);
				siso_resume(siso_array_uvcd);
			} else if (uvc_format_ptr->format == FORMAT_TYPE_H265) {
#ifdef UVC_MD
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_MD_STOP, 0);
#endif
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_STREAM_STOP, 0);
				vTaskDelay(1000);
				printf("siso pause\r\n");
				siso_pause(siso_array_uvcd);
				vTaskDelay(100);
				video_v1_params.type = VIDEO_HEVC;
				video_v1_params.use_static_addr = 1;
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_SET_PARAMS, (int)&video_v1_params);
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_APPLY, V1_CHANNEL);	// start channel 0
#ifdef UVC_MD
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_MD_SET_ROI, md_roi);
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_MD_SET_SENSITIVITY, 7);
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_MD_START, 0);
#endif
				siso_resume(siso_array_uvcd);
			}
			uvc_format_local->format = uvc_format_ptr->format;
			uvc_format_local->width = uvc_format_ptr->width;
			uvc_format_local->height = uvc_format_ptr->height;
		}

	}

mmf2_example_uvcd_fail:

#if CONFIG_TUNING
	tuning_deinit();
#endif
	return;
}

void example_media_uvcd_main(void *param)
{
#if defined(configENABLE_TRUSTZONE) && (configENABLE_TRUSTZONE == 1) && defined(CONFIG_PLATFORM_8735B)
	rtw_create_secure_context(configMINIMAL_SECURE_STACK_SIZE);
#endif
	example_media_uvcd_init();
	// TODO: exit condition or signal
	while (1) {
		vTaskDelay(1000);
	}
}

void example_media_uvcd(void)
{
	/*user can start their own task here*/
	if (xTaskCreate(example_media_uvcd_main, ((const char *)"example_media_dual_uvcd_main"), 4096, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
		printf("\r\n example_media_two_source_main: Create Task Error\n");
	}

	if (xTaskCreate(example_shell_cmd_thread, ((const char *)"example_shell_cmd_thread"), 2048, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
		printf("\n\r%s xTaskCreate(example_shell_cmd_thread) failed", __FUNCTION__);
	}
}
