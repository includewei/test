/******************************************************************************
*
* Copyright(c) 2007 - 2021 Realtek Corporation. All rights reserved.
*
******************************************************************************/
#include "mmf2_link.h"
#include "mmf2_siso.h"
#include "mmf2_mimo.h"

#include "module_video.h"
#include "module_rtsp2.h"
#include "module_audio.h"
#include "module_aac.h"
#include "mmf2_pro2_video_config.h"
#include "video_example_media_framework.h"
#include "log_service.h"

/*****************************************************************************
* ISP channel : 0,1
* Video type  : H264/HEVC
*****************************************************************************/

#define V1_CHANNEL 0
#if USE_SENSOR == SENSOR_GC4653
#define V1_RESOLUTION VIDEO_2K
#define V1_FPS 15
#define V1_GOP 15
#else
#define V1_RESOLUTION VIDEO_FHD
#define V1_FPS 30
#define V1_GOP 30
#endif
#define V1_BPS 2*1024*1024
#define V1_RCMODE 2 // 1: CBR, 2: VBR

#define V2_CHANNEL 1
#define V2_RESOLUTION VIDEO_HD
#if USE_SENSOR == SENSOR_GC4653
#define V2_FPS 15
#define V2_GOP 15
#else
#define V2_FPS 30
#define V2_GOP 30
#endif
#define V2_BPS 1024*1024
#define V2_RCMODE 2 // 1: CBR, 2: VBR

#define USE_H265 0

#if USE_H265
#include "sample_h265.h"
#define VIDEO_TYPE VIDEO_HEVC
#define VIDEO_CODEC AV_CODEC_ID_H265
#else
#include "sample_h264.h"
#define VIDEO_TYPE VIDEO_H264
#define VIDEO_CODEC AV_CODEC_ID_H264
#endif

#if (V1_RESOLUTION == VIDEO_VGA)
#define V1_WIDTH	640
#define V1_HEIGHT	480
#elif (V1_RESOLUTION == VIDEO_HD)
#define V1_WIDTH	1280
#define V1_HEIGHT	720
#elif (V1_RESOLUTION == VIDEO_FHD)
#define V1_WIDTH	1920
#define V1_HEIGHT	1080
#elif V1_RESOLUTION == VIDEO_2K
#define V1_WIDTH	2560
#define V1_HEIGHT	1440
#endif

#if (V2_RESOLUTION == VIDEO_VGA)
#define V2_WIDTH	640
#define V2_HEIGHT	480
#elif (V2_RESOLUTION == VIDEO_HD)
#define V2_WIDTH	1280
#define V2_HEIGHT	720
#elif (V2_RESOLUTION == VIDEO_FHD)
#define V2_WIDTH	1920
#define V2_HEIGHT	1080
#endif

static void atcmd_userctrl_init(void);
static mm_context_t *video_v1_ctx			= NULL;
static mm_context_t *video_v2_ctx			= NULL;
static mm_context_t *audio_ctx				= NULL;
static mm_context_t *aac_ctx				= NULL;
static mm_context_t *rtsp2_v1_ctx			= NULL;
static mm_context_t *rtsp2_v2_ctx			= NULL;

static mm_siso_t *siso_audio_aac			= NULL;
static mm_mimo_t *mimo_2v_1a_rtsp			= NULL;

static video_params_t video_v1_params = {
	.stream_id = V1_CHANNEL,
	.type = VIDEO_TYPE,
	.resolution = V1_RESOLUTION,
	.width = V1_WIDTH,
	.height = V1_HEIGHT,
	.bps = V1_BPS,
	.fps = V1_FPS,
	.gop = V1_GOP,
	.rc_mode = V1_RCMODE,
	.use_static_addr = 1,
};

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

static audio_params_t audio_params = {
	.sample_rate = ASR_8KHZ,
	.word_length = WL_16BIT,
	.mic_gain    = MIC_0DB,
	.dmic_l_gain    = DMIC_BOOST_24DB,
	.dmic_r_gain    = DMIC_BOOST_24DB,
	.use_mic_type   = USE_AUDIO_AMIC,
	.channel     = 1,
	.enable_aec  = 0
};

static aac_params_t aac_params = {
	.sample_rate = 8000,
	.channel = 1,
	.bit_length = FAAC_INPUT_16BIT,
	.output_format = 1,
	.mpeg_version = MPEG4,
	.mem_total_size = 10 * 1024,
	.mem_block_size = 128,
	.mem_frame_size = 1024
};

