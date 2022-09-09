/******************************************************************************
*
* Copyright(c) 2007 - 2018 Realtek Corporation. All rights reserved.
*
******************************************************************************/
#include "mmf2_link.h"
#include "mmf2_siso.h"

#include "module_array.h"
#include "module_vipnn.h"

#include "avcodec.h"

#include "input_image_640x360x3.h"

#include "model_mbnetssd.h"
#include "model_yolov3t.h"

#include "module_video.h"
#include "mmf2_pro2_video_config.h"


#define NN_CHANNEL 4
//#define NN_RESOLUTION VIDEO_WVGA
#define NN_RESOLUTION VIDEO_VGA
#define NN_FPS 2
#define NN_GOP 2
#define NN_BPS 256*1024

#define NN_TYPE VIDEO_RGB

static video_params_t video_v4_params = {
	.stream_id 		= NN_CHANNEL,
	.type 			= NN_TYPE,
	.resolution	 	= NN_RESOLUTION,
	.fps 			= NN_FPS,
	.gop 			= NN_GOP,
	.direct_output 	= 0,
	.use_static_addr = 1,
	.out_buf_size = 1024 * 1024 * 2,
	.out_rsvd_size = 384 * 1024
};

static mm_context_t *video_rgb_ctx			= NULL;

static mm_context_t *array_ctx            = NULL;
static mm_context_t *vipnn_ctx            = NULL;
static mm_siso_t *siso_array_vipnn         = NULL;

static array_params_t rgb_array_params = {
	.type = AVMEDIA_TYPE_VIDEO,
	.codec_id = AV_CODEC_ID_RGB888,
	.mode = ARRAY_MODE_LOOP,
	.u = {
		.v = {
			.fps    = 30,
		}
	}
};

#if 0
static vipnn_params_t vipnn_ssd_params = {
	//.model_mem = mobilenet_ssd_uint8,
	//.model_size = mobilenet_ssd_uint8_size,

	.m_width = 300,
	.m_height = 300,
	.in_width = 640,
	.in_height = 360,

	.roi = {
		.xmin = 0,
		.ymin = 0,
		.xmax = 640,
		.ymax = 360,
	}
};

static vipnn_params_t vipnn_yolo_params = {
	//.model_mem = mobilenet_ssd_uint8,
	//.model_size = mobilenet_ssd_uint8_size,

	.m_width = 416,
	.m_height = 416,
	.in_width = 416,
	.in_height = 416,

	.roi = {
		.xmin = 0,
		.ymin = 0,
		.xmax = 416,
		.ymax = 416,
	}

};
#endif

#define USE_ARRAY 1

extern const unsigned char testRGB_416x416[];

nn_data_param_t test_wvga = {
	.img = {
		.width = 640,
		.height = 360,
		.rgb = 1,
		.roi = {
			.xmin = 0,
			.ymin = 0,
			.xmax = 640,
			.ymax = 352,
		}
	}
};

nn_data_param_t test_416 = {
	.img = {
		.width = 416,
		.height = 416,
		.rgb = 0,
		.roi = {
			.xmin = 0,
			.ymin = 0,
			.xmax = 416,
			.ymax = 416,
		}
	}
};

void mmf2_example_array_vipnn_init(void)
{
#if USE_ARRAY
	// Video array input (H264)
	array_t array;
	array.data_addr = (uint32_t) testRGB_640x360;
	array.data_len = (uint32_t) 640 * 360 * 3;

	//array.data_addr = (uint32_t) testRGB_416x416;
	//array.data_len = (uint32_t) 416*416*3;

	array_ctx = mm_module_open(&array_module);
	if (array_ctx) {
		mm_module_ctrl(array_ctx, CMD_ARRAY_SET_PARAMS, (int)&rgb_array_params);
		mm_module_ctrl(array_ctx, CMD_ARRAY_SET_ARRAY, (int)&array);
		mm_module_ctrl(array_ctx, MM_CMD_SET_QUEUE_LEN, 6);
		mm_module_ctrl(array_ctx, MM_CMD_INIT_QUEUE_ITEMS, MMQI_FLAG_DYNAMIC);
		mm_module_ctrl(array_ctx, CMD_ARRAY_APPLY, 0);
		mm_module_ctrl(array_ctx, CMD_ARRAY_STREAMING, 1);	// streamming on
	} else {
		rt_printf("ARRAY open fail\n\r");
		goto mmf2_example_array_vipnn_fail;
	}
#else
	video_rgb_ctx = mm_module_open(&video_module);
	if (video_rgb_ctx) {
		mm_module_ctrl(video_rgb_ctx, CMD_VIDEO_SET_VOE_HEAP, 32 * 1024 * 1024);
		mm_module_ctrl(video_rgb_ctx, CMD_VIDEO_SET_PARAMS, (int)&video_v4_params);
		mm_module_ctrl(video_rgb_ctx, MM_CMD_SET_QUEUE_LEN, 2);
		mm_module_ctrl(video_rgb_ctx, MM_CMD_INIT_QUEUE_ITEMS, MMQI_FLAG_DYNAMIC);
		mm_module_ctrl(video_rgb_ctx, CMD_VIDEO_APPLY, NN_CHANNEL);	// start channel 4
	} else {
		rt_printf("video open fail\n\r");
		goto mmf2_example_array_vipnn_fail;
	}
	mm_module_ctrl(video_rgb_ctx, CMD_VIDEO_YUV, 2);
#endif
	// vipnn
	vipnn_ctx = mm_module_open(&vipnn_module);
	if (vipnn_ctx) {
		//mm_module_ctrl(vipnn_ctx, CMD_VIPNN_SET_MODEL, (int)&mbnetssd);
		//mm_module_ctrl(vipnn_ctx, CMD_VIPNN_SET_IN_PARAMS, (int)&test_wvga);

		mm_module_ctrl(vipnn_ctx, CMD_VIPNN_SET_MODEL, (int)&yolov3_tiny);
		//mm_module_ctrl(vipnn_ctx, CMD_VIPNN_SET_IN_PARAMS, (int)&test_416);
		mm_module_ctrl(vipnn_ctx, CMD_VIPNN_SET_IN_PARAMS, (int)&test_wvga);

		mm_module_ctrl(vipnn_ctx, CMD_VIPNN_APPLY, 0);

	} else {
		rt_printf("vipnn open fail\n\r");
		goto mmf2_example_array_vipnn_fail;
	}
	rt_printf("vipnn opened\n\r");

	//--------------Link---------------------------
	siso_array_vipnn = siso_create();
	if (siso_array_vipnn) {
#if USE_ARRAY
		siso_ctrl(siso_array_vipnn, MMIC_CMD_ADD_INPUT, (uint32_t)array_ctx, 0);
#else
		siso_ctrl(siso_array_vipnn, MMIC_CMD_ADD_INPUT, (uint32_t)video_rgb_ctx, 0);
#endif
		siso_ctrl(siso_array_vipnn, MMIC_CMD_ADD_OUTPUT, (uint32_t)vipnn_ctx, 0);
		siso_start(siso_array_vipnn);
	} else {
		rt_printf("siso_array_vipnn open fail\n\r");
		goto mmf2_example_array_vipnn_fail;
	}
	rt_printf("siso_array_vipnn started\n\r");

	return;
mmf2_example_array_vipnn_fail:

	return;
}
