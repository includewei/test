/******************************************************************************
*
* Copyright(c) 2021 Realtek Corporation. All rights reserved.
*
******************************************************************************/

#include <stdint.h>
#include <stdlib.h>


#include "mmf2_module.h"
#include "osdep_service.h"
#include "module_facerecog.h"

#include "nn_api.h"
#include "nn_model_init.h"
#include "nn_model_info.h"

#include "nn_run_frc.h"

static uint32_t nn_measure_tick[8];


#if !defined(MODEL_FACE_USED) || !defined(MODEL_FACERECOG_USED)
#error MODEL_FACE_USED and MODEL_FACERECOG_USED should be 1
#endif

#if MODEL_FACE_USED==0 || MODEL_FACERECOG_USED==0
#error MODEL_FACE_USED and MODEL_FACERECOG_USED should be 1
#endif


#define ABS(x) ((x)>0?(x):-(x))

typedef enum {
	FRC_RECOGNITION = 0,
	FRC_REGISTER
} frc_mode_t;

#define MAX_FRC_REG_NUM		20

// TODO : load model from flash or files
typedef struct frc_ctx_s {
	frc_param_t params;

	MODEL_INFO_S model_face;
	MODEL_INFO_S model_frc;

	struct FRC_INFO_S frc_info[MAX_DETECT_OBJ_NUM];

	int dummy0[1024];

	// save and load those 3 members
	int reg_feature_num;
	float reg_feature[MAX_FRC_REG_NUM][NNMODEL_FR_FEATURE_NUM];
	char  reg_name[MAX_FRC_REG_NUM][32];

	int dummy1[1024];

	float face_feature[MAX_FRC_OBJ_NUM][NNMODEL_FR_FEATURE_NUM];

	int dummy2[1024];

	frc_mode_t mode;
	char tmp_reg_name[32];


} frc_ctx_t;



#define TICK_INIT()
#define TICK_GET() (uint32_t)xTaskGetTickCount()

#define NN_MEASURE_INIT(n)	do{nn_measure_tick[n] = 0; TICK_INIT();}while(0)
#define NN_MEASURE_START(n) do{nn_measure_tick[n] = TICK_GET();}while(0)
#define NN_MEASURE_STOP(n)  do{nn_measure_tick[n] = TICK_GET() - nn_measure_tick[n];}while(0)
#define NN_MEASURE_PRINT(n) do{printf("nn tick[%d] = %d\n\r", n, nn_measure_tick[n]);}while(0)
//#define NN_MEASURE_PRINT(n) do{}while(0)

#define MODEL_LOCATION

#include "fwfs.h"
#include "nn_model_info_ddr.h"

extern uint32_t __nn_eram_start__[];
extern uint32_t __nn_eram_end__[];
int facerecog_load_model(void *p)
{
	frc_ctx_t *ctx = (frc_ctx_t *)p;

	NN_MEASURE_START(0);
#if 0	// model in c array
	nn_model_interpreted(MODEL_FACE, &ctx->model_face);
	nn_model_interpreted(MODEL_FACERECOG, &ctx->model_frc);
#else
	// check current NN base address
	int dev_nn_base = (uint32_t)__nn_eram_start__;
	int dev_nn_size = (uint32_t)__nn_eram_end__ - (uint32_t)__nn_eram_start__;

	int model_size = 0;
	uint8_t *load_addr = NULL;

	printf("nn base %x, nn size %x\n\r", dev_nn_base, dev_nn_size);

	// fixed address required
	if (dev_nn_base > NNMODEL_FACE_ADDR_BASE) {
		printf("link script not correct\n\r");
		return -1;
	}
	if (dev_nn_base > NNMODEL_FR_ADDR_BASE) {
		printf("link script not correct\n\r");
		return -1;
	}

	void *fp = pfw_open("NN_MDL/model_facedetect.bin", M_NORMAL);
	if (!fp) {
		printf("open firmware fail\n\r");
		return -1;
	}

	pfw_seek(fp, 0, SEEK_END);
	model_size = pfw_tell(fp);
	pfw_seek(fp, 0, SEEK_SET);

	if (model_size > ABS((uint32_t)__nn_eram_end__ - NNMODEL_FACE_ADDR_BASE)) {
		printf("face detect, out of space request size %d > avaliable size %d\n\r", model_size, ABS((uint32_t)__nn_eram_end__ - NNMODEL_FACE_ADDR_BASE));
		return -1;
	}

	load_addr = (uint8_t *)NNMODEL_FACE_ADDR_BASE;
	pfw_read(fp, load_addr, model_size);
	pfw_close(fp);

	nn_model_info_set(MODEL_FACE, &ctx->model_face);

	fp = pfw_open("NN_MDL/model_facerecog.bin", M_NORMAL);
	if (!fp)	{
		printf("open firmware fail\n\r");
		return -1;
	}

	pfw_seek(fp, 0, SEEK_END);
	model_size = pfw_tell(fp);
	pfw_seek(fp, 0, SEEK_SET);

	if (model_size > ABS(NNMODEL_FACE_ADDR_BASE - NNMODEL_FR_ADDR_BASE)) {
		printf("face recognition, out of space request size %d > avaliable size %d\n\r", model_size, ABS(NNMODEL_FACE_ADDR_BASE - NNMODEL_FR_ADDR_BASE));
		return -1;
	}

	load_addr = (uint8_t *)NNMODEL_FR_ADDR_BASE;
	pfw_read(fp, load_addr, model_size);
	pfw_close(fp);

	nn_model_info_set(MODEL_FACE, &ctx->model_frc);
#endif
	NN_MEASURE_STOP(0);
	NN_MEASURE_PRINT(0);
}

