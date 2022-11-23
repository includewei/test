/******************************************************************************
*
* Copyright(c) 2007 - 2018 Realtek Corporation. All rights reserved.
*
******************************************************************************/

#include <stdint.h>
#include <stdlib.h>
#include "module_md.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "hal_video.h"//draw rect
#include "avcodec.h"

static float sw_ALS_lite(void)
{
	int iAECGain, iAEExp;
	float fAECGain;
	hal_video_isp_ctrl(0x0011, 0, &iAEExp);
	hal_video_isp_ctrl(0x0013, 0, &iAECGain);
	fAECGain = iAEExp * iAECGain / 25600;
	//printf("Exp:%.6d, AEC Gain: %.4f, fAECGain : %.6f\r\n", iAEExp, (float)iAECGain / 256, fAECGain);
	return fAECGain;
}

static char check_AE_stable(void)
{
	float ETGain1, ETGain2;
	ETGain1 = sw_ALS_lite();
	vTaskDelay(100);
	ETGain2 = sw_ALS_lite();
	//printf("ETGain1 = %.5f, ETGain2 = %.5f", ETGain1, ETGain2);
	if (ETGain1 == ETGain2) {
		return 1;
	} else {
		return 0;
	}
}

int md_handle(void *p, void *input, void *output)
{
	md_ctx_t *ctx = (md_ctx_t *)p;
	mm_queue_item_t *input_item = (mm_queue_item_t *)input;
	mm_queue_item_t *output_item = (mm_queue_item_t *)output;
	int i, j, k, l;

	//printf("width = %d,height = %d\n",ctx->params->width, ctx->params->height);
	//printf("image size = %d\n",input_item->size);

#if MD_AFTER_AE_STABLE
	if (ctx->motion_detect_ctx->AE_stable == 0) {
		printf("AE not sable\n\r");
		ctx->motion_detect_ctx->AE_stable = check_AE_stable();
		return 0;
	}
#endif

	if (ctx->motion_detect_ctx->count % MOTION_DETECT_INTERVAL == 0) {
		unsigned long tick1 = xTaskGetTickCount();
		md_get_YRGB_value(ctx->motion_detect_ctx, ctx->params->width, ctx->params->height, (unsigned char *)input_item->data_addr);
		//printf("\r\nCalculate YRGB after %dms.\n", (xTaskGetTickCount() - tick1));
	}

	if (ctx->motion_detect_ctx->count == 0) {
		printf("initial_bgmodel\n\r");
		initial_bgmodel(ctx->motion_detect_ctx);
	}
	if (ctx->motion_detect_ctx->count % MOTION_DETECT_INTERVAL == 0) {
		motion_detect(ctx->motion_detect_ctx);
		if (ctx->motion_detect_ctx->count == MOTION_DETECT_INTERVAL * 1000) {
			ctx->motion_detect_ctx->count = MOTION_DETECT_INTERVAL;
		}
		if (ctx->disp_postproc) {
			ctx->disp_postproc(ctx->motion_detect_ctx->md_result);
		}
	}
	ctx->motion_detect_ctx->count ++;

	if (ctx->md_out_en) {

		int motion = ctx->motion_detect_ctx->left_motion + ctx->motion_detect_ctx->right_motion + ctx->motion_detect_ctx->middle_motion;
		if (motion > ctx->motion_detect_ctx->md_trigger_block_threshold) {
			//printf("Motion Detected!\r\n");
			output_item->timestamp = input_item->timestamp;
			output_item->size = input_item->size;
			output_item->type = AV_CODEC_ID_MD_RAW;
			memcpy((unsigned char *)output_item->data_addr, (unsigned char *) input_item->data_addr, input_item->size);
			return output_item->size;
		}
	}

	return 0;
}

