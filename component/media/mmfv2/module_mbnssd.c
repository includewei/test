/******************************************************************************
*
* Copyright(c) 2021 Realtek Corporation. All rights reserved.
*
******************************************************************************/

#include <stdint.h>
#include <stdlib.h>

#include "mmf2_module.h"
#include "osdep_service.h"
#include "module_mbnssd.h"

#include "nn_api.h"
#include "nn_model_init.h"
#include "nn_model_info.h"

//#include "nn_run_frc.h"

static uint32_t nn_measure_tick[8];


#if !defined(MODEL_SSDM_USED) || !defined(MODEL_SSDM_USED)
#error MODEL_SSDM_USED should be 1
#endif

typedef struct mbnssd_output_s {
	float detect_result[MAX_DETECT_OBJ_NUM * 6];
	int detect_obj_num;
} mbnssd_output_t;

// TODO : load model from flash or files
typedef struct mbnssd_ctx_s {
	mbnssd_param_t params;

	int roi_w, roi_h;

	MODEL_INFO_S model_ssd;
} mbnssd_ctx_t;



#define TICK_INIT()
#define TICK_GET() (uint32_t)xTaskGetTickCount()

#define NN_MEASURE_INIT(n)	do{nn_measure_tick[n] = 0; TICK_INIT();}while(0)
#define NN_MEASURE_START(n) do{nn_measure_tick[n] = TICK_GET();}while(0)
#define NN_MEASURE_STOP(n)  do{nn_measure_tick[n] = TICK_GET() - nn_measure_tick[n];}while(0)
//#define NN_MEASURE_PRINT(n) do{printf("nn tick[%d] = %d\n\r", n, nn_measure_tick[n]);}while(0)
#define NN_MEASURE_PRINT(n) do{}while(0)

typedef struct ssd_box_s {
	float id;
	float score;
	float xmin, ymin, xmax, ymax;
} ssd_box_t;

void mbnssd_draw(void *p, ssd_osd_draw_t draw_method, float *result, int num)
{
	mbnssd_ctx_t *ctx = (mbnssd_ctx_t *)p;

	ssd_box_t *bbox = (ssd_box_t *)result;

	draw_method(0, 0, 0, 0, NULL, 1);	// reset counter
	for (int i = 0; i < num; i++) {
		ssd_box_t *bb = &bbox[i];
		char name_str[64];
		sprintf(name_str, "%d %0.2f", (int)bb->id, bb->score);
		int xmin = (int)(bb->xmin * ctx->roi_w);
		int ymin = (int)(bb->ymin * ctx->roi_h);
		int xmax = (int)(bb->xmax * ctx->roi_w);
		int ymax = (int)(bb->ymax * ctx->roi_h);
		//printf("<%2d %2d %2d %2d>\n\r", xmin, ymin, xmax, ymax);
		draw_method(xmin, ymin, xmax, ymax, name_str, 0);
	}
	draw_method(0, 0, 0, 0, NULL, 2);	// set to voe
}

//-----------------------------------------------------------------------
#ifndef NNIRQ_USED_SKIP
__attribute__((weak)) int nn_done;

void mbnssd_irqhandler(void)
{
	uint32_t value;
	//AQIntrAcknowledge - Reading from this register clears outstanding interrupt
	value = HAL_READ32(NN_BASE, 0x10); //AQIntrAcknowledge

	hal_irq_clear_pending(NN_IRQn);
	nn_done = 1;
	//printf("NN IRQ %x\n\r", value);
}

void mbnssd_irq_init(void)
{
	// IRQ vector may has been registered, disable and re-register it
	hal_irq_disable(NN_IRQn);
	__ISB();
	hal_irq_set_vector(NN_IRQn, (uint32_t)mbnssd_irqhandler);
	hal_irq_set_priority(NN_IRQn, NN_IRQPri);
	hal_irq_enable(NN_IRQn);
}

void mbnssd_irq_deinit(void)
{
	// IRQ vector may has been registered, disable and re-register it
	hal_irq_disable(NN_IRQn);
	__ISB();
	hal_irq_set_vector(NN_IRQn, (uint32_t)NULL);
	hal_irq_enable(NN_IRQn);
}
#else
void mbnssd_irq_init(void)	{}
void mbnssd_irq_deinit(void)	{}
#endif

