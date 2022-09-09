/******************************************************************************
*
* Copyright(c) 2007 - 2021 Realtek Corporation. All rights reserved.
*
******************************************************************************/
#include "mmf2_link.h"
#include "mmf2_siso.h"
#include "module_video.h"
#include "module_vipnn.h"
#include "module_rtsp2.h"
#include "mmf2_pro2_video_config.h"
#include "video_example_media_framework.h"
#include "log_service.h"
#include "avcodec.h"

#include "input_image_640x360x3.h"
#include "model_mbnetssd.h"
#include "model_yolov3t.h"

#include "hal_video.h"
#include "hal_isp.h"

/*****************************************************************************
* ISP channel : 4
* Video type  : RGB
*****************************************************************************/
#define RTSP_CHANNEL 0
#define RTSP_RESOLUTION VIDEO_FHD
#define RTSP_FPS 30
#define RTSP_GOP 30
#define RTSP_BPS 1*1024*1024
#define VIDEO_RCMODE 2 // 1: CBR, 2: VBR

#define USE_H265 0

#if USE_H265
#include "sample_h265.h"
#define RTSP_TYPE VIDEO_HEVC
#define RTSP_CODEC AV_CODEC_ID_H265
#else
#include "sample_h264.h"
#define RTSP_TYPE VIDEO_H264
#define RTSP_CODEC AV_CODEC_ID_H264
#endif

#if RTSP_RESOLUTION == VIDEO_VGA
#define RTSP_WIDTH	640
#define RTSP_HEIGHT	480
#elif RTSP_RESOLUTION == VIDEO_HD
#define RTSP_WIDTH	1280
#define RTSP_HEIGHT	720
#elif RTSP_RESOLUTION == VIDEO_FHD
#define RTSP_WIDTH	1920
#define RTSP_HEIGHT	1080
#endif

static video_params_t video_v1_params = {
	.stream_id 		= RTSP_CHANNEL,
	.type 			= RTSP_TYPE,
	.resolution 	= RTSP_RESOLUTION,
	.width 			= RTSP_WIDTH,
	.height 		= RTSP_HEIGHT,
	.bps            = RTSP_BPS,
	.fps 			= RTSP_FPS,
	.gop 			= RTSP_GOP,
	.rc_mode        = VIDEO_RCMODE,
	.use_static_addr = 1,
	//.fcs = 1
};

static rtsp2_params_t rtsp2_v1_params = {
	.type = AVMEDIA_TYPE_VIDEO,
	.u = {
		.v = {
			.codec_id = RTSP_CODEC,
			.fps      = RTSP_FPS,
			.bps      = RTSP_BPS
		}
	}
};

// NN model selction //
#define MOBILENET_SSD_MODEL     1
#define YOLO_MODEL              2
#define USE_NN_MODEL            YOLO_MODEL

#define NN_CHANNEL 4
#define NN_RESOLUTION VIDEO_VGA //don't care for NN
#define NN_FPS 10
#define NN_GOP NN_FPS
#define NN_BPS 1024*1024 //don't care for NN
#define NN_TYPE VIDEO_RGB

#if (USE_NN_MODEL==MOBILENET_SSD_MODEL)
#define NN_MODEL_OBJ   mbnetssd_fwfs
#define NN_WIDTH	300
#define NN_HEIGHT	300
static float nn_confidence_thresh = 0.75;
static float nn_nms_thresh = 0.3;
#elif (USE_NN_MODEL==YOLO_MODEL)
#define NN_MODEL_OBJ   yolov3_tiny_fwfs
#define NN_WIDTH	416
#define NN_HEIGHT	416
static float nn_confidence_thresh = 0.4;
static float nn_nms_thresh = 0.3;
#else
#error Please set model correctly. (YOLO_MODEL, MOBILENET_SSD_MODEL)
#endif

static video_params_t video_v4_params = {
	.stream_id 		= NN_CHANNEL,
	.type 			= NN_TYPE,
	.resolution	 	= NN_RESOLUTION,
	.width 			= NN_WIDTH,
	.height 		= NN_HEIGHT,
	.bps 			= NN_BPS,
	.fps 			= NN_FPS,
	.gop 			= NN_GOP,
	.direct_output 	= 0,
	.use_static_addr = 1,
	.use_roi = 1,
	.roi = {
		.xmin = 0,
		.ymin = 0,
		.xmax = 1920, //ORIGIN WIDTH
		.ymax = 1080, //ORIGIN WIDTH
	}
};

