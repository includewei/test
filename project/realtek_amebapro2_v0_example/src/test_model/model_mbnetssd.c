//-----------------------------------------------------------------
// mobilenet ssd model
//-----------------------------------------------------------------
#include <stdio.h>
#include "nn_api.h"
#include "mmf2_module.h"
#include "module_vipnn.h"
#include "hal_cache.h"

#include "ssd_post_process.h"

#define USE_MOBILENET_SSD_ARRAY_MODEL   0
#if USE_MOBILENET_SSD_ARRAY_MODEL
#include "mobilenet_ssd_uint8.h"
#else
//#define NOR_MODEL_OFFSET 0x710000
#define FLASH_ADDR(x) (0x8000000+x)
#endif

#include "fwfs.h"
static int mbnetssd_model_size;
void *mbnetssd_get_network_binary(void)
{
#if USE_MOBILENET_SSD_ARRAY_MODEL
	return (void *)mobilenet_ssd_uint8;
#else
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
			printf("mobilenet : cannot open filen\r");
			return NULL;
		}

		pfw_seek(fp, 0, SEEK_END);
		int size = pfw_tell(fp);
		pfw_seek(fp, 0, SEEK_SET);

		mbnetssd_model_size = size;
		uint8_t *nn_model = malloc(size + 128);
		if (!nn_model) {
			printf("mobilenet : out of memory\n\r");
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
#endif
}

void *mbnetssd_free_model(void *nn_model)
{
	if (sys_get_boot_sel() == 1) { // NAND
		if (nn_model) {
			free(nn_model);
		}
	}
}

void *mbnetssd_get_network_filename(void)
{
	return (void *)"NN_MDL";	// fix name for NN model binary
}

int mbnetssd_get_network_size(void)
{
#if USE_MOBILENET_SSD_ARRAY_MODEL
	return mobilenet_ssd_uint8_size;
#else
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
		// mbnetssd_get_network_binary will be executed before mbnetssd_get_network_size
		// mbnetssd_model_size will be ready
		return mbnetssd_model_size;
	}
#endif
}

int mbnetssd_preprocess(void *data_in, nn_data_param_t *data_param, void *tensor_in, nn_tensor_param_t *tensor_param)
{
	void **tensor = (void **)tensor_in;
	struct cvRect_S roi = {
		.xmin = data_param->img.roi.xmin,
		.ymin = data_param->img.roi.ymin,
		.xmax = data_param->img.roi.xmax,
		.ymax = data_param->img.roi.ymax,
	};

	cvImage img_in, img_out;

	img_in.width  = data_param->img.width;
	img_in.height = data_param->img.height;
	img_out.width  = tensor_param->dim[0].size[0];
	img_out.height = tensor_param->dim[0].size[1];

	img_in.data   = (unsigned char *)data_in;
	img_out.data   = (unsigned char *)tensor[0];

	//printf("preproc w %d h %d ow %d oh %d\n\r", img_in.width, img_in.height, img_out.width, img_out.height);
	//printf("preproc %x, %x\n\r", img_in.data, img_out.data);

	// resize src ROI area to dst
	if (img_in.width == img_out.width && img_in.height == img_out.height) {
		memcpy(img_out.data, img_in.data, img_out.width * img_out.height * 3);
	} else {
		nn_resize_rgb888(&img_in, &img_out, roi, data_param->img.rgb);
	}
	//printf("preprocess finish\n\r");
	dcache_clean_by_addr((uint32_t *)img_out.data, img_out.width * img_out.height * 3);
	//dcache_clean();		// if don't care other hardware dma, you can use this. ugly method, should clean dst address with its data size
	return 0;
}

static vipnn_res_t ssd_res;
void *mbnetssd_postprocess(void *tensor_out, nn_tensor_param_t *param)
{
	void **tensor = (void **)tensor_out;

	vipnn_res_t *res = &ssd_res;
	//float result[6 * 21];
	//int obj_num;

	mbnetssd_postprocessing((unsigned char *)tensor[0],  //loc
							(unsigned char *)tensor[1], 	//scores
							res->od_res.result, &res->od_res.obj_num);

	//printf("obj num %d \n\r", res->od_res.obj_num);
	nn_display_results(0, res->od_res.result, &res->od_res.obj_num);

	return (void *)res;
}

float mbnetssd_confidence_thresh = 0.75;  // default
float mbnetssd_nms_thresh = 0.3;     // default

void set_mbnetssd_confidence_thresh(void *confidence_thresh)
{
	mbnetssd_confidence_thresh = *(float *)confidence_thresh;
	printf("set mbnetssd confidence thresh to %f\n\r", *(float *)confidence_thresh);
}

void set_mbnetssd_nms_thresh(void *nms_thresh)
{
	mbnetssd_nms_thresh = *(float *)nms_thresh;
	printf("set mbnetssd NMS thresh to %f\n\r", *(float *)nms_thresh);
}

nnmodel_t mbnetssd = {
	.nb 		= mbnetssd_get_network_binary,
	.nb_size 	= mbnetssd_get_network_size,
	.preprocess 	= mbnetssd_preprocess,
	.postprocess 	= mbnetssd_postprocess,
	.set_confidence_thresh   = set_mbnetssd_confidence_thresh,
	.set_nms_thresh     = set_mbnetssd_nms_thresh,

	.name = "MBNETSSD"
};

nnmodel_t mbnetssd_fwfs = {
	.nb 			= mbnetssd_get_network_filename,
	.preprocess 	= mbnetssd_preprocess,
	.postprocess 	= mbnetssd_postprocess,
	.model_src 		= MODEL_SRC_FILE,
	.set_confidence_thresh   = set_mbnetssd_confidence_thresh,
	.set_nms_thresh     = set_mbnetssd_nms_thresh,

	.name = "MBNETSSD"
};
