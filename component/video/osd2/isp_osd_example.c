#include <platform_stdlib.h>
#include <platform_opts.h>
#include <build_info.h>
#include "log_service.h"
#include "atcmd_isp.h"
#include "main.h"
#include "flash_api.h"
#include "hal_osd_util.h"
#include "osd_custom.h"
#include "osd_pict_custom.h"
#include "osd_api.h"
#include "isp_osd_example.h"


const struct result_frame results[] = {
	{
		2, {
			{0, 2, 88, 0, 447, 821, 812},
			{1, 0, 58, 894, 150, 1253, 785}
		}
	},
	{
		2, {
			{0, 2, 88, 0, 447, 821, 812},
			{1, 0, 52, 867, 156, 1260, 790}
		}
	},
	{
		2, {
			{0, 2, 88, 0, 447, 821, 812},
			{1, 0, 58, 870, 152, 1263, 786}
		}
	},
	{
		2, {
			{0, 2, 86, 0, 447, 821, 812},
			{1, 0, 61, 870, 150, 1263, 785}
		}
	},
	{
		2, {
			{0, 2, 86, 0, 447, 821, 812},
			{1, 0, 58, 867, 147, 1260, 781}
		}
	},
	{
		1, {
			{0, 2, 87, 0, 449, 821, 814}
		}
	},
	{
		1, {
			{0, 2, 89, 0, 449, 821, 814}
		}
	},
	{
		2, {
			{0, 2, 87, 0, 447, 821, 812},
			{1, 0, 50, 867, 148, 1260, 782}
		}
	},
	{
		2, {
			{0, 2, 87, 0, 449, 821, 814},
			{1, 0, 50, 889, 214, 1248, 848}
		}
	},
	{
		2, {
			{0, 2, 88, 0, 447, 821, 812},
			{1, 0, 58, 872, 148, 1265, 782}
		}
	},
	{
		2, {
			{0, 2, 87, 0, 449, 821, 814},
			{1, 0, 61, 865, 171, 1258, 749}
		}
	},
	{
		2, {
			{0, 2, 87, 0, 447, 821, 812},
			{1, 0, 55, 870, 152, 1263, 786}
		}
	},
	{
		2, {
			{0, 2, 89, 0, 449, 821, 814},
			{1, 0, 72, 874, 217, 1267, 851}
		}
	},
	{
		2, {
			{0, 2, 89, 0, 449, 821, 814},
			{1, 0, 69, 877, 217, 1270, 851}
		}
	},
	{
		2, {
			{0, 2, 89, 0, 447, 821, 812},
			{1, 0, 61, 879, 219, 1273, 853}
		}
	},
	{
		1, {
			{0, 2, 88, 0, 447, 821, 812}
		}
	},
	{
		2, {
			{0, 2, 89, 0, 447, 821, 812},
			{1, 0, 61, 891, 221, 1284, 855}
		}
	},
	{
		2, {
			{0, 2, 90, 0, 447, 821, 812},
			{1, 0, 70, 898, 224, 1291, 858}
		}
	},
	{
		2, {
			{0, 2, 89, 0, 446, 821, 811},
			{1, 0, 74, 901, 221, 1294, 855}
		}
	},
	{
		2, {
			{0, 2, 90, 0, 446, 821, 811},
			{1, 0, 74, 922, 219, 1281, 853}
		}
	},
	{
		2, {
			{0, 2, 90, 0, 447, 821, 812},
			{1, 0, 80, 908, 222, 1301, 857}
		}
	},
	{
		2, {
			{0, 2, 91, 0, 447, 821, 812},
			{1, 0, 78, 911, 217, 1304, 851}
		}
	},
	{
		2, {
			{0, 2, 89, 0, 447, 821, 812},
			{1, 0, 74, 913, 219, 1307, 853}
		}
	},
	{
		2, {
			{0, 2, 89, 0, 447, 821, 812},
			{1, 0, 77, 916, 217, 1310, 851}
		}
	},
	{
		2, {
			{0, 2, 90, 0, 446, 821, 811},
			{1, 0, 75, 921, 214, 1314, 848}
		}
	},
	{
		2, {
			{0, 2, 90, 0, 446, 821, 811},
			{1, 0, 81, 921, 217, 1314, 851}
		}
	},
	{
		2, {
			{0, 2, 89, 0, 446, 821, 811},
			{1, 0, 78, 921, 221, 1314, 855}
		}
	},
	{
		2, {
			{0, 2, 90, 0, 447, 821, 812},
			{1, 0, 81, 923, 219, 1317, 853}
		}
	},
	{
		2, {
			{0, 2, 89, 0, 447, 821, 812},
			{1, 0, 78, 921, 217, 1314, 851}
		}
	},
	{
		2, {
			{0, 2, 89, 0, 447, 821, 812},
			{1, 0, 74, 938, 219, 1297, 853}
		}
	},
	{
		2, {
			{0, 2, 91, 0, 446, 821, 811},
			{1, 0, 77, 938, 221, 1297, 855}
		}
	},
	{
		2, {
			{0, 2, 89, 0, 446, 821, 811},
			{1, 0, 77, 938, 217, 1297, 851}
		}
	},
	{
		2, {
			{0, 2, 90, 0, 447, 821, 812},
			{1, 0, 80, 913, 219, 1307, 853}
		}
	},
	{
		2, {
			{0, 2, 90, 0, 447, 821, 812},
			{1, 0, 75, 916, 217, 1310, 851}
		}
	},
	{
		2, {
			{0, 2, 89, 0, 447, 821, 812},
			{1, 0, 78, 938, 219, 1297, 853}
		}
	},
	{
		2, {
			{0, 2, 89, 0, 447, 821, 812},
			{1, 0, 78, 938, 221, 1297, 855}
		}
	},
	{
		2, {
			{0, 2, 89, 0, 447, 821, 812},
			{1, 0, 78, 938, 217, 1297, 851}
		}
	},
	{
		2, {
			{0, 2, 89, 0, 447, 821, 812},
			{1, 0, 78, 938, 219, 1297, 853}
		}
	},
	{
		2, {
			{0, 2, 90, 0, 447, 821, 812},
			{1, 0, 80, 938, 219, 1297, 853}
		}
	},
	{
		2, {
			{0, 2, 90, 0, 447, 821, 812},
			{1, 0, 77, 938, 219, 1297, 853}
		}
	},
	{
		2, {
			{0, 2, 90, 0, 447, 821, 812},
			{1, 0, 75, 938, 215, 1297, 849}
		}
	},
	{
		2, {
			{0, 2, 90, 0, 447, 821, 812},
			{1, 0, 77, 921, 219, 1314, 853}
		}
	},
	{
		2, {
			{0, 2, 90, 0, 447, 821, 812},
			{1, 0, 78, 938, 217, 1297, 851}
		}
	},
	{
		1, {
			{0, 0, 91, 1122, 441, 1203, 603}
		}
	},
	{
		1, {
			{0, 0, 81, 1131, 435, 1204, 597}
		}
	},
	{
		1, {
			{0, 0, 77, 1152, 431, 1232, 609}
		}
	},
	{
		1, {
			{0, 0, 77, 1156, 433, 1236, 611}
		}
	},
	{
		1, {
			{0, 0, 89, 1151, 433, 1239, 611}
		}
	},
	{
		1, {
			{0, 0, 77, 1163, 448, 1251, 643}
		}
	},
	{
		1, {
			{0, 0, 87, 1171, 451, 1259, 646}
		}
	},
	{
		1, {
			{0, 0, 81, 1175, 450, 1263, 645}
		}
	},
	{
		2, {
			{0, 0, 57, 1128, 328, 1324, 773},
			{1, 0, 83, 1183, 460, 1279, 638}
		}
	},
	{
		1, {
			{0, 0, 52, 1134, 328, 1313, 773}
		}
	},
	{
		1, {
			{0, 0, 83, 1191, 454, 1287, 649}
		}
	},
	{
		1, {
			{0, 0, 63, 1141, 330, 1337, 774}
		}
	},
	{
		1, {
			{0, 0, 61, 1167, 335, 1363, 779}
		}
	},
	{
		2, {
			{0, 0, 58, 1176, 351, 1373, 756},
			{1, 0, 71, 895, 395, 937, 488}
		}
	},
	{
		2, {
			{0, 0, 50, 1188, 335, 1384, 779},
			{1, 0, 64, 895, 393, 930, 486}
		}
	},
	{
		1, {
			{0, 0, 52, 1148, 277, 1420, 856}
		}
	},
	{
		1, {
			{0, 0, 75, 1200, 305, 1406, 884}
		}
	},
	{
		2, {
			{0, 0, 75, 1193, 313, 1419, 891},
			{1, 0, 76, 837, 394, 883, 486}
		}
	},
	{
		2, {
			{0, 0, 77, 1197, 297, 1423, 931},
			{1, 0, 74, 832, 395, 878, 488}
		}
	},
	{
		2, {
			{0, 0, 83, 1195, 297, 1422, 931},
			{1, 0, 63, 827, 394, 873, 486}
		}
	},
	{
		2, {
			{0, 0, 77, 1193, 294, 1419, 929},
			{1, 0, 63, 819, 396, 869, 481}
		}
	},
	{
		1, {
			{0, 0, 78, 1184, 294, 1433, 929}
		}
	},
	{
		2, {
			{0, 0, 70, 1220, 306, 1468, 940},
			{1, 0, 69, 817, 391, 868, 475}
		}
	},
	{
		2, {
			{0, 0, 74, 1170, 284, 1442, 979},
			{1, 0, 76, 821, 391, 863, 475}
		}
	},
	{
		2, {
			{0, 0, 57, 1210, 289, 1483, 984},
			{1, 0, 67, 818, 393, 864, 478}
		}
	},
	{
		1, {
			{0, 0, 77, 1205, 297, 1503, 992}
		}
	},
	{
		2, {
			{0, 0, 72, 1188, 301, 1515, 996},
			{1, 0, 50, 818, 391, 860, 484}
		}
	},
	{
		1, {
			{0, 0, 81, 1193, 270, 1520, 1032}
		}
	},
	{
		2, {
			{0, 0, 50, 1161, 270, 1459, 1032},
			{1, 0, 50, 802, 391, 852, 484}
		}
	},
	{
		2, {
			{0, 0, 70, 1157, 316, 1455, 1078},
			{1, 0, 58, 800, 392, 851, 494}
		}
	},
	{
		2, {
			{0, 0, 86, 1120, 317, 1447, 1079},
			{1, 0, 67, 794, 445, 861, 593}
		}
	},
	{
		2, {
			{0, 0, 86, 1081, 325, 1408, 1079},
			{1, 0, 83, 795, 445, 876, 593}
		}
	},
	{
		2, {
			{0, 0, 87, 1055, 274, 1382, 1079},
			{1, 0, 79, 811, 451, 884, 585}
		}
	},
	{
		2, {
			{0, 0, 75, 959, 272, 1352, 1079},
			{1, 0, 51, 830, 402, 896, 486}
		}
	},
	{
		1, {
			{0, 0, 85, 972, 272, 1330, 1079}
		}
	},
	{
		2, {
			{0, 0, 87, 936, 308, 1329, 1071},
			{1, 0, 63, 842, 401, 897, 485}
		}
	},
	{
		2, {
			{0, 0, 84, 943, 312, 1302, 1074},
			{1, 0, 52, 839, 401, 889, 485}
		}
	},
	{
		1, {
			{0, 0, 77, 938, 307, 1265, 1069}
		}
	},
	{
		1, {
			{0, 0, 62, 931, 351, 1258, 1046}
		}
	},
	{
		2, {
			{0, 0, 61, 845, 451, 896, 562},
			{1, 0, 93, 1025, 431, 1113, 609}
		}
	},
	{
		2, {
			{0, 0, 81, 841, 431, 891, 532},
			{1, 0, 83, 1009, 436, 1097, 598}
		}
	},
	{
		2, {
			{0, 0, 77, 841, 427, 891, 529},
			{1, 0, 85, 1012, 439, 1092, 601}
		}
	},
	{
		2, {
			{0, 0, 76, 837, 427, 887, 529},
			{1, 0, 67, 972, 434, 1060, 582}
		}
	},
	{
		2, {
			{0, 0, 80, 833, 429, 884, 531},
			{1, 0, 79, 967, 433, 1055, 581}
		}
	},
	{
		2, {
			{0, 0, 86, 829, 426, 879, 528},
			{1, 0, 76, 959, 434, 1047, 582}
		}
	},
	{
		2, {
			{0, 0, 85, 829, 426, 879, 527},
			{1, 0, 81, 954, 441, 1042, 575}
		}
	},
	{
		2, {
			{0, 0, 90, 829, 426, 875, 528},
			{1, 0, 83, 951, 440, 1032, 574}
		}
	},
	{
		2, {
			{0, 0, 90, 829, 421, 871, 523},
			{1, 0, 87, 952, 424, 1019, 547}
		}
	},
	{
		2, {
			{0, 0, 88, 828, 419, 870, 521},
			{1, 0, 79, 950, 426, 1017, 548}
		}
	},
	{
		3, {
			{0, 0, 52, 137, 699, 396, 1069},
			{1, 0, 86, 841, 393, 883, 495},
			{2, 0, 77, 961, 428, 1012, 539}
		}
	},
	{
		3, {
			{0, 0, 52, 125, 716, 409, 1053},
			{1, 0, 83, 837, 394, 876, 495},
			{2, 0, 86, 959, 428, 1005, 530}
		}
	},
	{
		2, {
			{0, 0, 65, 836, 402, 877, 494},
			{1, 0, 89, 954, 427, 1004, 529}
		}
	},
	{
		2, {
			{0, 0, 50, 830, 401, 880, 494},
			{1, 0, 93, 958, 426, 1004, 527}
		}
	},
	{
		2, {
			{0, 0, 77, 830, 395, 868, 488},
			{1, 0, 91, 950, 424, 996, 525}
		}
	},
	{
		2, {
			{0, 0, 67, 829, 394, 871, 486},
			{1, 0, 80, 952, 425, 994, 526}
		}
	},
	{
		2, {
			{0, 0, 76, 830, 395, 868, 488},
			{1, 0, 83, 952, 428, 994, 521}
		}
	},
	{
		3, {
			{0, 0, 50, 198, 761, 340, 1024},
			{1, 0, 65, 828, 398, 870, 482},
			{2, 0, 50, 949, 425, 995, 526}
		}
	},
	{
		1, {
			{0, 0, 55, 825, 399, 867, 483}
		}
	},
	{
		1, {
			{0, 0, 67, 822, 399, 856, 483}
		}
	},
	{
		1, {
			{0, 0, 65, 820, 401, 855, 485}
		}
	},
	{
		2, {
			{0, 0, 77, 825, 390, 847, 474},
			{1, 0, 61, 915, 420, 961, 522}
		}
	},
	{
		3, {
			{0, 0, 50, 205, 760, 334, 1023},
			{1, 0, 58, 820, 402, 858, 486},
			{2, 0, 71, 909, 423, 951, 515}
		}
	},
};


