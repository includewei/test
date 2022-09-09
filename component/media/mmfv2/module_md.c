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

//set shift value
double set_shift(void *p, double YValue)
{
	md_ctx_t *ctx = (md_ctx_t *)p;
	double shift = 0;
	if (YValue > BRIGHT_THRESHOLD) {
		shift = (YValue - BRIGHT_THRESHOLD) * (YValue - BRIGHT_THRESHOLD) * 0.001;
	} else if (YValue < DARK_THRESHOLD) {
		shift = (DARK_THRESHOLD - YValue) * (DARK_THRESHOLD - YValue) * 0.001;
	}
	if (shift > ctx->motion_detect_ctx->max_threshold_shift) {
		shift = ctx->motion_detect_ctx->max_threshold_shift;
	}
	return shift;
}

static float sw_ALS_lite(void)
{
	int iAECGain, iAEExp;
	float fAECGain;
	hal_video_isp_ctrl(0x0011, 0, &iAEExp);
	hal_video_isp_ctrl(0x0013, 0, &iAECGain);
	fAECGain = iAEExp * iAECGain / 25600;
	printf("Exp:%.6d, AEC Gain: %.4f, fAECGain : %.6f\r\n", iAEExp, (float)iAECGain / 256, fAECGain);
	return fAECGain;
}

static char check_AE_stable(void)
{
	float ETGain1, ETGain2;
	ETGain1 = sw_ALS_lite();
	vTaskDelay(100);
	ETGain2 = sw_ALS_lite();
	printf("ETGain1 = %.5f, ETGain2 = %.5f", ETGain1, ETGain2);
	if (ETGain1 == ETGain2) {
		return 1;
	} else {
		return 0;
	}
}

void initial_bgmodel(void *p)
{
	md_ctx_t *ctx = (md_ctx_t *)p;
	//initial bgmodel value
	for (int j = 0; j < row; j++) {
		for (int k = 0; k < col; k++) {
			ctx->motion_detect_ctx->md_bgmodel.RValue[j * col + k] = (double) ctx->motion_detect_ctx->YRGB_data.RValueNow[j * col + k];
			ctx->motion_detect_ctx->md_bgmodel.GValue[j * col + k] = (double) ctx->motion_detect_ctx->YRGB_data.GValueNow[j * col + k];
			ctx->motion_detect_ctx->md_bgmodel.BValue[j * col + k] = (double) ctx->motion_detect_ctx->YRGB_data.BValueNow[j * col + k];
			ctx->motion_detect_ctx->md_bgmodel.YValue[j * col + k] = (double) ctx->motion_detect_ctx->YRGB_data.YValueNow[j * col + k];
		}
	}
}

static obj_ctrl_s md_object;
void motion_detect(void *p)
{
	md_ctx_t *ctx = (md_ctx_t *)p;
	md_context_t *md = ctx->motion_detect_ctx;
	//printf("motion_detect\n\r");

	double md_thr = 6;
	int j, k;
	double Ydiff[col * row] = {0}, Rdiff[col * row] = {0}, Gdiff[col * row] = {0}, Bdiff[col * row] = {0};
	double Yavg_diff = 0, Ravg_diff = 0, Gavg_diff = 0, Bavg_diff = 0;
	double Yavg_diff_ = 0, Ravg_diff_ = 0, Gavg_diff_ = 0, Bavg_diff_ = 0;
	int left_motion = 0, right_motion = 0, middle_motion = 0;

	for (j = 0; j < row; j++) {
		for (k = 0; k < col; k++) {
			Ydiff[j * col + k] = (double)md->YRGB_data.YValueNow[j * col + k] - md->md_bgmodel.YValue[j * col + k];
			Rdiff[j * col + k] = (double)md->YRGB_data.RValueNow[j * col + k] - md->md_bgmodel.RValue[j * col + k];
			Gdiff[j * col + k] = (double)md->YRGB_data.GValueNow[j * col + k] - md->md_bgmodel.GValue[j * col + k];
			Bdiff[j * col + k] = (double)md->YRGB_data.BValueNow[j * col + k] - md->md_bgmodel.BValue[j * col + k];

			if (Ydiff[j * col + k] < 0) {
				Ydiff[j * col + k] = -Ydiff[j * col + k];
			}
			if (Rdiff[j * col + k] < 0) {
				Rdiff[j * col + k] = -Rdiff[j * col + k];
			}
			if (Gdiff[j * col + k] < 0) {
				Gdiff[j * col + k] = -Gdiff[j * col + k];
			}
			if (Bdiff[j * col + k] < 0) {
				Bdiff[j * col + k] = -Bdiff[j * col + k];
			}

			//avg-diff calculate
			Yavg_diff_ += Ydiff[j * col + k];
			Ravg_diff_ += Rdiff[j * col + k];
			Gavg_diff_ += Gdiff[j * col + k];
			Bavg_diff_ += Bdiff[j * col + k];

		}
	}

	Yavg_diff = Yavg_diff_ / col / row;
	Ravg_diff = Ravg_diff_ / col / row;
	Gavg_diff = Gavg_diff_ / col / row;
	Bavg_diff = Bavg_diff_ / col / row;

	for (j = 0; j < row; j++) {
		for (k = 0; k < col; k++) {

			int score = 0;
			double shift = 0;

			// if enable dynamic threshold set shift value
			if (DYNAMIC_THRESHOLD) {
				shift = set_shift(ctx, (double)md->YRGB_data.YValueNow[j * col + k]);
			}

			double threshold1 = Yavg_diff * md->md_threshold->Tlum + md->md_threshold->Tbase - shift;
			double threshold2 = Yavg_diff * md->md_threshold->Tlum + md->Tauto;
			if (Ydiff[j * col + k] > threshold1) {
				score += 3;
				if (Ydiff[j * col + k] > threshold2) {
					score += 3;
				}
			}

			threshold1 = Ravg_diff * md->md_threshold->Tlum + md->md_threshold->Tbase - shift;
			threshold2 = Ravg_diff * md->md_threshold->Tlum + md->Tauto;

			if (Rdiff[j * col + k] > threshold1) {
				score += 1;
				if (Rdiff[j * col + k] > threshold2) {
					score += 1;
				}
			}

			threshold1 = Gavg_diff * md->md_threshold->Tlum + md->md_threshold->Tbase - shift;
			threshold2 = Gavg_diff * md->md_threshold->Tlum + md->Tauto;

			if (Gdiff[j * col + k] > threshold1) {
				score += 1;
				if (Gdiff[j * col + k] > (threshold2)) {
					score += 1;
				}
			}

			threshold1 = Bavg_diff * md->md_threshold->Tlum + md->md_threshold->Tbase - shift;
			threshold2 = Bavg_diff * md->md_threshold->Tlum + md->Tauto;

			if (Bdiff[j * col + k] > threshold1) {
				score += 1;
				if (Bdiff[j * col + k] > threshold2) {
					score += 1;
				}
			}

			// if too dark, skip this region
			if ((md->YRGB_data.YValueNow[j * col + k] < TURN_OFF_THRESHOLD) &&
				(md->YRGB_data.YValueNow[j * col + k] < md->max_turn_off)) {
				score = 0;
			}

			if (score > md_thr) {
				md->md_result[j * col + k] = 1 & md->md_mask[j * col + k];

				if (k < (int)(col / 3)) {
					left_motion += 2;
				} else if (k < (int)(col * 2 / 3)) {
					middle_motion += 2;
				} else {
					right_motion += 2;
				}
			} else {
				md->md_result[j * col + k] = 0 & md->md_mask[j * col + k];
			}

			//bg-model calculate
			md->md_bgmodel.YValue[j * col + k] = ((double)md->YRGB_data.YValueNow[j * col + k] + md->md_bgmodel.YValue[j * col
												  + k]) / 2;
			md->md_bgmodel.RValue[j * col + k] = ((double)md->YRGB_data.RValueNow[j * col + k] + md->md_bgmodel.RValue[j * col
												  + k]) / 2;
			md->md_bgmodel.GValue[j * col + k] = ((double)md->YRGB_data.GValueNow[j * col + k] + md->md_bgmodel.GValue[j * col
												  + k]) / 2;
			md->md_bgmodel.BValue[j * col + k] = ((double)md->YRGB_data.BValueNow[j * col + k] + md->md_bgmodel.BValue[j * col
												  + k]) / 2;

		}
	}
	md->left_motion = left_motion;
	md->right_motion = right_motion;
	md->middle_motion = middle_motion;

}

