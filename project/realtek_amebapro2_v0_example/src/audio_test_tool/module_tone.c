/******************************************************************************
*
* Copyright(c) 2007 - 2018 Realtek Corporation. All rights reserved.
*
******************************************************************************/

#include <stdint.h>
#include <math.h>
#include "platform_stdlib.h"
#include "osdep_service.h"
#include "avcodec.h"
#include "mmf2_module.h"
#include "module_tone.h"

//------------------------------------------------------------------------------
void toneframe_timer_handler(uint32_t hid);

#define TIMER_FUNCTION


static void tone_timer_thread(void *param)
{
	tone_ctx_t *ctx = (tone_ctx_t *)param;
	while (1) {
		vTaskDelay(ctx->audio_timer_delay_ms);
		toneframe_timer_handler((uint32_t)ctx);
	}
}

void tonetimer_task_enable(void *parm)
{
	tone_ctx_t *ctx = (tone_ctx_t *)parm;

	if (xTaskCreate(tone_timer_thread, ((const char *)"tone_timer_thread"), 2048, ctx, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
		printf("\n\r%s xTaskCreate failed", __FUNCTION__);
	}
}


void toneframe_timer_handler(uint32_t hid)
{
	tone_ctx_t *ctx = (tone_ctx_t *)hid;
	uint32_t i;
	short tx_pattern;
	float temp, n_cnt, m_cnt;

	if (ctx->stop) {
		return;
	}

	BaseType_t xTaskWokenByReceive = pdFALSE;
	BaseType_t xHigherPriorityTaskWoken;

#ifdef TIMER_FUNCTION
	uint32_t timestamp = xTaskGetTickCountFromISR();
#else
	uint32_t timestamp = xTaskGetTickCount();
#endif

	mm_context_t *mctx = (mm_context_t *)ctx->parent;
	mm_queue_item_t *output_item;

#ifdef TIMER_FUNCTION
	int is_output_ready = xQueueReceiveFromISR(mctx->output_recycle, &output_item, &xTaskWokenByReceive) == pdTRUE;
#else
	int is_output_ready = xQueueReceive(mctx->output_recycle, &output_item, 1000) == pdTRUE;
#endif
	if (is_output_ready) {

		output_item->type = ctx->params.codec_id;
		output_item->timestamp = timestamp;
		output_item->size = ctx->params.frame_size;
		n_cnt = ctx->params.audiotonerate;
		m_cnt = ctx->params.samplerate;
		short *output_buf = (short *)output_item->data_addr;

		for (i = 0; i < (ctx->params.frame_size >> 1); i++) {
			if (ctx->tone_data_offset == ctx->params.samplerate) {
				ctx->tone_data_offset = 0;
			}
			temp = sin((2 * (3.141592653589793f) * (float)n_cnt) / m_cnt * (ctx->tone_data_offset)) * (32767.0f);// / (pow(10, 3.0f/20));
			tx_pattern = (short)temp;
			output_buf[i] = tx_pattern;
			ctx->tone_data_offset++;
		}
#ifdef TIMER_FUNCTION
		xQueueSendFromISR(mctx->output_ready, (void *)&output_item, &xHigherPriorityTaskWoken);
#else
		xQueueSend(mctx->output_ready, (void *)&output_item, 0);
#endif
	}
#ifdef TIMER_FUNCTION
	if (xHigherPriorityTaskWoken || xTaskWokenByReceive) {
		taskYIELD();
	}
#endif
}

int tone_control(void *p, int cmd, int arg)
{
	tone_ctx_t *ctx = (tone_ctx_t *)p;

	switch (cmd) {
	case CMD_TONE_SET_PARAMS:
		memcpy(&ctx->params, (void *)arg, sizeof(tone_params_t));
		if (ctx->params.samplerate != 8000 && ctx->params.samplerate != 16000 && ctx->params.samplerate != 32000 && ctx->params.samplerate != 44100 &&
			ctx->params.samplerate != 48000 && ctx->params.samplerate != 88200 && ctx->params.samplerate != 96000) {
			ctx->params.samplerate = 16000;
			mm_printf("Invalid sample rate, Set default sample rate: %d\r\n", ctx->params.audiotonerate);
		}
		if (ctx->params.audiotonerate < 0 || ctx->params.audiotonerate > (ctx->params.samplerate >> 1)) {
			ctx->params.audiotonerate = 1000;
			mm_printf("Invalid audio tone rate, Set default audio tone: %d\r\n", ctx->params.audiotonerate);
		}
		break;
	case CMD_TONE_GET_PARAMS:
		memcpy((void *)arg, &ctx->params, sizeof(tone_params_t));
		break;
	case CMD_TONE_SET_AUDIOTONE:
		mm_printf("SET_AUDIOTONE\r\n");
		if (arg > 0 && arg < (ctx->params.samplerate >> 1)) {
			ctx->params.audiotonerate = arg;
			mm_printf("Set audio tone: %d\r\n", ctx->params.audiotonerate);
		} else {
			ctx->params.audiotonerate = 1000;
			mm_printf("Invalid audio tone rate for sample rate %d, Set default audio tone: %d\r\n", ctx->params.samplerate, ctx->params.audiotonerate);
		}
		ctx->tone_data_offset = 0;
		break;
	case CMD_TONE_SET_SAMPLERATE:
		mm_printf("SET_SAMPLERATE\r\n");
		if (arg == 8000 || arg == 16000 || arg == 32000 || arg == 44100 || arg == 48000 || arg == 88200 || arg == 96000) {
			ctx->params.samplerate = arg;
		} else {
			ctx->params.samplerate = 16000;
			mm_printf("Invalid sample rate.");
		}
		mm_printf("Set default sample rate: %d\r\n", ctx->params.audiotonerate);
		if (ctx->params.audiotonerate >= (ctx->params.samplerate >> 1)) {
			ctx->params.audiotonerate = 1000;
			mm_printf("Invalid audio tone rate for sample rate %d, Set default audio tone: %d\r\n", ctx->params.samplerate, ctx->params.audiotonerate);
		}
		ctx->tone_data_offset = 0;
		break;
	case CMD_TONE_RECOUNT_PERIOD:
		if (ctx->params.codec_id == AV_CODEC_ID_PCM_RAW) {
			ctx->frame_timer_period = (int)(1000000 / ((float)ctx->params.samplerate * 2 / ctx->params.frame_size));
		} else {
			return -1;
		}

		if (ctx->frame_timer_period == 0) {
			printf("Error, frame_timer_period can't be 0\n\r");
			return -1;
		}
		printf("Recount frame_timer_period success\n\r");
		ctx->audio_timer_delay_ms = ctx->frame_timer_period / 1000;
		break;
	case CMD_TONE_APPLY:
		if (ctx->params.codec_id == AV_CODEC_ID_PCM_RAW) {
			ctx->frame_timer_period = (int)(1000000 / ((float)ctx->params.samplerate * 2 / ctx->params.frame_size));
		} else {
			return -1;
		}

		if (ctx->frame_timer_period == 0) {
			printf("Error, frame_timer_period can't be 0\n\r");
			return -1;
		}
#ifdef TIMER_FUNCTION
		gtimer_init(&ctx->frame_timer, 0xff);
#else
		ctx->audio_timer_delay_ms = ctx->frame_timer_period / 1000;
#endif

		break;
	case CMD_TONE_GET_STATE:
		*(int *)arg = ((ctx->stop) ? 0 : 1);
		break;
	case CMD_TONE_STREAMING:
		if (arg == 1) {	// stream on
			if (ctx->stop) {
#ifdef TIMER_FUNCTION
				gtimer_start_periodical(&ctx->frame_timer, ctx->frame_timer_period, (void *)toneframe_timer_handler, (uint32_t)ctx);
#else
				timer_task_enable(ctx);
#endif
				ctx->stop = 0;
				ctx->tone_data_offset = 0;
			}
		} else {			// stream off
			if (!ctx->stop) {
#ifdef TIMER_FUNCTION
				gtimer_stop(&ctx->frame_timer);
#endif
				ctx->stop = 1;
			}
		}
		break;
	}
	return 0;
}

int tone_handle(void *ctx, void *input, void *output)
{
	return 0;
}

void *tone_destroy(void *p)
{
	tone_ctx_t *ctx = (tone_ctx_t *)p;

	if (ctx->stop == 0) {
		tone_control((void *)ctx, CMD_TONE_STREAMING, 0);
	}

	if (ctx && ctx->up_sema) {
		rtw_free_sema(&ctx->up_sema);
	}
	if (ctx && ctx->task) {
		vTaskDelete(ctx->task);
	}
	if (ctx)	{
		free(ctx);
	}
	return NULL;
}

void *tone_create(void *parent)
{
	tone_ctx_t *ctx = malloc(sizeof(tone_ctx_t));
	if (!ctx) {
		return NULL;
	}
	memset(ctx, 0, sizeof(tone_ctx_t));

	ctx->parent = parent;

	ctx->stop = 1;
	rtw_init_sema(&ctx->up_sema, 0);

	return ctx;

}

void *tone_new_item(void *p)
{
	tone_ctx_t *ctx = (tone_ctx_t *)p;

	return (void *)malloc(ctx->params.frame_size);
}

void *tone_del_item(void *p, void *d)
{
	(void)p;
	if (d) {
		free(d);
	}
	return NULL;
}

mm_module_t tone_module = {
	.create = tone_create,
	.destroy = tone_destroy,
	.control = tone_control,
	.handle = tone_handle,

	.new_item = tone_new_item,
	.del_item = tone_del_item,

	.output_type = MM_TYPE_ASINK | MM_TYPE_ADSP,
	.module_type = MM_TYPE_ASRC,
	.name = "TONE"
};