void facerecog_check_dummy(void *p)
{
	frc_ctx_t *ctx = (frc_ctx_t *)p;

	int sum0 = 0;
	int sum1 = 0;
	int sum2 = 0;
	for (int i = 0; i < 1024; i++) {
		sum0 += ctx->dummy0[i];
	}
	if (sum0 != 0)	{
		printf("dummy0 polluted\n\r");
	}
	for (int i = 0; i < 1024; i++) {
		sum1 += ctx->dummy1[i];
	}
	if (sum1 != 0)	{
		printf("dummy1 polluted\n\r");
	}
	for (int i = 0; i < 1024; i++) {
		sum2 += ctx->dummy2[i];
	}
	if (sum2 != 0)	{
		printf("dummy2 polluted\n\r");
	}
}

void facerecog_dump_feature(void *feature, int feature_num)
{
	printf("register feature number %d\n\r", feature_num);
}

void facerecog_dump_frc(void *frc_info, int frc_num)
{
	struct FRC_INFO_S *info = (struct FRC_INFO_S *)frc_info;
	printf("---------%02d----------\n\r", frc_num);
	for (int i = 0; i < frc_num; i++) {
		struct cvRect_S *bb = &info[i].bbox;
		printf("%02d bbox : %d, %d, %d, %d\n\r", i, bb->xmin, bb->ymin, bb->xmax, bb->ymax);
		printf("%02d idx  : %d\n\r", i, info[i].reg_index);
		printf("%02d score: %f\n\r", i, info[i].score);
	}
	printf("--------------------\n\r");
}

//-----------------------------------------------------------------------
#ifndef NNIRQ_USED_SKIP
__attribute__((weak)) int nn_done;

void frc_irqhandler(void)
{
	uint32_t value;
	//AQIntrAcknowledge - Reading from this register clears outstanding interrupt
	value = HAL_READ32(NN_BASE, 0x10); //AQIntrAcknowledge

	hal_irq_clear_pending(NN_IRQn);
	nn_done = 1;
	printf("NN IRQ %x\n\r", value);

	uint32_t basepri = __get_BASEPRI();
	if (basepri != 0) {
		printf("in NN IRQ basepri = %d\n\r", basepri);
	}
}

void frc_irq_init(void)
{
	// IRQ vector may has been registered, disable and re-register it
	hal_irq_disable(NN_IRQn);
	__ISB();
	hal_irq_set_vector(NN_IRQn, (uint32_t)frc_irqhandler);
	hal_irq_set_priority(NN_IRQn, NN_IRQPri);
	hal_irq_enable(NN_IRQn);
}

void frc_irq_deinit(void)
{
	// IRQ vector may has been registered, disable and re-register it
	hal_irq_disable(NN_IRQn);
	__ISB();
	hal_irq_set_vector(NN_IRQn, (uint32_t)NULL);
	hal_irq_enable(NN_IRQn);
}
#else
void frc_irq_init(void)	{}
void frc_irq_deinit(void)	{}
#endif

//-----------------------------------------------------------------------

void facerecog_draw(void *p, frc_osd_draw_t draw_method, struct FRC_INFO_S *info, int num)
{
	frc_ctx_t *ctx = (frc_ctx_t *)p;

	if (ctx->reg_feature_num == 0) {
		return;
	}

	draw_method(0, 0, 0, 0, NULL, 1);	// reset counter
	for (int i = 0; i < num; i++) {
		struct cvRect_S *bb = &info[i].bbox;
		char name_str[64];
		sprintf(name_str, "%s %0.2f", ctx->reg_name[info[i].reg_index], info[i].score);
		draw_method(bb->xmin, bb->ymin, bb->xmax, bb->ymax, name_str, 0);
	}
	draw_method(0, 0, 0, 0, NULL, 2);	// set to voe
}

