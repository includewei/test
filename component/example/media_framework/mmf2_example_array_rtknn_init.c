/******************************************************************************
*
* Copyright(c) 2007 - 2018 Realtek Corporation. All rights reserved.
*
******************************************************************************/
#include "mmf2_link.h"
#include "mmf2_siso.h"

#include "module_array.h"
#include "module_rtknn.h"

#include "avcodec.h"

#include "input_image_640x360x3.h"
// TODO: move model id to proper header
#include "nn_model_info_ddr.h"
#include "mobilenet_ssd_uint8.h"
#include "face300x300_uint8.h"


static mm_context_t *array_ctx            = NULL;
static mm_context_t *rtknn_ctx            = NULL;
static mm_siso_t *siso_array_rtknn         = NULL;

static array_params_t h264_array_params = {
	.type = AVMEDIA_TYPE_VIDEO,
	.codec_id = AV_CODEC_ID_RGB888,
	.mode = ARRAY_MODE_LOOP,
	.u = {
		.v = {
			.fps    = 7,
		}
	}
};

#if MODEL_SSDM_USED
static rtknn_params_t rtknn_ssd_params = {
	.model_id = MOBILENETSSD_20OBJ,	// for built-in postbuild
	.model = mobilenet_ssd_uint8,
	.m_width = NNMODEL_SM_IMG_WIDTH,
	.m_height = NNMODEL_SM_IMG_HEIGHT,

	.roi = {
		.xmin = 0,
		.ymin = 0,
		.xmax = 640,
		.ymax = 360,
	}
};
#endif

#if MODEL_FACE_USED
static rtknn_params_t rtknn_face_params = {
	.model_id = MODEL_FACE,			// for built-in postbuild
	.model = face_300x300_uint8,
	.m_width = NNMODEL_FACE_IMG_WIDTH,
	.m_height = NNMODEL_FACE_IMG_HEIGHT,

	.roi = {
		.xmin = 320,
		.ymin = 0,
		.xmax = 640,
		.ymax = 360,
	}
};
#endif

void mmf2_example_array_rtknn_init(void)
{
	// Video array input (H264)
	array_t array;
	array.data_addr = (uint32_t) testRGB_640x360;
	array.data_len = (uint32_t) testRGB_640x360_size;
	array_ctx = mm_module_open(&array_module);
	if (array_ctx) {
		mm_module_ctrl(array_ctx, CMD_ARRAY_SET_PARAMS, (int)&h264_array_params);
		mm_module_ctrl(array_ctx, CMD_ARRAY_SET_ARRAY, (int)&array);
		mm_module_ctrl(array_ctx, MM_CMD_SET_QUEUE_LEN, 6);
		mm_module_ctrl(array_ctx, MM_CMD_INIT_QUEUE_ITEMS, MMQI_FLAG_DYNAMIC);
		mm_module_ctrl(array_ctx, CMD_ARRAY_APPLY, 0);
		mm_module_ctrl(array_ctx, CMD_ARRAY_STREAMING, 1);	// streamming on
	} else {
		rt_printf("ARRAY open fail\n\r");
		goto mmf2_example_array_rtknn_fail;
	}

	// RTKNN
	rtknn_ctx = mm_module_open(&rtknn_module);
	if (rtknn_ctx) {
		//mm_module_ctrl(rtknn_ctx, CMD_RTKNN_SET_MODEL_ID, MOBILENETSSD_20OBJ);
#if MODEL_SSDM_USED
		mm_module_ctrl(rtknn_ctx, CMD_RTKNN_SET_PARAMS, (int)&rtknn_ssd_params);
#endif
#if MODEL_FACE_USED
		mm_module_ctrl(rtknn_ctx, CMD_RTKNN_SET_PARAMS, (int)&rtknn_face_params);
#endif
		mm_module_ctrl(rtknn_ctx, CMD_RTKNN_SET_WIDTH, 640);
		mm_module_ctrl(rtknn_ctx, CMD_RTKNN_SET_HEIGHT, 360);
		mm_module_ctrl(rtknn_ctx, CMD_RTKNN_SET_FPS, 5);

	} else {
		rt_printf("RTKNN open fail\n\r");
		goto mmf2_example_array_rtknn_fail;
	}
	rt_printf("RTKNN opened\n\r");


	//--------------Link---------------------------
	siso_array_rtknn = siso_create();
	if (siso_array_rtknn) {
		siso_ctrl(siso_array_rtknn, MMIC_CMD_ADD_INPUT, (uint32_t)array_ctx, 0);
		siso_ctrl(siso_array_rtknn, MMIC_CMD_ADD_OUTPUT, (uint32_t)rtknn_ctx, 0);
		siso_start(siso_array_rtknn);
	} else {
		rt_printf("siso_array_rtknn open fail\n\r");
		goto mmf2_example_array_rtknn_fail;
	}
	rt_printf("siso_array_rtknn started\n\r");

	return;
mmf2_example_array_rtknn_fail:

	return;
}