#include "module_array.h"
static array_params_t h264_array_params = {
	.type = AVMEDIA_TYPE_VIDEO,
	.codec_id = AV_CODEC_ID_RGB888,
	.mode = ARRAY_MODE_LOOP,
	.u = {
		.v = {
			.fps    = 5,
		}
	}
};

static nn_data_param_t roi_nn = {
	.img = {
		.width = NN_WIDTH,
		.height = NN_HEIGHT,
		.rgb = 0, // set to 1 if want RGB->BGR or BGR->RGB
		.roi = {
			.xmin = 0,
			.ymin = 0,
			.xmax = NN_WIDTH,
			.ymax = NN_HEIGHT,
		}
	}
};

#define V1_ENA 1
#define V4_ENA 1
#define V4_SIM 0

static void atcmd_userctrl_init(void);
static mm_context_t *array_ctx            = NULL;
static mm_context_t *video_v1_ctx			= NULL;
static mm_context_t *rtsp2_v1_ctx			= NULL;
static mm_siso_t *siso_video_rtsp_v1			= NULL;

static mm_context_t *video_rgb_ctx			= NULL;
static mm_context_t *vipnn_ctx            = NULL;
static mm_siso_t *siso_array_vipnn         = NULL;


//--------------------------------------------
// Draw Rect
//--------------------------------------------
#include "../../../../component/video/osd2/isp_osd_example.h"

static struct result_frame g_results;
static obj_ctrl_s sw_object;
static int nn_class[10] = {0};
static int nn_score[10] = {0};
static int desired_class_num = 4;
static int desired_class_list[] = {1, 57, 63, 68};
static char *tag[4] = { "person", "chair", "tv", "cell phone"};

static int check_in_list(int class_indx)
{
	for (int i = 0; i < desired_class_num; i++) {
		if (class_indx == desired_class_list[i]) {
			return i;
		}
	}
	return -1;
}