static rtsp2_params_t rtsp2_v1_params = {
	.type = AVMEDIA_TYPE_VIDEO,
	.u = {
		.v = {
			.codec_id = VIDEO_CODEC,
			.fps      = V1_FPS,
			.bps      = V1_BPS
		}
	}
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

static rtsp2_params_t rtsp2_a_params = {
	.type = AVMEDIA_TYPE_AUDIO,
	.u = {
		.a = {
			.codec_id   = AV_CODEC_ID_MP4A_LATM,
			.channel    = 1,
			.samplerate = 8000
		}
	}
};


void mmf2_video_example_av2_init(void)
{
	atcmd_userctrl_init();
	// ------ Channel 1--------------

	int voe_heap_size = video_voe_presetting(1, V1_WIDTH, V1_HEIGHT, V1_BPS, 0,
						1, V2_WIDTH, V2_HEIGHT, V2_BPS, 0,
						0, 0, 0, 0, 0,
						0, 0, 0);

	printf("\r\n voe heap size = %d\r\n", voe_heap_size);
	if (voe_boot_fsc_status()) {
		video_v1_params.fcs = 1;
	}
	video_v1_ctx = mm_module_open(&video_module);
	if (video_v1_ctx) {
		mm_module_ctrl(video_v1_ctx, CMD_VIDEO_SET_PARAMS, (int)&video_v1_params);
		mm_module_ctrl(video_v1_ctx, MM_CMD_SET_QUEUE_LEN, V1_FPS*3);
		mm_module_ctrl(video_v1_ctx, MM_CMD_INIT_QUEUE_ITEMS, MMQI_FLAG_DYNAMIC);
	} else {
		rt_printf("video open fail\n\r");
		goto mmf2_video_exmaple_av2_fail;
	}



	// ------ Channel 2--------------
	video_v2_ctx = mm_module_open(&video_module);
	if (video_v2_ctx) {
		mm_module_ctrl(video_v2_ctx, CMD_VIDEO_SET_PARAMS, (int)&video_v2_params);
		mm_module_ctrl(video_v2_ctx, MM_CMD_SET_QUEUE_LEN, V2_FPS*3);
		mm_module_ctrl(video_v2_ctx, MM_CMD_INIT_QUEUE_ITEMS, MMQI_FLAG_DYNAMIC);
	} else {
		rt_printf("video open fail\n\r");
		goto mmf2_video_exmaple_av2_fail;
	}


	//--------------Audio --------------
	audio_ctx = mm_module_open(&audio_module);
	if (audio_ctx) {
		mm_module_ctrl(audio_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_params);
		mm_module_ctrl(audio_ctx, MM_CMD_SET_QUEUE_LEN, 6);
		mm_module_ctrl(audio_ctx, MM_CMD_INIT_QUEUE_ITEMS, MMQI_FLAG_STATIC);
		mm_module_ctrl(audio_ctx, CMD_AUDIO_APPLY, 0);
	} else {
		rt_printf("audio open fail\n\r");
		goto mmf2_video_exmaple_av2_fail;
	}

	aac_ctx = mm_module_open(&aac_module);
	if (aac_ctx) {
		mm_module_ctrl(aac_ctx, CMD_AAC_SET_PARAMS, (int)&aac_params);
		mm_module_ctrl(aac_ctx, MM_CMD_SET_QUEUE_LEN, 16);
		mm_module_ctrl(aac_ctx, MM_CMD_INIT_QUEUE_ITEMS, MMQI_FLAG_DYNAMIC);
		mm_module_ctrl(aac_ctx, CMD_AAC_INIT_MEM_POOL, 0);
		mm_module_ctrl(aac_ctx, CMD_AAC_APPLY, 0);
	} else {
		rt_printf("AAC open fail\n\r");
		goto mmf2_video_exmaple_av2_fail;
	}


	//--------------RTSP---------------
	rtsp2_v1_ctx = mm_module_open(&rtsp2_module);
	if (rtsp2_v1_ctx) {
		mm_module_ctrl(rtsp2_v1_ctx, CMD_RTSP2_SELECT_STREAM, 0);
		mm_module_ctrl(rtsp2_v1_ctx, CMD_RTSP2_SET_PARAMS, (int)&rtsp2_v1_params);
		mm_module_ctrl(rtsp2_v1_ctx, CMD_RTSP2_SET_APPLY, 0);

		mm_module_ctrl(rtsp2_v1_ctx, CMD_RTSP2_SELECT_STREAM, 1);
		mm_module_ctrl(rtsp2_v1_ctx, CMD_RTSP2_SET_PARAMS, (int)&rtsp2_a_params);
		mm_module_ctrl(rtsp2_v1_ctx, CMD_RTSP2_SET_APPLY, 0);

		mm_module_ctrl(rtsp2_v1_ctx, CMD_RTSP2_SET_STREAMMING, ON);
	} else {
		rt_printf("RTSP2 open fail\n\r");
		goto mmf2_video_exmaple_av2_fail;
	}


	rtsp2_v2_ctx = mm_module_open(&rtsp2_module);
	if (rtsp2_v2_ctx) {
		mm_module_ctrl(rtsp2_v2_ctx, CMD_RTSP2_SELECT_STREAM, 0);
		mm_module_ctrl(rtsp2_v2_ctx, CMD_RTSP2_SET_PARAMS, (int)&rtsp2_v2_params);
		mm_module_ctrl(rtsp2_v2_ctx, CMD_RTSP2_SET_APPLY, 0);

		mm_module_ctrl(rtsp2_v2_ctx, CMD_RTSP2_SELECT_STREAM, 1);
		mm_module_ctrl(rtsp2_v2_ctx, CMD_RTSP2_SET_PARAMS, (int)&rtsp2_a_params);
		mm_module_ctrl(rtsp2_v2_ctx, CMD_RTSP2_SET_APPLY, 0);

		mm_module_ctrl(rtsp2_v2_ctx, CMD_RTSP2_SET_STREAMMING, ON);
	} else {
		rt_printf("RTSP2 open fail\n\r");
		goto mmf2_video_exmaple_av2_fail;
	}


	//--------------Link---------------------------
	siso_audio_aac = siso_create();
	if (siso_audio_aac) {
		siso_ctrl(siso_audio_aac, MMIC_CMD_ADD_INPUT, (uint32_t)audio_ctx, 0);
		siso_ctrl(siso_audio_aac, MMIC_CMD_ADD_OUTPUT, (uint32_t)aac_ctx, 0);
		siso_start(siso_audio_aac);
	} else {
		rt_printf("siso1 open fail\n\r");
		goto mmf2_video_exmaple_av2_fail;
	}

	rt_printf("siso started\n\r");


	mimo_2v_1a_rtsp = mimo_create();
	if (mimo_2v_1a_rtsp) {
#if defined(configENABLE_TRUSTZONE) && (configENABLE_TRUSTZONE == 1)
		mimo_ctrl(mimo_2v_1a_rtsp, MMIC_CMD_SET_SECURE_CONTEXT, 1, 0);
#endif
		mimo_ctrl(mimo_2v_1a_rtsp, MMIC_CMD_ADD_INPUT0, (uint32_t)video_v1_ctx, 0);
		mimo_ctrl(mimo_2v_1a_rtsp, MMIC_CMD_ADD_INPUT1, (uint32_t)video_v2_ctx, 0);
		mimo_ctrl(mimo_2v_1a_rtsp, MMIC_CMD_ADD_INPUT2, (uint32_t)aac_ctx, 0);
		mimo_ctrl(mimo_2v_1a_rtsp, MMIC_CMD_ADD_OUTPUT0, (uint32_t)rtsp2_v1_ctx, MMIC_DEP_INPUT0 | MMIC_DEP_INPUT2);
		mimo_ctrl(mimo_2v_1a_rtsp, MMIC_CMD_ADD_OUTPUT1, (uint32_t)rtsp2_v2_ctx, MMIC_DEP_INPUT1 | MMIC_DEP_INPUT2);
		mimo_start(mimo_2v_1a_rtsp);
	} else {
		rt_printf("mimo open fail\n\r");
		goto mmf2_video_exmaple_av2_fail;
	}

	mm_module_ctrl(video_v1_ctx, CMD_VIDEO_APPLY, V1_CHANNEL);
	mm_module_ctrl(video_v2_ctx, CMD_VIDEO_APPLY, V2_CHANNEL);

	rt_printf("mimo started\n\r");


	return;
mmf2_video_exmaple_av2_fail:

	return;
}

static char *example = "mmf2_video_example_av2";
static void example_deinit(void)
{
	//Pause Linker
	mimo_pause(mimo_2v_1a_rtsp, MM_OUTPUT0 | MM_OUTPUT1);
	siso_pause(siso_audio_aac);

	//Stop module
	mm_module_ctrl(rtsp2_v1_ctx, CMD_RTSP2_SET_STREAMMING, OFF);
	mm_module_ctrl(rtsp2_v2_ctx, CMD_RTSP2_SET_STREAMMING, OFF);
	mm_module_ctrl(video_v1_ctx, CMD_VIDEO_STREAM_STOP, V1_CHANNEL);
	mm_module_ctrl(video_v2_ctx, CMD_VIDEO_STREAM_STOP, V2_CHANNEL);
	mm_module_ctrl(audio_ctx, CMD_AUDIO_SET_TRX, 0);
	mm_module_ctrl(aac_ctx, CMD_AAC_STOP, 0);

	//Delete linker
	mimo_delete(mimo_2v_1a_rtsp);
	siso_delete(siso_audio_aac);

	//Close module
	mm_module_close(rtsp2_v1_ctx);
	mm_module_close(rtsp2_v2_ctx);
	mm_module_close(video_v1_ctx);
	mm_module_close(video_v2_ctx);
	mm_module_close(audio_ctx);
	mm_module_close(aac_ctx);

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