int md_control(void *p, int cmd, int arg)
{
	int ret = 0;
	md_ctx_t *ctx = (md_ctx_t *)p;

	switch (cmd) {
	case CMD_MD_SET_PARAMS:
		ctx->params = (md_param_t *)arg;
		break;
	case CMD_MD_SET_MD_THRESHOLD:
		memcpy(ctx->motion_detect_ctx->md_threshold, (motion_detect_threshold_t *)arg, sizeof(motion_detect_threshold_t));
		printf("Set MD Threshold: Tbase = %lf, Tlum = %lf\r\n", ctx->motion_detect_ctx->md_threshold->Tbase, ctx->motion_detect_ctx->md_threshold->Tlum);

		if (ctx->motion_detect_ctx->md_threshold->Tbase > 1) {
			ctx->motion_detect_ctx->Tauto = ctx->motion_detect_ctx->md_threshold->Tbase + 1;
		} else {
			ctx->motion_detect_ctx->Tauto = 1;
		}

		break;
	case CMD_MD_GET_MD_THRESHOLD:
		memcpy((motion_detect_threshold_t *)arg, ctx->motion_detect_ctx->md_threshold, sizeof(motion_detect_threshold_t));
		break;
	case CMD_MD_SET_MD_MASK:
		memcpy(ctx->motion_detect_ctx->md_mask, (char *)arg, sizeof(ctx->motion_detect_ctx->md_mask));
		printf("Set MD Mask: \r\n");
		for (int j = 0; j < md_row; j++) {
			for (int k = 0; k < md_col; k++) {
				printf("%d ", ctx->motion_detect_ctx->md_mask[j * md_col + k]);
			}
			printf("\r\n");
		}
		printf("\r\n");
		printf("\r\n");
		break;
	case CMD_MD_GET_MD_MASK:
		memcpy((int *)arg, ctx->motion_detect_ctx->md_mask, sizeof(ctx->motion_detect_ctx->md_mask));
		break;
	case CMD_MD_GET_MD_RESULT:
		memcpy((int *)arg, ctx->motion_detect_ctx->md_result, sizeof(ctx->motion_detect_ctx->md_result));
		break;
	case CMD_MD_SET_OUTPUT:
		ctx->md_out_en = (bool)arg;
		((mm_context_t *)ctx->parent)->module->output_type = MM_TYPE_VSINK;
		break;
	case CMD_MD_SET_DISPPOST:
		ctx->disp_postproc = (md_disp_postprcess)arg;
		break;
	case CMD_MD_SET_TRIG_BLK:
		ctx->motion_detect_ctx->md_trigger_block_threshold = arg;
		break;
	}

	return ret;
}

void *md_destroy(void *p)
{
	md_ctx_t *ctx = (md_ctx_t *)p;

	if (ctx) {
		if (ctx->motion_detect_ctx) {
			if (ctx->motion_detect_ctx->md_threshold) {
				free(ctx->motion_detect_ctx->md_threshold);
			}
			free(ctx->motion_detect_ctx);
		}
		free(ctx);
	}
	return NULL;
}

void *md_create(void *parent)
{
	md_ctx_t *ctx = (md_ctx_t *)malloc(sizeof(md_ctx_t));
	memset(ctx, 0, sizeof(md_ctx_t));
	//motion_detection_init();

	ctx->motion_detect_ctx = NULL;
	ctx->motion_detect_ctx = (md_context_t *) malloc(sizeof(md_context_t));
	if (ctx->motion_detect_ctx == NULL) {
		printf("[Error] Allocate motion_detect_ctx fail\n\r");
		goto md_error;
	}
	memset(ctx->motion_detect_ctx, 0, sizeof(md_context_t));
	ctx->motion_detect_ctx->max_threshold_shift = 0.7;
	ctx->motion_detect_ctx->max_turn_off = 15;
	ctx->motion_detect_ctx->md_trigger_block_threshold = 1;

	ctx->motion_detect_ctx->md_threshold = (motion_detect_threshold_t *) malloc(sizeof(motion_detect_threshold_t));
	ctx->motion_detect_ctx->md_threshold->Tbase = 2;
	ctx->motion_detect_ctx->md_threshold->Tlum = 3;
	ctx->motion_detect_ctx->Tauto = 1;
	ctx->disp_postproc = NULL;

	for (int i = 0; i < md_col * md_row; i++) {
		ctx->motion_detect_ctx->md_mask[i] = 1;
	}
	if (DYNAMIC_THRESHOLD) {
		ctx->motion_detect_ctx->md_threshold->Tbase = 2;
	}

	ctx->parent = parent;

	return ctx;

md_error:
	return NULL;
}

void *md_new_item(void *p)
{
	md_ctx_t *ctx = (md_ctx_t *)p;

	return (void *)malloc(ctx->params->width * ctx->params->height * 3);
}

void *md_del_item(void *p, void *d)
{
	(void)p;
	if (d) {
		free(d);
	}
	return NULL;
}

mm_module_t md_module = {
	.create = md_create,
	.destroy = md_destroy,
	.control = md_control,
	.handle = md_handle,

	.new_item = md_new_item,
	.del_item = md_del_item,

	.output_type = MM_TYPE_NONE,
	.module_type = MM_TYPE_VDSP,
	.name = "md"
};