int facerecog_handle(void *p, void *input, void *output)
{
	frc_ctx_t *ctx = (frc_ctx_t *)p;
	mm_queue_item_t *input_item = (mm_queue_item_t *)input;
	uint8_t *frame_in = (uint8_t *)input_item->data_addr;

	// TBD
	int face_feature_num;
	//printf(">>frc %d\n\r", ctx->mode);

	if (ctx->mode == FRC_REGISTER) {
		int last_reg_num = ctx->reg_feature_num;
		// only process one frame, when user switch to register mode
		nn_face_register(&ctx->model_face, &ctx->model_frc,
						 frame_in, ctx->params.in_width, ctx->params.in_height, &ctx->params.roi,
						 ctx->frc_info, ctx->face_feature, &ctx->reg_feature_num, ctx->reg_feature);

		if (last_reg_num != ctx->reg_feature_num) {
			strcpy(ctx->reg_name[last_reg_num], ctx->tmp_reg_name);
		}
		ctx->mode = FRC_RECOGNITION;
		facerecog_dump_feature(&ctx->reg_feature, ctx->reg_feature_num);
	} else if (ctx->mode == FRC_RECOGNITION) {
		NN_MEASURE_START(1);
		nn_face_recog(&ctx->model_face, &ctx->model_frc,
					  frame_in, ctx->params.in_width, ctx->params.in_height, &ctx->params.roi,
					  ctx->frc_info, &face_feature_num,
					  ctx->face_feature, ctx->reg_feature_num, ctx->reg_feature);
		//facerecog_dump_frc(&ctx->frc_info, face_feature_num);
		NN_MEASURE_STOP(1);
		NN_MEASURE_PRINT(1);
		if (ctx->params.draw) {
			facerecog_draw(ctx, ctx->params.draw, ctx->frc_info, face_feature_num);
		}
	} else {

	}
	facerecog_check_dummy(ctx);
	//printf("<<frc\n\r");
	return 0;
}

#include "platform_opts.h"
#include "flash_api.h"
typedef struct face_data_s {
	int reg_feature_num;
	float reg_feature[MAX_FRC_REG_NUM][NNMODEL_FR_FEATURE_NUM];
	char  reg_name[MAX_FRC_REG_NUM][32];
} face_data_t;

// load feature file or read from flash position
static void facerecog_load_feature(void *p)
{
	frc_ctx_t *ctx = (frc_ctx_t *)p;
	// if file, open file and read to reg_feature_num/reg_feature
	// format
	// _0____1____2____3____4__....____F___
	//  reg_feature_num   |   reg_feature array(size = MAX_FRC_REG_NUM*(NNMODEL_FR_FEATURE_NUM*sizeof(float)+128))
	flash_t flash;

#if 0
	//uin32_t face_data_base = FLASH_BASE + FACE_FEATURE_DATA;
	uint8_t *buf = (uint8_t *)&ctx->reg_feature_num;
	flash_stream_read(&flash, FACE_FEATURE_DATA, sizeof(face_data_t), buf);
#else
	uint8_t *buf = (uint8_t *)&ctx->reg_feature_num;
	void *fp = pfw_open("UDATA", M_RAW);
	if (!fp) {
		printf("cannot open UDATA\n\r");
		return;
	}
	pfw_read(fp, buf, sizeof(face_data_t));
	pfw_close(fp);

	face_data_t *ff = (face_data_t *)buf;
	printf("Face feature %d\n\r", ff->reg_feature_num);
	for (int i = 0; i < ff->reg_feature_num; i++) {
		printf("%d : %s -", i, ff->reg_name[i]);
		for (int x = 0; x < NNMODEL_FR_FEATURE_NUM; x++) {
			printf("%02.1f", ff->reg_feature[i][x]);
		}
		printf("\n\r");
	}
#endif

}

static void facerecog_save_feature(void *p)
{
	frc_ctx_t *ctx = (frc_ctx_t *)p;
#if 0
	flash_t flash;

	uint8_t *buf = (uint8_t *)&ctx->reg_feature_num;
	flash_erase_sector(&flash, FACE_FEATURE_DATA);
	flash_erase_sector(&flash, FACE_FEATURE_DATA + 0x1000);
	flash_erase_sector(&flash, FACE_FEATURE_DATA + 0x2000);
	flash_erase_sector(&flash, FACE_FEATURE_DATA + 0x3000);
	flash_stream_write(&flash, FACE_FEATURE_DATA, sizeof(face_data_t), buf);
#else
	uint8_t *buf = (uint8_t *)&ctx->reg_feature_num;

	face_data_t *ff = (face_data_t *)buf;
	printf("Face feature %d\n\r", ff->reg_feature_num);
	for (int i = 0; i < ff->reg_feature_num; i++) {
		printf("%d : %s -", i, ff->reg_name[i]);
		for (int x = 0; x < NNMODEL_FR_FEATURE_NUM; x++) {
			printf("%02.1f", ff->reg_feature[i][x]);
		}
		printf("\n\r");
	}

	void *fp = pfw_open("UDATA", M_RAW | M_CREATE);
	if (!fp) {
		printf("cannot open UDATA\n\r");
		return;
	}
	pfw_write(fp, buf, sizeof(face_data_t));
	pfw_close(fp);
#endif
}


