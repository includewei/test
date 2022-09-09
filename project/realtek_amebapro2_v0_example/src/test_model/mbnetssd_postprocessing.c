#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

//#include "ssd_post_buffer.h"
//#include "nn_model_info.h"

//#include "nn_model_class.h"
#define NNMODEL_SM_POST_CLASS_NUM      21
#define NNMODEL_SM_POST_LOCA_NUM       1917
#define NNMODEL_SM_POST_LOCA_SCALE     0.124952
#define NNMODEL_SM_POST_LOCA_ZEROPOINT 169
#define NNMODEL_SM_POST_CONF_NUM       40257

#define MAX_CLASS_NUM 21
#define MAX_LOCA_NUM  1917

extern float mbnetssd_confidence_thresh;
extern float mbnetssd_nms_thresh;

static char *CLASSES_21OBJ[21] = {"__background__",
								  "aeroplane", "bicycle", "bird", "boat",
								  "bottle", "bus", "car", "cat", "chair",
								  "cow", "diningtable", "dog", "horse",
								  "motorbike", "person", "pottedplant",
								  "sheep", "sofa", "train", "tvmonitor"
								 };

typedef struct _BBoxRect {
	float xmin;
	float ymin;
	float xmax;
	float ymax;
	int label;
} BBoxRect;
extern const float nnmodel_sm_anchorbox[];

__attribute__((section(".ddr.data"))) float bboxes[MAX_LOCA_NUM * 4];
__attribute__((section(".ddr.data"))) BBoxRect class_bbox_rects[MAX_LOCA_NUM];
__attribute__((section(".ddr.data"))) float class_bbox_scores[MAX_LOCA_NUM];
__attribute__((section(".ddr.data"))) int picked[MAX_LOCA_NUM];
__attribute__((section(".ddr.data"))) BBoxRect *all_class_bbox_rects[MAX_CLASS_NUM];
__attribute__((section(".ddr.data"))) float *all_class_bbox_scores[MAX_CLASS_NUM];
__attribute__((section(".ddr.data"))) BBoxRect bbox_rects[MAX_LOCA_NUM];
__attribute__((section(".ddr.data"))) float bbox_scores[MAX_LOCA_NUM];
//Too big for RAM
__attribute__((section(".ddr.data"))) float all_class_bbox_scores_all[MAX_LOCA_NUM * MAX_CLASS_NUM];
__attribute__((section(".ddr.data"))) BBoxRect all_class_bbox_rects_all[MAX_LOCA_NUM * MAX_CLASS_NUM];


#define SWAP(T,a,b) { T c = a;a=b;b=c;}

#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))



static float intersection_area(BBoxRect *a,  BBoxRect *b)
{
	float inter_width = 0;
	float inter_height = 0;
	if (a->xmin > b->xmax || a->xmax < b->xmin || a->ymin > b->ymax || a->ymax < b->ymin) {
		// no intersection
		return 0.f;
	}

	inter_width = MIN(a->xmax, b->xmax) - MAX(a->xmin, b->xmin);
	inter_height = MIN(a->ymax, b->ymax) - MAX(a->ymin, b->ymin);

	return inter_width * inter_height;
}

static void qsort_descent_inplace_c(BBoxRect *datas, float *scores, int left, int right)
{
	int i = left;
	int j = right;
	float p = scores[(left + right) / 2];

	while (i <= j) {
		while (scores[i] > p) {
			i++;
		}

		while (scores[j] < p) {
			j--;
		}

		if (i <= j) {
			// swap
			SWAP(BBoxRect, datas[i], datas[j])
			SWAP(float, scores[i], scores[j])
			i++;
			j--;
		}
	}

	if (left < j) {
		qsort_descent_inplace_c(datas, scores, left, j);
	}

	if (i < right) {
		qsort_descent_inplace_c(datas, scores, i, right);
	}
}

static void nms_sorted_bboxes_c(BBoxRect *bboxes, int num_box, int *picked, int *num_picked, float nms_threshold)
{
	int m = 0;
	int n = num_box;
	int i, j;

	float areas[20];

	for (i = 0; i < n; i++) {
		BBoxRect r = bboxes[i];

		float width = r.xmax - r.xmin;
		float height = r.ymax - r.ymin;

		areas[i] = width * height;
	}

	for (i = 0; i < n; i++) {
		BBoxRect a = bboxes[i];

		int keep = 1;
		for (j = 0; j < m; j++) {
			BBoxRect b = bboxes[picked[j]];

			// intersection over union
			float inter_area = intersection_area(&a, &b);
			float union_area = areas[i] + areas[picked[j]] - inter_area;
//             float IoU = inter_area / union_area
			if (inter_area / union_area > nms_threshold) {
				keep = 0;
			}
		}

		if (keep) {
			picked[m] = i;
			m++;
		}

	}
	*num_picked = m;
}

static float vnn_Uint8toFp32(unsigned char val, int zeroPoint, float scale)
{
	float result = 0.0f;

	result = (val - (unsigned char)zeroPoint) * scale;
	return result;
}

