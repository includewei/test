//------------------------------------------------------
// YAMNET
//------------------------------------------------------
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "mmf2_module.h"
#include "module_vipnn.h"
#include "hal_cache.h"

#define FLASH_ADDR(x) (0x8000000+x)

#include "fwfs.h"
static int yamnet_model_size;
static void *yamnet_get_network_binary(void)
{
	if (sys_get_boot_sel() == 0) { // NOR
		// RAW
		uint8_t *model = (uint8_t *)FLASH_ADDR(nor_pfw_get_address("NN_MDL"));
		printf("Partition NN model addr = %x\n\r", model);

		if (memcmp(model, "RTL8735B", 8) == 0) {
			return (void *)&model[4096 + 0x80];
		} else {
			return (void *)&model[128];
		}

		// FW
		//return (void *)(FLASH_ADDR(NOR_MODEL_OFFSET) + 4096 + 0x80);
	} else if (sys_get_boot_sel() == 1) { // NAND
		void *fp = pfw_open("NN_MDL", 0);
		if (!fp) {
			printf("yamnet : cannot open filen\r");
			return NULL;
		}

		pfw_seek(fp, 0, SEEK_END);
		int size = pfw_tell(fp);
		pfw_seek(fp, 0, SEEK_SET);

		yamnet_model_size = size;
		uint8_t *nn_model = malloc(size + 128);
		if (!nn_model) {
			printf("yamnet : out of memory\n\r");
			pfw_close(fp);
			return NULL;
		}

		pfw_read(fp, nn_model, size);
		pfw_close(fp);

		return nn_model;
	} else {
		printf("Cannot use flash firmware in this mode\n\t");
		while (1);
	}
}

static void *yamnet_free_model(void *nn_model)
{
	if (sys_get_boot_sel() == 1) { // NAND
		if (nn_model) {
			free(nn_model);
		}
	}
}

static void *yamnet_get_network_filename(void)
{
	return (void *)"NN_MDL/yamnet_fp16.nb";	// fix name for NN model binary
}

static int yamnet_get_network_size(void)
{
	if (sys_get_boot_sel() == 0) { // NOR
		// RAW
		//uint32_t *model = (uint32_t *)FLASH_ADDR(NOR_MODEL_OFFSET);
		uint8_t *model = (uint8_t *)FLASH_ADDR(nor_pfw_get_address("NN_MDL"));
		if (memcmp(model, "RTL8735B", 8) == 0) {
			//uint32_t *imghdr = (uint32_t*)FLASH_ADDR(NOR_MODEL_OFFSET+4096);
			uint32_t *imghdr = (uint32_t *)&model[4096];
			return imghdr[0];
		} else {
			uint32_t size = *(uint32_t *)model;
			return size;
		}
	} else {
		return yamnet_model_size;
	}
}

#define MEAN_COUNT	  2	 // 4 times = ~ 1.9s
#define OVERLAP_RATIO 50 // 50 = 50%
static short *src_temp = NULL;
static int src_temp_cnt = 0;
static int tensor_in_cnt = 0;
static int overlap_cnt = -1;
static void *class_score = NULL;
static int class_update_idx = 0;

static int yamnet_preprocess(void *data_in, nn_data_param_t *data_param, void *tensor_in, nn_tensor_param_t *tensor_param)
{
	void **tensor = (void **)tensor_in;

	short *src = (short *)data_in;
	int src_sample_cnt = data_param->size_in_byte / (data_param->aud.bit_pre_sample / 8);

	//printf("in %d, level %d\n\r", src_sample_cnt, tensor_in_cnt);

	__fp16 *input   = (__fp16 *)tensor[0];
	int tensor_require_cnt = tensor_param->dim[0].size[0] * tensor_param->dim[0].size[1];

	if (overlap_cnt < 0) {
		overlap_cnt = tensor_require_cnt * OVERLAP_RATIO / 100;
		memset(input, 0, tensor_require_cnt * sizeof(__fp16));
	}

	if (tensor_in_cnt == 0) {
		// copy overlap count from tail to head
		memmove(input, &input[tensor_require_cnt - overlap_cnt], overlap_cnt * sizeof(__fp16));
		tensor_in_cnt = overlap_cnt;
	}

	if (!src_temp) {
		src_temp = (short *)malloc(src_sample_cnt * sizeof(short));
		if (!src_temp) {
			printf("yamnet : out of resource\n\r");
		}
	}

	int proc_size = (tensor_require_cnt - tensor_in_cnt) > src_sample_cnt ? src_sample_cnt : (tensor_require_cnt - tensor_in_cnt);

	if (src_temp_cnt) {
		//printf("src temp cnt %d\n\r", src_temp_cnt);
		for (int i = 0; i < src_temp_cnt; i++) {
			input[tensor_in_cnt + i] = (__fp16)src_temp[i] / 32768.0;
		}
		tensor_in_cnt += src_temp_cnt;
		src_temp_cnt = 0;
	}

	for (int i = 0; i < proc_size; i++) {
		input[tensor_in_cnt + i] = (__fp16)src[i] / 32768.0;
	}

	tensor_in_cnt += proc_size;

	if (tensor_in_cnt == tensor_require_cnt) {
		src_temp_cnt = src_sample_cnt - proc_size;
		if (src_temp && src_temp_cnt > 0) {
			memcpy(src_temp, &src[proc_size], src_temp_cnt * sizeof(short));
		}

		// may have alignment issue
		dcache_clean_by_addr((uint32_t *)input, tensor_require_cnt * sizeof(__fp16));
		tensor_in_cnt = 0;
		return 0;	// DO inference
	}


	return 1;	// SKIP,
}