static void nn_set_object(void *p, void *img_param)
{
	int i = 0;
	objdetect_res_t *res = (objdetect_res_t *)p;
	nn_data_param_t *im = (nn_data_param_t *)img_param;

	if (!p || !img_param)	{
		return;
	}

	int im_h = RTSP_HEIGHT;
	int im_w = RTSP_WIDTH;

	float ratio_h = (float)im_h / (float)im->img.height;
	float ratio_w = (float)im_w / (float)im->img.width;
	int roi_h = (int)((im->img.roi.ymax - im->img.roi.ymin) * ratio_h);
	int roi_w = (int)((im->img.roi.xmax - im->img.roi.xmin) * ratio_w);
	int roi_x = (int)(im->img.roi.xmin * ratio_w);
	int roi_y = (int)(im->img.roi.ymin * ratio_h);

	// float ratio_h = (float)im_h / (float)im->img.height;
	// int roi_h = (int)((im->img.roi.ymax - im->img.roi.ymin) * ratio_h);
	// int roi_w = (int)((im->img.roi.xmax - im->img.roi.xmin) * ratio_h);
	// int roi_x = (int)(im->img.roi.xmin * ratio_h + (im_w - roi_w) / 2);
	// int roi_y = (int)(im->img.roi.ymin * ratio_h);

	printf("object num = %d\r\n", res->obj_num);
	if (res->obj_num > 0) {
		sw_object.objDetectNumber = 0;
		for (i = 0; i < res->obj_num; i++) {
			int obj_class = (int)res->result[6 * i ];
			if (sw_object.objDetectNumber == 10) {
				break;
			}
			//printf("obj_class = %d\r\n",obj_class);

			int class_id = check_in_list(obj_class + 1); //show class in desired_class_list
			//int class_id = obj_class; //coco label
			if (class_id != -1) {
				int ind = sw_object.objDetectNumber;
				sw_object.objTopY[ind] = (int)(res->result[6 * i + 3] * roi_h) + roi_y;
				if (sw_object.objTopY[ind] < 0) {
					sw_object.objTopY[ind] = 0;
				} else if (sw_object.objTopY[ind] >= im_h) {
					sw_object.objTopY[ind] = im_h - 1;
				}

				sw_object.objTopX[ind] = (int)(res->result[6 * i + 2] * roi_w) + roi_x;
				if (sw_object.objTopX[ind] < 0) {
					sw_object.objTopX[ind] = 0;
				} else if (sw_object.objTopX[ind] >= im_w) {
					sw_object.objTopX[ind] = im_w - 1;
				}

				sw_object.objBottomY[ind] = (int)(res->result[6 * i + 5] * roi_h) + roi_y;
				if (sw_object.objBottomY[ind] < 0) {
					sw_object.objBottomY[ind] = 0;
				} else if (sw_object.objBottomY[ind] >= im_h) {
					sw_object.objBottomY[ind] = im_h - 1;
				}

				sw_object.objBottomX[ind] = (int)(res->result[6 * i + 4] * roi_w) + roi_x;
				if (sw_object.objBottomX[ind] < 0) {
					sw_object.objBottomX[ind] = 0;
				} else if (sw_object.objBottomX[ind] >= im_w) {
					sw_object.objBottomX[ind] = im_w - 1;
				}
				nn_class[ind] = class_id;
				nn_score[ind] = (int)(res->result[6 * i + 1 ] * 100);
				sw_object.objDetectNumber++;
				printf("%d,c%d:%d %d %d %d\n\r", i, (int)res->result[6 * i ], (int)sw_object.objTopX[ind], (int)sw_object.objTopY[ind], (int)sw_object.objBottomX[ind],
					   (int)sw_object.objBottomY[ind]);
			}
		}
	} else {
		sw_object.objDetectNumber = 0;
	}

	int osd_ready2draw = osd_get_status();
	if (osd_ready2draw == 1) {
		//printf("sw_object.objDetectNumber = %d\r\n", sw_object.objDetectNumber);
		g_results.num = sw_object.objDetectNumber;
		if (sw_object.objDetectNumber > RECT_NUM) {
			g_results.num = RECT_NUM;
		}

		for (i = 0; i < RECT_NUM; i++) {
			if (i < g_results.num) {
				g_results.obj[i].idx = i;
				g_results.obj[i].class = nn_class[i];
				g_results.obj[i].score = nn_score[i];
				g_results.obj[i].left = sw_object.objTopX[i];
				g_results.obj[i].top = sw_object.objTopY[i];
				g_results.obj[i].right = sw_object.objBottomX[i];
				g_results.obj[i].bottom = sw_object.objBottomY[i];

			} else {
				g_results.obj[i].idx = i;
				g_results.obj[i].class = 0;
				g_results.obj[i].score = 0;
				g_results.obj[i].left = 0;
				g_results.obj[i].top = 0;
				g_results.obj[i].right = 0;
				g_results.obj[i].bottom = 0;
			}
			//printf("num=%d  %d, %d, %d, %d.\r\n", g_results.num, g_results.obj[i].left, g_results.obj[i].right, g_results.obj[i].top, g_results.obj[i].bottom);
		}
		osd_update_rect_result(g_results);
	}

}