static xQueueHandle nn_data_queue = NULL;

static osd_text_info_st s_txt_info_time;
static osd_text_info_st s_txt_info_date;
static osd_text_info_st s_txt_info_string;
static char teststring[] = "RTK-AmebaPro2";

static int tag_num = 80;
static char **tag_name = NULL;
static const char *cococlass[80] = { "person",    "bicycle",    "car",    "motorbike",    "aeroplane",    "bus",    "train",    "truck",    "boat",    "traffic light",    "fire hydrant",    "stop sign",    "parking meter",    "bench",    "bird",    "cat",    "dog",    "horse",    "sheep",    "cow",    "elephant",    "bear",    "zebra",    "giraffe",    "backpack",    "umbrella",    "handbag",    "tie",    "suitcase",    "frisbee",    "skis",    "snowboard",    "sports ball",    "kite",    "baseball bat",    "baseball glove",    "skateboard",    "surfboard",    "tennis racket",    "bottle",    "wine glass",    "cup",    "fork",    "knife",    "spoon",    "bowl",    "banana",    "apple",    "sandwich",    "orange",    "broccoli",    "carrot",    "hot dog",    "pizza",    "donut",    "cake",    "chair",    "sofa",    "pottedplant",    "bed",    "diningtable",    "toilet",    "tvmonitor",    "laptop",    "mouse",    "remote",    "keyboard",    "cell phone",    "microwave",    "oven",    "toaster",    "sink",    "refrigerator",    "book",    "clock",    "vase",    "scissors",    "teddy bear",    "hair drier",    "toothbrush" };
static int osd_ready2draw = 0;

