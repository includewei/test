/******************************************************************************
*
* Copyright(c) 2007 - 2018 Realtek Corporation. All rights reserved.
*
******************************************************************************/
#include "basic_types.h"
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
//#define UVC_EXT_IQ
#define VIDEO_CHANNEL 0
#define VIDEO_RESOLUTION VIDEO_FHD//VIDEO_HD//VIDEO_FHD 
#define VIDEO_FPS 30
#define VIDEO_GOP 30
#define VIDEO_BPS 1024*1024
#define VIDEO_RCMODE 1 // 1: CBR, 2: VBR

#define VIDEO_TYPE VIDEO_NV16//VIDEO_JPEG//VIDEO_NV12//VIDEO_JPEG//VIDEO_H264//VIDEO_HEVC//VIDEO_NV16
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


#if VIDEO_RESOLUTION == VIDEO_VGA
#define VIDEO_WIDTH	640
#define VIDEO_HEIGHT	480
#elif VIDEO_RESOLUTION == VIDEO_HD
#define VIDEO_WIDTH	1280
#define VIDEO_HEIGHT	720
#elif VIDEO_RESOLUTION == VIDEO_FHD

#if USE_SENSOR == SENSOR_GC4653
#define VIDEO_WIDTH	2560
#define VIDEO_HEIGHT	1440
#else
#define VIDEO_WIDTH	1920
#define VIDEO_HEIGHT	1080
#endif

#endif

static mm_context_t *video_v1_ctx			= NULL;
static mm_context_t *rtsp2_v1_ctx			= NULL;
static mm_siso_t *siso_video_rtsp_v1			= NULL;