int md_handle(void *p, void *input, void *output)
{
	unsigned long tick1 = xTaskGetTickCount();
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
		unsigned char *buffer = (unsigned char *) input_item->data_addr;

		int Y[col * row] = {0};
		int R[col * row] = {0};
		int G[col * row] = {0};
		int B[col * row] = {0};
		int offset, offset_R, offset_G, offset_B;

		int x_slim = ctx->params->width / col;
		int y_slim = ctx->params->height / row;
		offset_R = 0;
		offset_G = ctx->params->width * ctx->params->height;
		offset_B = ctx->params->width * ctx->params->height * 2;

		for (i = 0; i < row; i++) {
			for (j = 0; j < col; j++) {
				offset = i * y_slim * ctx->params->width + j * x_slim ;
				for (k = 0; k < y_slim; k++) {
					for (l = 0; l < x_slim; l++) {
						R[i * col + j] += buffer[offset + offset_R + l + k * ctx->params->width];
						G[i * col + j] += buffer[offset + offset_G + l + k * ctx->params->width];
						B[i * col + j] += buffer[offset + offset_B + l + k * ctx->params->width];
					}
				}
				R[i * col + j] = R[i * col + j] / x_slim / y_slim;
				G[i * col + j] = G[i * col + j] / x_slim / y_slim;
				B[i * col + j] = B[i * col + j] / x_slim / y_slim;
				Y[i * col + j] = (int)(0.299 * R[i * col + j] + 0.587 * G[i * col + j] + 0.114 * B[i * col + j]);

				ctx->motion_detect_ctx->YRGB_data.RValueNow[i * col + j] = R[i * col + j];
				ctx->motion_detect_ctx->YRGB_data.GValueNow[i * col + j] = G[i * col + j];
				ctx->motion_detect_ctx->YRGB_data.BValueNow[i * col + j] = B[i * col + j];
				ctx->motion_detect_ctx->YRGB_data.YValueNow[i * col + j] = Y[i * col + j];
			}
		}
	}
	printf("\r\nCalculate YRGB after %dms.\n", (xTaskGetTickCount() - tick1));

	if (ctx->motion_detect_ctx->count == 0) {
		printf("initial_bgmodel\n\r");
		initial_bgmodel(ctx);
	}
	if (ctx->motion_detect_ctx->count % MOTION_DETECT_INTERVAL == 0) {
		motion_detect(ctx);
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
		memcpy(ctx->motion_detect_ctx->md_mask, (int *)arg, sizeof(ctx->motion_detect_ctx->md_mask));
		printf("Set MD Mask: \r\n");
		for (int j = 0; j < row; j++) {
			for (int k = 0; k < col; k++) {
				rt_printf("%d ", ctx->motion_detect_ctx->md_mask[j * col + k]);
			}
			rt_printf("\r\n");
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
		ctx->disp_postproc = (md_disp_postprcess *)arg;
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

	for (int i = 0; i < col * row; i++) {
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