static void facerecog_reset_feature(void *p)
{
	frc_ctx_t *ctx = (frc_ctx_t *)p;
	ctx->reg_feature_num = 1;
	for (int i = 0; i < NNMODEL_FR_FEATURE_NUM; i++) {
		ctx->reg_feature[0][i] = 0.0;
	}
	strcpy(ctx->reg_name[0], "unknown");
}

int facerecog_control(void *p, int cmd, int arg)
{
	frc_ctx_t *ctx = (frc_ctx_t *)p;

	switch (cmd) {
	case CMD_FRC_SET_PARAMS:
		memcpy(&ctx->params, (void *)arg, sizeof(frc_param_t));
		break;
	case CMD_FRC_SET_ROI:
		memcpy(&ctx->params.roi, (void *)arg, sizeof(struct cvRect_S));
		break;
	case CMD_FRC_SET_IMG_WIDTH:
		ctx->params.in_width = arg;
		break;
	case CMD_FRC_SET_IMG_HEIGHT:
		ctx->params.in_height = arg;
		break;
	case CMD_FRC_SET_OSD_DRAW:
		ctx->params.draw = (frc_osd_draw_t)arg;
		break;
	case CMD_FRC_REGISTER_MODE:
		ctx->mode = FRC_REGISTER;
		if (arg) {
			strncpy(ctx->tmp_reg_name, (char *)arg, 31);
			printf("reg mode %d, reg name %s\n\r", ctx->mode, ctx->tmp_reg_name);
		} else {
			printf("reg mode %d\n\r", ctx->mode);
		}
		break;
	case CMD_FRC_RECOGNITION_MODE:
		ctx->mode = FRC_RECOGNITION;
		printf("rec mode %d\n\r", ctx->mode);
		break;
	case CMD_FRC_LOAD_FEATURES:
		facerecog_load_feature(p);
		break;
	case CMD_FRC_SAVE_FEATURES:
		facerecog_save_feature(p);
		break;
	case CMD_FRC_RESET_FEATURES:
		facerecog_reset_feature(p);
		break;
	}

	return 0;
}

void *facerecog_destroy(void *p)
{
	frc_ctx_t *ctx = (frc_ctx_t *)p;

	if (ctx) {
		free(ctx);
	}

	frc_irq_deinit();
	hal_sys_peripheral_en(NN_SYS, DISABLE);
	return NULL;
}

void *facerecog_create(void *parent)
{
	frc_ctx_t *ctx = (frc_ctx_t *)malloc(sizeof(frc_ctx_t));

	if (!ctx) {
		return NULL;
	}
	memset(ctx, 0, sizeof(frc_ctx_t));

	NN_MEASURE_INIT(0);
	NN_MEASURE_INIT(1);
	NN_MEASURE_INIT(2);
	NN_MEASURE_INIT(3);
	NN_MEASURE_INIT(4);

	// setup NN hw
	hal_sys_set_clk(NN_SYS, NN_500M);
	hal_sys_peripheral_en(NN_SYS, ENABLE);
	int val = hal_sys_get_clk(NN_SYS);
	dbg_printf("hal_sys_get_clk %x \n", val);

	nn_hw_version();
	nn_set_mode();
	nn_clkcontrol(0);  //PPU & core_clock

	/*
	nn_model_download(MODEL_FACE);
	nn_model_download(MODEL_FACERECOG);
	nn_model_info_set(MODEL_FACE, &ctx->model_face);
	nn_model_info_set(MODEL_FACERECOG, &ctx->model_frc);
	*/

	if (facerecog_load_model(ctx) < 0) {
		free(ctx);
		return NULL;
	}

	// init unknown face feature???
	ctx->reg_feature_num = 1;
	for (int i = 0; i < NNMODEL_FR_FEATURE_NUM; i++) {
		ctx->reg_feature[0][i] = 0.0;
	}
	strcpy(ctx->reg_name[0], "unknown");

	frc_irq_init();

	return ctx;
}

/*
 * example1, create module => control(load feature)
 * example2, create module => register mode => save feature => switch to recognition mode
 *
 */


mm_module_t facerecog_module = {
	.create = facerecog_create,
	.destroy = facerecog_destroy,
	.control = facerecog_control,
	.handle = facerecog_handle,

	.output_type = MM_TYPE_NONE,
	.module_type = MM_TYPE_VDSP,
	.name = "facerecog"
};