typedef struct data_format_s {
	nn_tensor_format_t *format;
	nn_tensor_dim_t *dim;
} data_format_t;

typedef struct yamnet_res_s {
	float prob;
	int	  idx;
} yamnet_res_t;

#define CLASS_CNT 521
static yamnet_res_t res[CLASS_CNT];

static int prob_comparator(const void *pa, const void *pb)
{
	yamnet_res_t *a = (yamnet_res_t *)pa;
	yamnet_res_t *b = (yamnet_res_t *)pb;
	float diff = a->prob - b->prob;
	if (diff < 0) {
		return 1;
	} else if (diff > 0) {
		return -1;
	}
	return 0;
}

static void *yamnet_postprocess(void *tensor_out, nn_tensor_param_t *param)
{
	void **tensor = (void **)tensor_out;
	__fp16 *prob = (__fp16 *)tensor[0];
	int sort_cnt = 0;

	data_format_t fmt;

	fmt.format = &param->format[0];
	fmt.dim = &param->dim[0];
	int class_cnt = fmt.dim->size[0];

	if (class_score == NULL) {
		class_score = malloc(MEAN_COUNT * sizeof(__fp16) * class_cnt);
		if (!class_score) {
			printf("postprocess : out of resource\n\r");
			while (1);
		}
		memset(class_score, 0, MEAN_COUNT * sizeof(__fp16)*class_cnt);
	}

	if (class_cnt != CLASS_CNT) {
		printf("extra class in this model\n\r");
		class_cnt = CLASS_CNT;
	}

	memset(res, 0, sizeof(res));
#if 0
	__fp16(*cscore)[CLASS_CNT] = (__fp16(*)[CLASS_CNT])class_score;
	memcpy(cscore[class_update_idx], prob, sizeof(__fp16)*class_cnt);
	class_update_idx ++;
	if (class_update_idx >= MEAN_COUNT) {
		class_update_idx = 0;
	}

	for (int i = 0; i < class_cnt; i++) {
		//res[i].prob = (float)prob[i];
		res[i].prob = 0;
		for (int c = 0; c < MEAN_COUNT; c++) {
			res[i].prob += cscore[c][i];
		}
		res[i].prob = res[i].prob / MEAN_COUNT;
		res[i].idx = i;
	}
	sort_cnt = class_cnt;
#else
	sort_cnt = 0;
	for (int i = 0; i < class_cnt; i++) {
		if ((float)prob[i] > 0.5) {
			res[sort_cnt].prob = (float)prob[i];
			res[sort_cnt].idx = i;
			sort_cnt++;
		}
	}
	printf("total sort cnt %d\n\r", sort_cnt);
#endif

	if (sort_cnt > 0) {
		qsort(res, sort_cnt, sizeof(yamnet_res_t), prob_comparator);

		for (int i = 0; i < (sort_cnt > 5 ? 5 : sort_cnt); i++) {
			printf("class %d, prob %0.2f\n\r", res[i].idx, res[i].prob);
		}
	}
	return NULL;
}


nnmodel_t yamnet_fwfs = {
	.nb 			= yamnet_get_network_filename,
	.preprocess 	= yamnet_preprocess,
	.postprocess 	= yamnet_postprocess,
	.model_src 		= MODEL_SRC_FILE,

	.name = "YAMNET"
};
