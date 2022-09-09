/******************************************************************************
*
* Copyright(c) 2007 - 2018 Realtek Corporation. All rights reserved.
*
******************************************************************************/

#include <stdint.h>
#include <stdlib.h>

#include "mmf2_module.h"
#include "osdep_service.h"
#include "module_rtknn.h"

#include "nn_api.h"
#include "nn_model_init.h"
//#include "ssd_post_process.h"


#include "hal_cache.h"
#include "hal_video.h"
#include "hal_isp.h"

uint32_t nn_measure_tick[8];


#define TICK_INIT()
#define TICK_GET() (uint32_t)xTaskGetTickCount()

#define NN_MEASURE_INIT(n)	do{nn_measure_tick[n] = 0; TICK_INIT();}while(0)
#define NN_MEASURE_START(n) do{nn_measure_tick[n] = TICK_GET();}while(0)
#define NN_MEASURE_STOP(n)  do{nn_measure_tick[n] = TICK_GET() - nn_measure_tick[n];}while(0)
//#define NN_MEASURE_PRINT(n) do{printf("nn tick[%d] = %d\n\r", n, nn_measure_tick[n]);}while(0)
#define NN_MEASURE_PRINT(n) do{}while(0)

extern uint32_t __nn_eram_start__[];
//----------> For WD1 NN driver
//int VIDEO_MEMORY_HEAP_BASE_ADDRESS = (uint32_t)__nn_eram_start__;
int nn_done = 0;
//<-----------------------------

static int current_model = 0xffff;
static int nn_mutex_init = 0;
static _mutex nn_mutex;

static int nn_time0 = 0;
static int nn_count = 0;
static float nn_fps = 0;


int rtknn_model_loader(struct MODEL_INFO_S *info, int model_id, uint8_t *model)
{
	if (current_model != model_id) {

		hal_sys_set_clk(NN_SYS, NN_500M);
		hal_sys_peripheral_en(NN_SYS, ENABLE);
		//hal_sys_set_clk(NN_SYS, NN_250M);
		int val = hal_sys_get_clk(NN_SYS);
		dbg_printf("hal_rtl_sys_get_clk %x \n", val);

		nn_hw_version();
		nn_set_mode();
		nn_clkcontrol(0);  //PPU & core_clock

		//Download model to pre-defined executable location
		NN_MEASURE_START(0);
		viplite_parser(info, model, (uint32_t)__nn_eram_start__);
		dcache_clean();	// <- very dangerous may damange hw shared memory,
		NN_MEASURE_STOP(0);
		NN_MEASURE_PRINT(0);

		dbg_printf("nnreg_CmdBufferAHBControl: %x\r\n", info->nnreg_CmdBufferAHBControl);
		dbg_printf("nnreg_AQCmdBufferAddr    : %x\r\n", info->nnreg_AQCmdBufferAddr);
		dbg_printf("tensor0Out_addr          : %x\r\n", info->tensor0Out_addr);
		dbg_printf("tensor1Out_addr          : %x\r\n", info->tensor1Out_addr);
		dbg_printf("tensor0in_addr           : %x\r\n", info->tensor0in_addr);
		dbg_printf("model_size               : %d\r\n", info->model_size);

		current_model = model_id;
	}
	return 0;
}

void rtknn_lock_init(void)
{
	if (nn_mutex_init == 0) {
		rtw_mutex_init(&nn_mutex);
		nn_mutex_init = 1;
	}
}

void rtknn_lock(void)
{
	rtknn_lock_init();
	rtw_mutex_get(&nn_mutex);

	//printf("->NN %d\n\r", current_model);
}

void rtknn_unlock(void)
{
	rtw_mutex_put(&nn_mutex);
	//printf("<-NN\n\r", current_model);
}

//------------------------------------------------------------------------------
static _sema rtknn_irq_sema;

void rtknn_irqhandler(void)
{
	u32 data_word;
	//AQIntrAcknowledge - Reading from this register clears outstanding interrupt
	data_word = HAL_READ32(NN_BASE, 0x10); //AQIntrAcknowledge
	//dbg_printf("NN_IRQHandler NN Status %08x; read to clear interrupt\r\n", data_word);
	data_word = HAL_READ32(NN_BASE, 0x10); //AQIntrAcknowledge
	//dbg_printf("NN_IRQHandler NN Status %08x; read again to check clear \r\n", data_word);

	hal_irq_clear_pending(NN_IRQn);

	// raise semaphore to resume waiting task
	rtw_up_sema_from_isr(&rtknn_irq_sema);
}