extern void osd_hide_bitmap(int ch, rt_osd2_info_st *posd2_pic);

int osd_get_status()
{
	return osd_ready2draw;
}

int osd_update_rect_result(struct result_frame nn_results)
{
	if (nn_data_queue) {
		xQueueSend(nn_data_queue, (void *) &nn_results, 0);
		return 1;
	}
	return 0;
}

int osd_set_tag(int tag_number, char **tag)
{
	if (nn_data_queue) {
		printf("Please initial tag name before running example_isp_osd\r\n");
		return 0;
	}

	tag_num = tag_number;
	tag_name = tag;
	return 1;
}

#define DRAW_NN_OSD 1

static int nn_rect_ch = 0;
static int nn_rect_txt_w = 16;
static int nn_rect_txt_h = 32;
void osd_rectangle_task(void *arg)
{
	struct result_frame g_results;
	BITMAP_S bmp_tag[tag_num];
	enum rts_osd2_blk_fmt disp_format = RTS_OSD2_BLK_FMT_RGBA2222;
	osd_pict_st osd2_pic[RECT_NUM];
	int nn_rect_ch_l = nn_rect_ch;
	int nn_rect_txt_w_l = nn_rect_txt_w;
	int nn_rect_txt_h_l = nn_rect_txt_h;
	int rect_w[RECT_NUM] = {0};
	int rect_h[RECT_NUM] = {0};
	int pic_w[RECT_NUM] = {0};
	int pic_h[RECT_NUM] = {0};
	int *pd1[RECT_NUM] = {0};
	int *pd2[RECT_NUM] = {0};
	int **pd[2] = {pd1, pd2};
	struct osd_rect_info_st rect_info;
	int tag_max_num = 0;
	int count = 999;
	struct result_frame *presults;
#if defined(configENABLE_TRUSTZONE) && (configENABLE_TRUSTZONE == 1)
	rtw_create_secure_context(2048);
#endif

	rts_osd_init(nn_rect_ch_l, nn_rect_txt_w_l, nn_rect_txt_h_l, (int)(8.0f * 3600));

	for (int i = 0; i < RECT_NUM; i++) {
		osd2_pic[i].chn_id = nn_rect_ch_l;
		osd2_pic[i].osd2.blk_idx = i;
		osd2_pic[i].osd2.blk_fmt = disp_format;
		rts_osd_set_info(rts_osd2_type_pict, &osd2_pic[i]);
	}

	if (tag_name == NULL) {
		printf("Default use coco class label\r\n");
		tag_num = 80;
		tag_name = cococlass;
	}
	for (int i = 0; i < tag_num; i++) {
		if (strlen(tag_name[i]) > tag_max_num) {
			tag_max_num = strlen(tag_name[i]);
		}
		bmp_tag[i].pData = NULL;
	}

	for (int i = 0; i < tag_num; i++) {
		//printf("tag malloc size: %d.\r\n", ((tag_max_num * txt_w + 7) / 8) * 8 * txt_h);
		bmp_tag[i].pData = malloc(((tag_max_num * nn_rect_txt_w_l + 7) / 8) * 8 * nn_rect_txt_h_l);
		if (bmp_tag[i].pData == NULL) {
			for (int j = 0; j < tag_num; j++) {
				if (bmp_tag[j].pData) {
					free(bmp_tag[j].pData);
				}
			}
			return;
		}
		osd_text2bmp(tag_name[i], &bmp_tag[i], nn_rect_ch_l, nn_rect_ch_l);
	}
//	printf("osd_rectangle_task.\r\n");
//	vTaskDelete(NULL);
//	return;

	rect_info.type = 0;
	rect_info.format = disp_format;
	rect_info.line_width = 3;
	rect_info.rect_clr_r = 0;
	rect_info.rect_clr_g = 255;
	rect_info.rect_clr_b = 255;
	rect_info.txt_clr_r = 255;
	rect_info.txt_clr_g = 255;
	rect_info.txt_clr_b = 0;

	presults = (struct result_frame *)results;
	osd_ready2draw = 1;
	while (1) {
#if DRAW_NN_OSD
		if (nn_data_queue && xQueueReceive(nn_data_queue, &g_results, portMAX_DELAY) == pdTRUE) {
			unsigned long tick1 = xTaskGetTickCount();
			osd_ready2draw = 0;
			int *pd_pp = pd[count % 2];

			count--;

			if (count == 1) {
				count = 999;
			}

#else
		if (1) {
			int *pd_pp = pd[count % 2];

			count--;

			if (count % 100 == 0) {
				presults = (struct result_frame *)results;
			}

			if (count == 1) {
				count = 999;
				presults = (struct result_frame *)results;
			}
			//printf("count: %d ch: %d.\r\n", count, nn_rect_ch_l);
			g_results = *presults;
#endif

			for (int i = 0; i < g_results.num; i++) {
				int align_w;
				g_results.obj[i].left = ((g_results.obj[i].left) / 8) * 8;
				g_results.obj[i].right = ((g_results.obj[i].right) / 8) * 8;
				g_results.obj[i].top = ((g_results.obj[i].top) / 8) * 8 - nn_rect_txt_h_l;
				if (g_results.obj[i].top < 0) {
					g_results.obj[i].top = 0;
				}
				g_results.obj[i].bottom = ((g_results.obj[i].bottom) / 8) * 8;
				pic_w[i] = g_results.obj[i].right - g_results.obj[i].left;
				pic_h[i] = g_results.obj[i].bottom - g_results.obj[i].top;
				//printf("osd num  = %d, class = %d, (l,t,r,b) = (%d, %d, %d, %d)\r\n", i, g_results.obj[i].class, g_results.obj[i].left, g_results.obj[i].top,
				//	   g_results.obj[i].right, g_results.obj[i].bottom);

				if (tag_max_num * nn_rect_txt_w_l > pic_w[i]) {
					pic_w[i] = tag_max_num * nn_rect_txt_w_l;
				}
				align_w = ((pic_w[i] + 7) / 8) * 8;
				rect_w[i] = g_results.obj[i].right - g_results.obj[i].left;
				rect_h[i] = pic_h[i] - nn_rect_txt_h_l;

				if (pd_pp[i]) {
					free(pd_pp[i]);
				}
				pd_pp[i] = malloc(align_w * (pic_h[i] + 8));
				if (pd_pp[i]) {
					rect_info.rect_w = rect_w[i];
					rect_info.rect_h = rect_h[i];
					rts_osd_rect_gen_with_txt(&bmp_tag[g_results.obj[i].class], pd_pp[i], pic_w[i], pic_h[i], rect_info, 1);

					osd2_pic[i].osd2.len = align_w * pic_h[i];
					osd2_pic[i].osd2.start_x = g_results.obj[i].left;
					osd2_pic[i].osd2.start_y = g_results.obj[i].top;
					osd2_pic[i].osd2.end_x = osd2_pic[i].osd2.start_x + pic_w[i];
					osd2_pic[i].osd2.end_y = osd2_pic[i].osd2.start_y + pic_h[i];
				}
			}

			for (int i = g_results.num; i < RECT_NUM; i++) {
				if (pd_pp[i]) {
					free(pd_pp[i]);
				}
				pd_pp[i] = malloc(8 * 8);
				if (pd_pp[i]) {
					osd2_pic[i].osd2.len = 8 * 8;
					osd2_pic[i].osd2.start_x = 0;
					osd2_pic[i].osd2.start_y = 0;
					osd2_pic[i].osd2.end_x = 8;
					osd2_pic[i].osd2.end_y = 8;
					memset(pd_pp[i], 0, osd2_pic[i].osd2.len);
				}
			}

			for (int i = 0; i < RECT_NUM; i++) {
				if (pd_pp[i]) {
					osd2_pic[i].osd2.buf = pd_pp[i];
				}
			}

			for (int i = 0; i < RECT_NUM; i++) {
				if (i < RECT_NUM - 1) {
					rts_osd_bitmap_update(nn_rect_ch_l, &osd2_pic[i].osd2, 0);
				} else {
					rts_osd_bitmap_update(nn_rect_ch_l, &osd2_pic[i].osd2, 1);
				}
			}

#if !DRAW_NN_OSD
			presults++;
#endif

			//printf("\r\nOSD draw after %dms.\n", (xTaskGetTickCount() - tick1));
		}
#if DRAW_NN_OSD
		osd_ready2draw = 1;
		vTaskDelay(10);
#else
		vTaskDelay(50);
#endif
	}
	vTaskDelay(200);
	for (int i = 0; i < RECT_NUM; i++) {
		osd_hide_bitmap(0, &osd2_pic[i].osd2);
	}
	osd_deinit(nn_rect_ch_l);
	for (int i = 0; i < tag_num; i++) {
		if (bmp_tag[i].pData) {
			free(bmp_tag[i].pData);
		}
		bmp_tag[i].pData = NULL;
	}
	for (int i = 0; i < RECT_NUM; i++) {
		if (pd1[i]) {
			free(pd1[i]);
		}
		pd1[i] = NULL;
		if (pd2[i]) {
			free(pd2[i]);
		}
		pd2[i] = NULL;
	}
	vTaskDelete(NULL);
}
extern void rts_osd_task(void *arg);
osd_pict_st posd2_pic_0, posd2_pic_1, posd2_pic_2;
void example_isp_osd(int idx, int ch_id, int txt_w, int txt_h)
{
	int ch = ch_id;
	rt_font_st font;
	printf("Text/Logo OSD Test\r\n");

	if (idx == 0) {

		printf("[osd] Heap available:%d\r\n", xPortGetFreeHeapSize());
		rts_osd_init(ch_id, txt_w, txt_h, (int)(8.0f * 3600));

		font.bg_enable		= OSD_TEXT_FONT_BG_ENABLE;
		font.bg_color		= OSD_TEXT_FONT_BG_COLOR;
		font.ch_color		= OSD_TEXT_FONT_CH_COLOR;
		font.block_alpha	= OSD_TEXT_FONT_BLOCK_ALPHA;
		font.h_gap			= OSD_TEXT_FONT_H_GAP;
		font.v_gap			= OSD_TEXT_FONT_V_GAP;
		font.date_fmt		= osd_date_fmt_9;
		font.time_fmt		= osd_time_fmt_24;



		posd2_pic_0.chn_id = ch;

		posd2_pic_1.chn_id = ch;

		posd2_pic_2.chn_id = ch;

		if (ch == 0) {
			s_txt_info_time.font = font;
			s_txt_info_time.blk_idx = 0;
			s_txt_info_time.chn_id = ch;
			s_txt_info_time.rotate = OSD_TEXT_ROTATE;
			s_txt_info_time.start_x = 10 + 320 + 50;
			s_txt_info_time.start_y = 10;


			s_txt_info_date.font = font;
			s_txt_info_date.blk_idx = 1;
			s_txt_info_date.chn_id = ch;
			s_txt_info_date.rotate = OSD_TEXT_ROTATE;
			s_txt_info_date.start_x = 10;
			s_txt_info_date.start_y = 10;


			s_txt_info_string.str = teststring;
			s_txt_info_string.font = font;
			s_txt_info_string.blk_idx = 5;
			s_txt_info_string.chn_id = ch;
			s_txt_info_string.rotate = RT_ROTATE_90L;
			s_txt_info_string.start_x = 10;
			s_txt_info_string.start_y = 10 + 100;

			posd2_pic_0.osd2.blk_idx = 2;
			posd2_pic_0.osd2.start_x = 150;
			posd2_pic_0.osd2.start_y = 200;
			posd2_pic_0.osd2.end_x = posd2_pic_0.osd2.start_x + PICT0_WIDTH;
			posd2_pic_0.osd2.end_y = posd2_pic_0.osd2.start_y + PICT0_HEIGHT;
			posd2_pic_0.osd2.blk_fmt = PICT0_BLK_FMT;
			posd2_pic_0.osd2.buf = PICT0_NAME;
			posd2_pic_0.osd2.len = PICT0_SIZE;


			posd2_pic_1.osd2.blk_idx = 3;
			posd2_pic_1.osd2.start_x = 150 + PICT0_WIDTH + 50;
			posd2_pic_1.osd2.start_y = 200;
			posd2_pic_1.osd2.end_x = posd2_pic_1.osd2.start_x + PICT1_WIDTH;
			posd2_pic_1.osd2.end_y = posd2_pic_1.osd2.start_y + PICT1_HEIGHT;
			posd2_pic_1.osd2.blk_fmt = PICT1_BLK_FMT;
			posd2_pic_1.osd2.buf = PICT1_NAME;
			posd2_pic_1.osd2.len = PICT1_SIZE;


			posd2_pic_2.osd2.blk_idx = 4;
			posd2_pic_2.osd2.start_x = 150 + PICT0_WIDTH + 50 + PICT1_WIDTH + 50;
			posd2_pic_2.osd2.start_y = 200;
			posd2_pic_2.osd2.end_x = posd2_pic_2.osd2.start_x + PICT2_WIDTH;
			posd2_pic_2.osd2.end_y = posd2_pic_2.osd2.start_y + PICT2_HEIGHT;
			posd2_pic_2.osd2.blk_fmt = PICT2_BLK_FMT;
			posd2_pic_2.osd2.color_1bpp = 0x000000FF;//0xAABBGGRR
			posd2_pic_2.osd2.buf = PICT2_NAME;
			posd2_pic_2.osd2.len = PICT2_SIZE;
		} else if (ch == 1) {
			s_txt_info_time.font = font;
			s_txt_info_time.blk_idx = 0;
			s_txt_info_time.chn_id = ch;
			s_txt_info_time.rotate = OSD_TEXT_ROTATE;
			s_txt_info_time.start_x = 10;
			s_txt_info_time.start_y = 10;


			s_txt_info_date.font = font;
			s_txt_info_date.blk_idx = 1;
			s_txt_info_date.chn_id = ch;
			s_txt_info_date.rotate = OSD_TEXT_ROTATE;
			s_txt_info_date.start_x = 10 + 320 + 50;
			s_txt_info_date.start_y = 10;


			s_txt_info_string.str = teststring;
			s_txt_info_string.font = font;
			s_txt_info_string.blk_idx = 5;
			s_txt_info_string.chn_id = ch;
			s_txt_info_string.rotate = RT_ROTATE_90R;
			s_txt_info_string.start_x = 10;
			s_txt_info_string.start_y = 10 + 100;

			posd2_pic_0.osd2.blk_idx = 2;
			posd2_pic_0.osd2.start_x = 150 + PICT0_WIDTH + 50 + PICT1_WIDTH + 50;
			posd2_pic_0.osd2.start_y = 200;
			posd2_pic_0.osd2.end_x = posd2_pic_0.osd2.start_x + PICT0_WIDTH;
			posd2_pic_0.osd2.end_y = posd2_pic_0.osd2.start_y + PICT0_HEIGHT;
			posd2_pic_0.osd2.blk_fmt = PICT0_BLK_FMT;
			posd2_pic_0.osd2.buf = PICT0_NAME;
			posd2_pic_0.osd2.len = PICT0_SIZE;


			posd2_pic_1.osd2.blk_idx = 3;
			posd2_pic_1.osd2.start_x = 150;
			posd2_pic_1.osd2.start_y = 300;
			posd2_pic_1.osd2.end_x = posd2_pic_1.osd2.start_x + PICT1_WIDTH;
			posd2_pic_1.osd2.end_y = posd2_pic_1.osd2.start_y + PICT1_HEIGHT;
			posd2_pic_1.osd2.blk_fmt = PICT1_BLK_FMT;
			posd2_pic_1.osd2.buf = PICT1_NAME;
			posd2_pic_1.osd2.len = PICT1_SIZE;


			posd2_pic_2.osd2.blk_idx = 4;
			posd2_pic_2.osd2.start_x = 150 + PICT0_WIDTH + 50;
			posd2_pic_2.osd2.start_y = 400;
			posd2_pic_2.osd2.end_x = posd2_pic_2.osd2.start_x + PICT2_WIDTH;
			posd2_pic_2.osd2.end_y = posd2_pic_2.osd2.start_y + PICT2_HEIGHT;
			posd2_pic_2.osd2.blk_fmt = PICT2_BLK_FMT;
			posd2_pic_2.osd2.color_1bpp = 0x000000FF;//0xAABBGGRR
			posd2_pic_2.osd2.buf = PICT2_NAME;
			posd2_pic_2.osd2.len = PICT2_SIZE;
		}

		rts_osd_set_info(rts_osd2_type_date, &s_txt_info_date);
		rts_osd_set_info(rts_osd2_type_time, &s_txt_info_time);
		rts_osd_set_info(rts_osd2_type_pict, &posd2_pic_0);
		rts_osd_set_info(rts_osd2_type_pict, &posd2_pic_1);
		rts_osd_set_info(rts_osd2_type_pict, &posd2_pic_2);
		rts_osd_set_info(rts_osd2_type_text, &s_txt_info_string);

		printf("[osd] Heap available:%d\r\n", xPortGetFreeHeapSize());

		if (xTaskCreate(rts_osd_task, "OSD", 10 * 1024, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
			printf("\n\r%s xTaskCreate failed", __FUNCTION__);
		}
	} else if (idx == 1) {
		nn_rect_ch	  = ch_id;
		nn_rect_txt_w = txt_w;
		nn_rect_txt_h = txt_h;

		printf("nn_rect_ch:%d, nn_rect_txt_w:%d, nn_rect_txt_h:%d.\r\n", nn_rect_ch, nn_rect_txt_w, nn_rect_txt_h);
#if DRAW_NN_OSD
		nn_data_queue = xQueueCreate(1, sizeof(struct result_frame));
		if (nn_data_queue == NULL) {
			printf("%s: nn_data_queue create fail \r\n", __FUNCTION__);
		}
#endif
		if (xTaskCreate(osd_rectangle_task, "OSD", 10 * 1024, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
			printf("\n\r%s xTaskCreate failed", __FUNCTION__);
		}
	}
}