static float Fp16toFp32(unsigned short in)
{
	unsigned int t1;
	unsigned int t2;
	unsigned int t3;
	float out;

	t1 = in & 0x7fff;                       // Non-sign bits
	t2 = in & 0x8000;                       // Sign bit
	t3 = in & 0x7c00;                       // Exponent

	t1 <<= 13;                              // Align mantissa on MSB
	t2 <<= 16;                              // Shift sign bit into position

	t1 += 0x38000000;                       // Adjust bias

	t1 = (t3 == 0 ? 0 : t1);                // Denormals-as-zero

	t1 |= t2;                               // Re-insert sign bit

	*((float *)&out) = t1;

	return out;
}


static void GetData_Uint8toFp32(float *output, unsigned int tensor_size, unsigned char *val, int zeroPoint, float scale)
{
	int iii;
	for (iii = 0; iii < tensor_size; iii++) {
		*output++ = vnn_Uint8toFp32(*val++, zeroPoint, scale);
	}
}

static void GetData_Fp16toFp32(float *output, unsigned int tensor_size, unsigned short *val)
{
	int iii;

	__fp16 *ptr_fp16 = (__fp16 *)val;
	for (iii = 0; iii < tensor_size; iii++) {
		//*output++ = Fp16toFp32(*val++);
		*output++ = (float)(*ptr_fp16++); //ARM __fp16 speedup
	}
}

static void nn_dtypetofloat(unsigned char *loc_nn, unsigned char *conf_nn, float *loc_fp, float *conf_fp)
{
	GetData_Uint8toFp32(&loc_fp[0],  NNMODEL_SM_POST_LOCA_NUM * 4,
						loc_nn, NNMODEL_SM_POST_LOCA_ZEROPOINT, NNMODEL_SM_POST_LOCA_SCALE);
	GetData_Fp16toFp32(&conf_fp[0], NNMODEL_SM_POST_CONF_NUM,
					   (unsigned short *)conf_nn);

}


static void nn_boxdecode_caffe_x(float *loc_buf, const float *prior_buf, float *decodeBB_buf)
{
	float *location_ptr = loc_buf;
	const float *priorbox_ptr = prior_buf;
	float *bboxes_ptr = decodeBB_buf;

	const float *loc = location_ptr;
	const float *pb = priorbox_ptr;
	float *bbox = bboxes_ptr;

	float pb_w = pb[2] - pb[0];
	float pb_h = pb[3] - pb[1];
	float pb_cx = (pb[0] + pb[2]) * 0.5f;
	float pb_cy = (pb[1] + pb[3]) * 0.5f;

	float bbox_cx = 0.1 * loc[0] * pb_w + pb_cx;
	float bbox_cy = 0.1 * loc[1] * pb_h + pb_cy;
	float bbox_w = exp(0.2 * loc[2]) * pb_w;
	float bbox_h = exp(0.2 * loc[3]) * pb_h;

	bbox[0] = bbox_cx - bbox_w * 0.5f;
	bbox[1] = bbox_cy - bbox_h * 0.5f;
	bbox[2] = bbox_cx + bbox_w * 0.5f;
	bbox[3] = bbox_cy + bbox_h * 0.5f;

}


static  void nn_boxdecode_tensorflow_x(float *loc_buf, const float *prior_buf, float *decodeBB_buf)
{
	float *location_ptr = loc_buf;
	const float *priorbox_ptr = prior_buf;
	float *bboxes_ptr = decodeBB_buf;

	const float *loc = location_ptr;
	const float *pb = priorbox_ptr;
	float *bbox = bboxes_ptr;

	float pb_w = pb[3];
	float pb_h = pb[2];
	float pb_cx = pb[1];
	float pb_cy = pb[0];

	float bbox_cy = 0.1 * loc[0] * pb_h + pb_cy;
	float bbox_cx = 0.1 * loc[1] * pb_w + pb_cx;
	float bbox_h = exp(0.2 * loc[2]) * pb_h;
	float bbox_w = exp(0.2 * loc[3]) * pb_w;

	bbox[0] = bbox_cx - bbox_w * 0.5f;
	bbox[1] = bbox_cy - bbox_h * 0.5f;
	bbox[2] = bbox_cx + bbox_w * 0.5f;
	bbox[3] = bbox_cy + bbox_h * 0.5f;

}

static  void nn_boxdecode_x(float *loc_buf, int idx, float *decodeBB_buf)
{
	nn_boxdecode_caffe_x(&loc_buf[idx], &nnmodel_sm_anchorbox[idx], decodeBB_buf);
}


//--------------------------------------------------------------------------------------

static int interest_class[] = {7, 15};

static int check_class(int curr)
{
	for (int i = 0; i < sizeof(interest_class) / sizeof(interest_class[0]); i++) {
		if (curr == interest_class[i]) {
			return 1;
		}
	}
	return 0;
}