void rtknn_irq_init(void)
{
	rtw_init_sema(&rtknn_irq_sema, 0);
	// IRQ vector may has been registered, disable and re-register it
	hal_irq_disable(NN_IRQn);
	__ISB();
	hal_irq_set_vector(NN_IRQn, (uint32_t)rtknn_irqhandler);
	hal_irq_enable(NN_IRQn);
}

void rtknn_irq_deinit(void)
{
	// IRQ vector may has been registered, disable and re-register it
	hal_irq_disable(NN_IRQn);
	__ISB();
	hal_irq_set_vector(NN_IRQn, (uint32_t)NULL);
	hal_irq_enable(NN_IRQn);

	rtw_free_sema(&rtknn_irq_sema);
}

int rtknn_wait_finish(void)
{
	int timeout = 2000 / 100;
	int res = 0;
	int counter = 0;
	while (counter <= timeout) {
		counter++;
		if (rtw_down_timeout_sema(&rtknn_irq_sema, 100) == pdTRUE) {
			uint32_t data_word = HAL_READ32(NN_BASE, 0x4); //AQHidle
			if (data_word == 0x7fffffff) {
				break;
			}
		}
		if (counter > timeout) {
			printf("Timeout for wating NN finish\n\r");
			res = -1;
			break;
		}
	}
	return res;
}


int rtknn_preprocess(void *p, void *s, void *d, void *r, unsigned int removed_permute)
{
	rtknn_ctx_t *ctx = (rtknn_ctx_t *)p;
	cvImage *src = (cvImage *)s;
	cvImage *dst = (cvImage *)d;
	cvRect_S *roi = (cvRect_S *)r;

	// resize src ROI area to dst
	//nn_resize_rgb24(src, dst, roi, removed_permute);
	nn_resize_rgb888(src, dst, roi, removed_permute);

	dcache_clean_by_addr((uint32_t *)dst->data, dst->width * dst->height * 3);
	//dcache_clean();		// if don't care other hardware dma, you can use this. ugly method, should clean dst address with its data size
	return 0;
}

//obj_ctrl_s sw_object;
//int objset = 0;
extern void mbnetssd_postprocess(unsigned char *loc_nn, unsigned char *conf_nn, float *detect_result, int *detect_obj_num);
int rtknn_postprocess_ssd(void *p, void *r)
{
	//return 0;
	int i;
	rtknn_ctx_t *ctx = (rtknn_ctx_t *)p;
	rtknn_res_t *res = (rtknn_res_t *)r;

	NN_MEASURE_START(4);
	/*
	nn_post_process_detect(ctx->model_id,
						   (unsigned char *)ctx->params.info.tensor0Out_addr,  //loc
						   (unsigned char *)ctx->params.info.tensor1Out_addr, 	//scores
						   res->result, &res->obj_num);
	*/
	mbnetssd_postprocess((unsigned char *)ctx->params.info.tensor0Out_addr,    //loc
						 (unsigned char *)ctx->params.info.tensor1Out_addr, 	//scores
						 res->result, &res->obj_num);
	NN_MEASURE_STOP(4);
	NN_MEASURE_PRINT(4);

	if (ctx->set_object) {
		ctx->set_object(res);
	}

	//nn_display_results(ctx->model_id, res->result, &res->obj_num);


	return 0;
}

rtknn_res_t nnobject;