void mmf2_video_example_vipnn_rtsp_init(void)
{
	atcmd_userctrl_init();

	int voe_heap_size = video_voe_presetting(V1_ENA, RTSP_WIDTH, RTSP_HEIGHT, RTSP_BPS, 0,
						0, 0, 0, 0, 0,
						0, 0, 0, 0, 0,
						V4_ENA, NN_WIDTH, NN_HEIGHT);

	printf("\r\n voe heap size = %d\r\n", voe_heap_size);

#if V1_ENA

	video_v1_ctx = mm_module_open(&video_module);
	if (video_v1_ctx) {
		mm_module_ctrl(video_v1_ctx, CMD_VIDEO_SET_PARAMS, (int)&video_v1_params);
		mm_module_ctrl(video_v1_ctx, MM_CMD_SET_QUEUE_LEN, RTSP_FPS*3);
		mm_module_ctrl(video_v1_ctx, MM_CMD_INIT_QUEUE_ITEMS, MMQI_FLAG_DYNAMIC);
	} else {
		printf("video open fail\n\r");
		goto mmf2_example_vnn_rtsp_fail;
	}

	// encode_rc_parm_t rc_parm;
	// rc_parm.minQp = 28;
	// rc_parm.maxQp = 45;
	// mm_module_ctrl(video_v1_ctx, CMD_VIDEO_SET_RCPARAM, (int)&rc_parm);

	rtsp2_v1_ctx = mm_module_open(&rtsp2_module);
	if (rtsp2_v1_ctx) {
		mm_module_ctrl(rtsp2_v1_ctx, CMD_RTSP2_SELECT_STREAM, 0);
		mm_module_ctrl(rtsp2_v1_ctx, CMD_RTSP2_SET_PARAMS, (int)&rtsp2_v1_params);
		mm_module_ctrl(rtsp2_v1_ctx, CMD_RTSP2_SET_APPLY, 0);
		mm_module_ctrl(rtsp2_v1_ctx, CMD_RTSP2_SET_STREAMMING, ON);
	} else {
		printf("RTSP2 open fail\n\r");
		goto mmf2_example_vnn_rtsp_fail;
	}
#endif
#if V4_ENA
#if V4_SIM==0
	video_rgb_ctx = mm_module_open(&video_module);
	if (video_rgb_ctx) {
		mm_module_ctrl(video_rgb_ctx, CMD_VIDEO_SET_PARAMS, (int)&video_v4_params);
		mm_module_ctrl(video_rgb_ctx, MM_CMD_SET_QUEUE_LEN, 2);
		mm_module_ctrl(video_rgb_ctx, MM_CMD_INIT_QUEUE_ITEMS, MMQI_FLAG_DYNAMIC);
	} else {
		printf("video open fail\n\r");
		goto mmf2_example_vnn_rtsp_fail;
	}
#else
	array_t array;
	array.data_addr = (uint32_t) testRGB_640x360;
	array.data_len = (uint32_t) 640 * 360 * 3;
	array_ctx = mm_module_open(&array_module);
	if (array_ctx) {
		mm_module_ctrl(array_ctx, CMD_ARRAY_SET_PARAMS, (int)&h264_array_params);
		mm_module_ctrl(array_ctx, CMD_ARRAY_SET_ARRAY, (int)&array);
		mm_module_ctrl(array_ctx, MM_CMD_SET_QUEUE_LEN, 6);
		mm_module_ctrl(array_ctx, MM_CMD_INIT_QUEUE_ITEMS, MMQI_FLAG_DYNAMIC);
		mm_module_ctrl(array_ctx, CMD_ARRAY_APPLY, 0);
		mm_module_ctrl(array_ctx, CMD_ARRAY_STREAMING, 1);	// streamming on
	} else {
		printf("ARRAY open fail\n\r");
		goto mmf2_example_vnn_rtsp_fail;
	}
#endif
	// VIPNN
	vipnn_ctx = mm_module_open(&vipnn_module);
	if (vipnn_ctx) {
		mm_module_ctrl(vipnn_ctx, CMD_VIPNN_SET_MODEL, (int)&NN_MODEL_OBJ);
		mm_module_ctrl(vipnn_ctx, CMD_VIPNN_SET_IN_PARAMS, (int)&roi_nn);
		mm_module_ctrl(vipnn_ctx, CMD_VIPNN_SET_DISPPOST, (int)nn_set_object);
		mm_module_ctrl(vipnn_ctx, CMD_VIPNN_SET_CONFIDENCE_THRES, (int)&nn_confidence_thresh);
		mm_module_ctrl(vipnn_ctx, CMD_VIPNN_SET_NMS_THRES, (int)&nn_nms_thresh);
		mm_module_ctrl(vipnn_ctx, CMD_VIPNN_APPLY, 0);
	} else {
		printf("VIPNN open fail\n\r");
		goto mmf2_example_vnn_rtsp_fail;
	}
	printf("VIPNN opened\n\r");
#endif

	//--------------Link---------------------------
#if V1_ENA
	siso_video_rtsp_v1 = siso_create();
	if (siso_video_rtsp_v1) {
#if defined(configENABLE_TRUSTZONE) && (configENABLE_TRUSTZONE == 1)
		siso_ctrl(siso_video_rtsp_v1, MMIC_CMD_SET_SECURE_CONTEXT, 1, 0);
#endif
		siso_ctrl(siso_video_rtsp_v1, MMIC_CMD_ADD_INPUT, (uint32_t)video_v1_ctx, 0);
		siso_ctrl(siso_video_rtsp_v1, MMIC_CMD_ADD_OUTPUT, (uint32_t)rtsp2_v1_ctx, 0);
		siso_start(siso_video_rtsp_v1);
	} else {
		printf("siso2 open fail\n\r");
		goto mmf2_example_vnn_rtsp_fail;
	}

	mm_module_ctrl(video_v1_ctx, CMD_VIDEO_APPLY, RTSP_CHANNEL);

#endif
#if 1 //V4_ENA
	siso_array_vipnn = siso_create();
	if (siso_array_vipnn) {
#if V4_SIM==0
#if defined(configENABLE_TRUSTZONE) && (configENABLE_TRUSTZONE == 1)
		siso_ctrl(siso_array_vipnn, MMIC_CMD_SET_SECURE_CONTEXT, 1, 0);
#endif
		siso_ctrl(siso_array_vipnn, MMIC_CMD_ADD_INPUT, (uint32_t)video_rgb_ctx, 0);
		siso_ctrl(siso_array_vipnn, MMIC_CMD_SET_STACKSIZE, (uint32_t)1024 * 64, 0);
		siso_ctrl(siso_array_vipnn, MMIC_CMD_SET_TASKPRIORITY, 3, 0);
#else
		siso_ctrl(siso_array_vipnn, MMIC_CMD_ADD_INPUT, (uint32_t)array_ctx, 0);
#endif
		siso_ctrl(siso_array_vipnn, MMIC_CMD_ADD_OUTPUT, (uint32_t)vipnn_ctx, 0);
		siso_start(siso_array_vipnn);
	} else {
		printf("siso_array_vipnn open fail\n\r");
		goto mmf2_example_vnn_rtsp_fail;
	}
#if V4_ENA
#if V4_SIM==0
	mm_module_ctrl(video_rgb_ctx, CMD_VIDEO_APPLY, NN_CHANNEL);
	mm_module_ctrl(video_rgb_ctx, CMD_VIDEO_YUV, 2);
#endif
#endif

	printf("siso_array_vipnn started\n\r");
#endif

	osd_set_tag(desired_class_num, tag);
	example_isp_osd(1, RTSP_CHANNEL, 16, 32);

	return;
mmf2_example_vnn_rtsp_fail:

	return;
}