//-----------------------------------------------------------------------

int mbnssd_handle(void *p, void *input, void *output)
{
	mbnssd_ctx_t *ctx = (mbnssd_ctx_t *)p;
	mm_queue_item_t *input_item = (mm_queue_item_t *)input;
	uint8_t *frame_in = (uint8_t *)input_item->data_addr;

	mm_queue_item_t *output_item = (mm_queue_item_t *)output;
	mbnssd_output_t *out = NULL;
	mbnssd_output_t default_out;

	if (output_item && output_item->data_addr) {
		out = (mbnssd_output_t *)output_item->data_addr;
	} else {
		out = &default_out;
	}

	int status = nn_detect_interpreted(MOBILENETSSD_20OBJ, &ctx->model_ssd,
									   frame_in, &ctx->params.roi,
									   out->detect_result, &out->detect_obj_num);

	if (ctx->params.draw) {
		mbnssd_draw(ctx, ctx->params.draw, out->detect_result, out->detect_obj_num);
	}


	return 0;
}

int mbnssd_control(void *p, int cmd, int arg)
{
	mbnssd_ctx_t *ctx = (mbnssd_ctx_t *)p;

	switch (cmd) {
	case CMD_SSD_SET_PARAMS:
		memcpy(&ctx->params, (void *)arg, sizeof(mbnssd_param_t));
		ctx->roi_w = ctx->params.roi.xmax - ctx->params.roi.xmin;
		ctx->roi_h = ctx->params.roi.ymax - ctx->params.roi.ymin;
		break;
	case CMD_SSD_SET_ROI:
		memcpy(&ctx->params.roi, (void *)arg, sizeof(struct cvRect_S));
		ctx->roi_w = ctx->params.roi.xmax - ctx->params.roi.xmin;
		ctx->roi_h = ctx->params.roi.ymax - ctx->params.roi.ymin;
		break;
	case CMD_SSD_SET_IMG_WIDTH:
		ctx->params.in_width = arg;
		break;
	case CMD_SSD_SET_IMG_HEIGHT:
		ctx->params.in_height = arg;
		break;
	case CMD_SSD_SET_OSD_DRAW:
		ctx->params.draw = (ssd_osd_draw_t)arg;
		break;
	}

	return 0;
}

void *mbnssd_destroy(void *p)
{
	mbnssd_ctx_t *ctx = (mbnssd_ctx_t *)p;

	if (ctx) {
		free(ctx);
	}

	mbnssd_irq_deinit();
	hal_sys_peripheral_en(NN_SYS, DISABLE);
	return NULL;
}

void *mbnssd_create(void *parent)
{
	mbnssd_ctx_t *ctx = (mbnssd_ctx_t *)malloc(sizeof(mbnssd_ctx_t));

	if (!ctx) {
		return NULL;
	}
	memset(ctx, 0, sizeof(mbnssd_ctx_t));

	// setup NN hw
	hal_sys_set_clk(NN_SYS, NN_500M);
	hal_sys_peripheral_en(NN_SYS, ENABLE);
	int val = hal_sys_get_clk(NN_SYS);
	dbg_printf("hal_sys_get_clk %x \n", val);

	nn_hw_version();
	nn_set_mode();
	nn_clkcontrol(0);  //PPU & core_clock

	nn_model_interpreted(MOBILENETSSD_20OBJ, &ctx->model_ssd);

	mbnssd_irq_init();

	NN_MEASURE_INIT(0);
	NN_MEASURE_INIT(1);
	NN_MEASURE_INIT(2);
	NN_MEASURE_INIT(3);
	NN_MEASURE_INIT(4);

	return ctx;
}

/*
 * example1, create module => control(load feature)
 * example2, create module => register mode => save feature => switch to recognition mode
 *
 */


mm_module_t mbnssd_module = {
	.create = mbnssd_create,
	.destroy = mbnssd_destroy,
	.control = mbnssd_control,
	.handle = mbnssd_handle,

	.output_type = MM_TYPE_NONE,
	.module_type = MM_TYPE_VDSP,
	.name = "mbnssd"
};