int rtknn_handle(void *p, void *input, void *output)
{
	int ret = -1;
	rtknn_ctx_t *ctx = (rtknn_ctx_t *)p;
	rtknn_res_t *res = (rtknn_res_t *)&nnobject;
	mm_queue_item_t *input_item = (mm_queue_item_t *)input;


	if (nn_time0 == 0) {
		nn_time0 = (int)xTaskGetTickCount();
	}

	// if (ctx->params.in_fps>0 && ctx->last_tick != 0) {
	// if (TICK_GET() - ctx->last_tick <= (1000 / ctx->params.in_fps)) {
	// return 0;
	// }
	// }
	ctx->last_tick = TICK_GET();

	rtknn_lock();
	ret = rtknn_model_loader(&ctx->params.info, ctx->model_id, ctx->model);


	cvImage  img_in;
	cvImage  img_out;
	unsigned int remove_permute;

	img_in.width  = ctx->params.in_width;
	img_in.height = ctx->params.in_height;

	img_out.width  = ctx->params.m_width;       //Identify from NB file
	img_out.height = ctx->params.m_height;      //Identify from NB file

	img_in.data   = (unsigned char *)input_item->data_addr;
	img_out.data   = (unsigned char *)ctx->params.info.tensor0in_addr;
	remove_permute = ctx->params.m_permute; 	//img_out is Planer


	NN_MEASURE_START(1);
	if (ctx->pre_process) {
		ctx->pre_process(ctx, &img_in, &img_out, &ctx->params.roi, remove_permute);
	}
	NN_MEASURE_STOP(1);
	NN_MEASURE_PRINT(1);

	NN_MEASURE_START(2);
	nn_process_kickstart(&ctx->params.info);
	nn_process_pollend();
	////rtknn_wait_finish();
	NN_MEASURE_STOP(2);
	NN_MEASURE_PRINT(2);

	NN_MEASURE_START(3);
	if (ctx->post_process) {
		ctx->post_process(ctx, res);
	}

	NN_MEASURE_STOP(3);
	NN_MEASURE_PRINT(3);

	//printf("rtknn_handle end = %d\r\n",xTaskGetTickCount() - ctx->last_tick);
	nn_count++;

	if (nn_count % 16 == 0) {
		nn_fps = (float)nn_count * 1000.0 / (float)(xTaskGetTickCount() - nn_time0);
		printf("NN_FPS = %0.2f, %d %d\n\r", nn_fps, nn_count, xTaskGetTickCount() - nn_time0);
	}

rtknn_exit:
	rtknn_unlock();

	return ret;
}



int rtknn_control(void *p, int cmd, int arg)
{
	rtknn_ctx_t *ctx = (rtknn_ctx_t *)p;

	switch (cmd) {
	case CMD_RTKNN_SET_MODEL_ID:
		ctx->params.model_id = arg;
		ctx->model_id = arg;
		break;
	case CMD_RTKNN_SET_MODEL:
		ctx->params.model = (uint8_t *)arg;
		ctx->model = (uint8_t *)arg;
		break;
	case CMD_RTKNN_SET_PARAMS:
		memcpy(&ctx->params, ((rtknn_params_t *)arg), sizeof(rtknn_params_t));
		ctx->model = ctx->params.model;
		ctx->model_id = ctx->params.model_id;
		break;

	case CMD_RTKNN_GET_PARAMS:
		memcpy(((rtknn_params_t *)arg), &ctx->params, sizeof(rtknn_params_t));
		break;
	case CMD_RTKNN_SET_WIDTH:
		ctx->params.in_width = arg;
		break;
	case CMD_RTKNN_SET_HEIGHT:
		ctx->params.in_height = arg;
		break;
	case CMD_RTKNN_SET_FPS:
		ctx->params.in_fps = arg;
		break;
	case CMD_RTKNN_SET_PREPROC:
		ctx->pre_process = (rtknn_preproc_t)arg;
		break;
	case CMD_RTKNN_SET_POSTPROC:
		ctx->post_process = (rtknn_postproc_t)arg;
		break;
	case CMD_RTKNN_SET_SETOBJECT:
		ctx->set_object = (rtknn_setoject_t)arg;
		break;
	case CMD_RTKNN_GET_HWVER:
		break;
	}

	return 0;
}

void *rtknn_destroy(void *p)
{
	rtknn_ctx_t *ctx = (rtknn_ctx_t *)p;

	//rtknn_irq_deinit();

	if (ctx) {
		free(ctx);
	}

	return NULL;
}

void *rtknn_create(void *parent)
{
	rtknn_ctx_t *ctx = (rtknn_ctx_t *)malloc(sizeof(rtknn_ctx_t));

	ctx->pre_process = rtknn_preprocess;
	ctx->post_process = rtknn_postprocess_ssd;
	ctx->set_object = NULL;

	// default model?
	//ctx->params.model = (uint8_t*)mobilenet_ssd_uint8;
	ctx->params.in_fps = 1;

	//rtknn_irq_init();

	NN_MEASURE_INIT(0);
	NN_MEASURE_INIT(1);
	NN_MEASURE_INIT(2);
	NN_MEASURE_INIT(3);
	NN_MEASURE_INIT(4);
	return ctx;
}


mm_module_t rtknn_module = {
	.create = rtknn_create,
	.destroy = rtknn_destroy,
	.control = rtknn_control,
	.handle = rtknn_handle,

	.output_type = MM_TYPE_NONE,
	.module_type = MM_TYPE_VDSP,
	.name = "rtk nn"
};