static char *example = "mmf2_video_example_vipnn_rtsp";
static void example_deinit(void)
{
	//Pause Linker
	siso_pause(siso_video_rtsp_v1);
	siso_pause(siso_array_vipnn);

	//Stop module
	mm_module_ctrl(rtsp2_v1_ctx, CMD_RTSP2_SET_STREAMMING, OFF);
	mm_module_ctrl(video_v1_ctx, CMD_VIDEO_STREAM_STOP, RTSP_CHANNEL);
	mm_module_ctrl(video_rgb_ctx, CMD_VIDEO_STREAM_STOP, NN_CHANNEL);

	//Delete linker
	siso_delete(siso_video_rtsp_v1);
	siso_delete(siso_array_vipnn);

	//Close module
	mm_module_close(rtsp2_v1_ctx);
	mm_module_close(video_v1_ctx);
	mm_module_close(video_rgb_ctx);

	//Video Deinit
	video_deinit();
}

static void fUC(void *arg)
{
	static uint32_t user_cmd = 0;

	if (!strcmp(arg, "TD")) {
		if (user_cmd & USR_CMD_EXAMPLE_DEINIT) {
			printf("invalid state, can not do %s deinit!\r\n", example);
		} else {
			example_deinit();
			user_cmd = USR_CMD_EXAMPLE_DEINIT;
			printf("deinit %s\r\n", example);
		}
	} else if (!strcmp(arg, "TSR")) {
		if (user_cmd & USR_CMD_EXAMPLE_DEINIT) {
			printf("reinit %s\r\n", example);
			sys_reset();
		} else {
			printf("invalid state, can not do %s reinit!\r\n", example);
		}
	} else {
		printf("invalid cmd");
	}

	printf("user command 0x%lx\r\n", user_cmd);
}

static log_item_t userctrl_items[] = {
	{"UC", fUC, },
};

static void atcmd_userctrl_init(void)
{
	log_service_add_table(userctrl_items, sizeof(userctrl_items) / sizeof(userctrl_items[0]));
}