static video_params_t video_v1_params = {
	.stream_id = VIDEO_CHANNEL,
	.type = VIDEO_TYPE,
	.resolution = VIDEO_RESOLUTION,
	.width = VIDEO_WIDTH,
	.height = VIDEO_HEIGHT,
	.fps = VIDEO_FPS,
	.gop = VIDEO_GOP,
	.bps = VIDEO_BPS,
	.rc_mode = VIDEO_RCMODE,
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

#ifdef UVC_EXT_IQ

extern void set_iq_heap(void *iq_heap);
extern void set_max_resolution(int width, int height);
unsigned char *g_uvcd_iq = NULL;
#include "ftl_common_api.h"
#define IQ_FW_ADDR (0xF00000)
void detect_iq_fw(int *iq_bin_start)
{
	int fw_size = 0;
	set_iq_heap(iq_bin_start);

	ftl_common_read(IQ_FW_ADDR, (u8 *) &fw_size, sizeof(int));
	if (fw_size > 15 * 1024 && fw_size < 65536) {
		ftl_common_read(IQ_FW_ADDR, (u8 *) iq_bin_start, sizeof(int) + fw_size);
		printf("[%s] fw_size: %d.\r\n", __FUNCTION__, fw_size);
	} else {
		printf("IQ is not in 0x%X\r\n", IQ_FW_ADDR);
	}
}
#endif
void example_media_dual_uvcd_init(void)
{
#ifdef UVC_MD
	int md_roi[6] = {0, 0, 320, 180, 6, 6};
#endif

	int voe_heap_size = video_voe_presetting(1, VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_BPS, 1,
						0, 0, 0, 0, 0,
						0, 0, 0, 0, 0,
						0, 0, 0);
	printf("\r\n voe heap size = %d\r\n", voe_heap_size);

#ifdef UVC_EXT_IQ
	if (g_uvcd_iq == NULL) {
		g_uvcd_iq = malloc(65536);
		memset(g_uvcd_iq, 0, 65536);
		detect_iq_fw((int *)g_uvcd_iq);
	}

	set_max_resolution(MAX_W, MAX_H);
#endif

	uvc_format_ptr = (struct uvc_format *)malloc(sizeof(struct uvc_format));
	memset(uvc_format_ptr, 0, sizeof(struct uvc_format));

	uvc_format_local = (struct uvc_format *)malloc(sizeof(struct uvc_format));
	memset(uvc_format_local, 0, sizeof(struct uvc_format));

	rtw_init_sema(&uvc_format_ptr->uvcd_change_sema, 0);

	printf("type = %d\r\n", VIDEO_TYPE);

	uvcd_ctx = mm_module_open(&uvcd_module);
	//  struct uvc_dev *uvc_ctx = (struct uvc_dev *)uvcd_ctx->priv;

	if (uvcd_ctx) {
		//mm_module_ctrl(uvcd_ctx, CMD_RTSP2_SET_APPLY, 0);
		//mm_module_ctrl(uvcd_ctx, CMD_RTSP2_SET_STREAMMING, ON);
	} else {
		rt_printf("uvcd open fail\n\r");
		goto mmf2_example_uvcd_fail;
	}
	//

	vTaskDelay(2000);

	uvc_format_ptr->format = FORMAT_TYPE_YUY2;
	uvc_format_ptr->height = MAX_H;//video_v1_params.height;
	uvc_format_ptr->width = MAX_W;//video_v1_params.width;
	//uvc_format_ptr->uvcd_ext_get_cb = NULL;//RTKUSER_USB_GET;
	//uvc_format_ptr->uvcd_ext_set_cb = NULL;//RTKUSER_USB_SET;

	uvc_format_local->format = FORMAT_TYPE_YUY2;
	uvc_format_local->height = MAX_H;//video_v1_params.height;
	uvc_format_local->width = MAX_W;//video_v1_params.width;

	printf("foramr %d height %d width %d\r\n", uvc_format_local->format, uvc_format_local->height, uvc_format_local->width);

	video_v1_ctx = mm_module_open(&video_module);
	if (video_v1_ctx) {
		mm_module_ctrl(video_v1_ctx, CMD_VIDEO_SET_VOE_HEAP, voe_heap_size);
		mm_module_ctrl(video_v1_ctx, CMD_VIDEO_SET_PARAMS, (int)&video_v1_params);
		mm_module_ctrl(video_v1_ctx, MM_CMD_SET_QUEUE_LEN, 1);//Default 30
		mm_module_ctrl(video_v1_ctx, MM_CMD_INIT_QUEUE_ITEMS, MMQI_FLAG_DYNAMIC);
		mm_module_ctrl(video_v1_ctx, CMD_VIDEO_APPLY, VIDEO_CHANNEL);	// start channel 0
#ifdef UVC_MD
		mm_module_ctrl(video_v1_ctx, CMD_VIDEO_MD_SET_ROI, md_roi);
		mm_module_ctrl(video_v1_ctx, CMD_VIDEO_MD_SET_SENSITIVITY, 7);
		mm_module_ctrl(video_v1_ctx, CMD_VIDEO_MD_START, 0);
#endif
	} else {
		rt_printf("video open fail\n\r");
		//goto mmf2_video_exmaple_v1_fail;
	}

	vTaskDelay(2000);

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

#if (VIDEO_TYPE == VIDEO_JPEG)
	mm_module_ctrl(video_v1_ctx, CMD_VIDEO_SNAPSHOT, 2);
#endif

#if (VIDEO_TYPE == VIDEO_NV12)
	mm_module_ctrl(video_v1_ctx, CMD_VIDEO_YUV, 2);
#endif

#if (VIDEO_TYPE == VIDEO_NV16)
	mm_module_ctrl(video_v1_ctx, CMD_VIDEO_YUV, 2);
#endif

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
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_APPLY, VIDEO_CHANNEL);	// start channel 0
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
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_APPLY, VIDEO_CHANNEL);	// start channel 0
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
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_APPLY, VIDEO_CHANNEL);	// start channel 0
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
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_APPLY, VIDEO_CHANNEL);	// start channel 0
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
				mm_module_ctrl(video_v1_ctx, CMD_VIDEO_APPLY, VIDEO_CHANNEL);	// start channel 0
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

	return;
}

void example_media_uvcd_main(void *param)
{
#if defined(configENABLE_TRUSTZONE) && (configENABLE_TRUSTZONE == 1) && defined(CONFIG_PLATFORM_8735B)
	rtw_create_secure_context(configMINIMAL_SECURE_STACK_SIZE);
#endif
	example_media_dual_uvcd_init();
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
}