static int ssd_post_process_x(float *conf_buf, float *loc_buf, int num_class, int num_prior, float *obj_buf, int *num_obj)
{
	float nms_threshold = mbnetssd_nms_thresh;
	int nms_top_k = 200;
	int keep_top_k = 200;
	float confidence_threshold = mbnetssd_confidence_thresh;
	int i;

	int bboxesWidth = 4;

	const float *confidence_ptr = conf_buf;

	int num_box = 0, num_picked = 0;
	int all_num_box[21] = {0};

	int total;
	int num_detected;

	for (i = 0; i < num_prior; i++) {
		class_bbox_scores[i] = 0;
		picked[i] = 0;
	}


	for (i = 0; i < num_class; i++) {
		all_class_bbox_rects[i]  =  &all_class_bbox_rects_all[i * num_prior];
		all_class_bbox_scores[i] =  &all_class_bbox_scores_all[i * num_prior];
	}

	// start from 1 to ignore background class
	for (i = 1; i < num_class; i++) {
		int j;
		num_box = 0;

		if (check_class(i) == 0)	{
			continue;
		}

		for (j = 0; j < num_prior; j++) {
			float score = confidence_ptr[j * num_class + i];

			if (score > confidence_threshold) {
				//const float *bbox = bboxes + bboxesWidth * j;
				float bbox[4];
				nn_boxdecode_x(loc_buf, j * 4, &class_bbox_rects[num_box]);
				class_bbox_rects[num_box].label = i;
				class_bbox_scores[num_box] = score;
				num_box++;
			}
		}
		if (num_box > 20) {
			num_box = 20;    //tpchua limit
		}

		if (i == 12) {
			i = i;
		}
		// sort inplace
		qsort_descent_inplace_c(class_bbox_rects, class_bbox_scores, 0, num_box - 1);

		// keep nms_top_k
		if (nms_top_k < num_box) {
			num_box = nms_top_k;
		}

		// apply nms
		num_picked = 0;
		nms_sorted_bboxes_c(class_bbox_rects, num_box, picked, &num_picked, nms_threshold);

		// select
		for (j = 0; j < num_picked; j++) {
			int z = picked[j];
			all_class_bbox_rects[i][all_num_box[i]]  = class_bbox_rects[z];
			all_class_bbox_scores[i][all_num_box[i]] = class_bbox_scores[z];
			all_num_box[i]++;
		}
	}

	// gather all class

	for (i = 0; i < num_prior; i++) {
		bbox_scores[i] = 0;
	}

	total = 0;
	for (i = 0; i < num_class; i++) {
		BBoxRect *class_bbox_rects = all_class_bbox_rects[i];
		float *class_bbox_scores = all_class_bbox_scores[i];
		int k;

		if (check_class(i) == 0)	{
			continue;
		}
		for (k = 0; k < all_num_box[i]; k++) {
			bbox_rects[total] = class_bbox_rects[k];
			bbox_scores[total] = class_bbox_scores[k];
			total++;
		}
	}
	if (total > 20) {
		total = 20;    //tpchua limit
	}

	// global sort inplace
	qsort_descent_inplace_c(bbox_rects, bbox_scores, 0, total);
	// keep_top_k
	if (keep_top_k < total) {
		total = keep_top_k;
	}

	// fill result
	num_detected = total;

	for (i = 0; i < num_detected; i++) {
		BBoxRect r = bbox_rects[i];
		float score = bbox_scores[i];

		obj_buf[6 * i + 0] = r.label;
		obj_buf[6 * i + 1] = score;
		obj_buf[6 * i + 2] = r.xmin;
		obj_buf[6 * i + 3] = r.ymin;
		obj_buf[6 * i + 4] = r.xmax;
		obj_buf[6 * i + 5] = r.ymax;

	}

	*num_obj = num_detected;
	return 0;
}

static float  loc_val[MAX_LOCA_NUM * 4];
static float  conf_val[MAX_LOCA_NUM * MAX_CLASS_NUM];
void mbnetssd_postprocessing(unsigned char *loc_nn, unsigned char *conf_nn, float *detect_result, int *detect_obj_num)
{
	int num_prior = 0;
	int num_class = 0;

	num_class = NNMODEL_SM_POST_CLASS_NUM;
	num_prior = NNMODEL_SM_POST_LOCA_NUM;

	nn_dtypetofloat(loc_nn, conf_nn, &loc_val[0], &conf_val[0]);
	ssd_post_process_x(conf_val, loc_val, num_class, num_prior, detect_result, detect_obj_num);

}

void mbnetssd_show_results(float *detect_result, int *detect_obj_num)
{

	int iii;
	char **classes_ptr;
	int print_num = *detect_obj_num;

	classes_ptr = CLASSES_21OBJ;


	dbg_printf("------object num = %d -------\r\n", print_num);
	for (iii = 0; iii < print_num; iii++) {
		int cls = (int)detect_result[6 * iii + 0];
		if ((cls > 0) && (cls < 21)) {
			// dbg_printf("Class %s label: %2.f, score: %f, location: %f,%f,%f,%f\n",
			// classes_ptr[cls],
			// detect_result[6 * iii + 0],
			// detect_result[6 * iii + 1],
			// detect_result[6 * iii + 2],
			// detect_result[6 * iii + 3],
			// detect_result[6 * iii + 4],
			// detect_result[6 * iii + 5]);
			dbg_printf("%s label\r\n", classes_ptr[cls]);
		}

	}
	dbg_printf("------------------------\r\n");
}

