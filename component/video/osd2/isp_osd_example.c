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
#include "osd_font_custom.h"
#include "osd_api.h"
#include "isp_osd_example.h"
#include "video_api.h"
#include "isp_ctrl_api.h"


static osd_text_info_st s_txt_info_time;
static osd_text_info_st s_txt_info_date;
static osd_text_info_st s_txt_info_string;
static osd_text_info_st s_txt_info_iq_string[6];
static char string_buf[6][64] = {0};
static char teststring[] = "RTK-AmebaPro2";

void iq_update_info(void *arg)
{
	while (1) {
		unsigned char *iq_addr = video_get_iq_buf();
		int dvalue, dvalue2, dvalue3;
		int exp, gain;

		sprintf(string_buf[0], "IQ Version: %04d/%02d/%02d %02d:%02d:%02d", *(unsigned short *)(iq_addr + 12), iq_addr[14], iq_addr[15], iq_addr[16],
				iq_addr[17], *(unsigned short *)(iq_addr + 18));

		dvalue = -1;
		isp_get_exposure_time(&dvalue);
		exp = dvalue;
		dvalue2 = -1;
		isp_get_ae_gain(&dvalue2);
		gain = dvalue2;
		//sprintf(string_buf[1], "[AE]Exposure: %.3f AE-Gain: %.3f ET-Gain: %.3f", ((float)dvalue)/1000.0f, ((float)dvalue)/256.0f, ((float)(exp*gain))/25600.0f);
		sprintf(string_buf[1], "ET:%6d AEG:%4d", dvalue, dvalue2);

		dvalue = -1;
		isp_get_red_balance(&dvalue);
		dvalue2 = -1;
		isp_get_blue_balance(&dvalue2);
		dvalue3 = -1;
		isp_get_wb_temperature(&dvalue3);
		sprintf(string_buf[2], "R-Gain:%d B-Gain:%d CT:%d", dvalue, dvalue2, dvalue3);

		dvalue = -1;
		isp_get_day_night(&dvalue);
		sprintf(string_buf[3], "Mode:%d ETGain:%.2f", dvalue, ((float)(exp * gain)) / 25600.0f);

		for (int i = 0; i < 4; i++) {
			s_txt_info_iq_string[i].str = string_buf[i];
			rts_osd_update_info(rts_osd2_type_text, &s_txt_info_iq_string[i]);
			vTaskDelay(10);
		}
		vTaskDelay(250);
	}
}

extern void rts_osd_task(void *arg);
osd_pict_st posd2_pic_0, posd2_pic_1, posd2_pic_2;
void example_isp_osd(int idx, int ch_id, int txt_w, int txt_h)
{
	int ch = ch_id;
	rt_font_st font;
	printf("Text/Logo OSD Test\r\n");

	font.bg_enable		= OSD_TEXT_FONT_BG_ENABLE;
	font.bg_color		= OSD_TEXT_FONT_BG_COLOR;
	font.ch_color		= OSD_TEXT_FONT_CH_COLOR;
	font.block_alpha	= OSD_TEXT_FONT_BLOCK_ALPHA;
	font.h_gap			= OSD_TEXT_FONT_H_GAP;
	font.v_gap			= OSD_TEXT_FONT_V_GAP;
	font.date_fmt		= osd_date_fmt_9;
	font.time_fmt		= osd_time_fmt_24;
	if (idx == 0) {

		printf("[osd] Heap available:%d\r\n", xPortGetFreeHeapSize());
		rts_osd_init(ch_id, txt_w, txt_h, (int)(8.0f * 3600));


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
			rts_set_font_char_size(ch_id, txt_w, txt_h, eng_bin_custom, chi_bin_custom);
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
		} else if (ch == 2) {
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
			posd2_pic_0.osd2.start_x = 150 + PICT0_WIDTH + 50;
			posd2_pic_0.osd2.start_y = 200;
			posd2_pic_0.osd2.end_x = posd2_pic_0.osd2.start_x + PICT0_WIDTH;
			posd2_pic_0.osd2.end_y = posd2_pic_0.osd2.start_y + PICT0_HEIGHT;
			posd2_pic_0.osd2.blk_fmt = PICT0_BLK_FMT;
			posd2_pic_0.osd2.buf = PICT0_NAME;
			posd2_pic_0.osd2.len = PICT0_SIZE;


			posd2_pic_1.osd2.blk_idx = 3;
			posd2_pic_1.osd2.start_x = 150 + PICT0_WIDTH + 50 + PICT1_WIDTH + 50;
			posd2_pic_1.osd2.start_y = 350;
			posd2_pic_1.osd2.end_x = posd2_pic_1.osd2.start_x + PICT1_WIDTH;
			posd2_pic_1.osd2.end_y = posd2_pic_1.osd2.start_y + PICT1_HEIGHT;
			posd2_pic_1.osd2.blk_fmt = PICT1_BLK_FMT;
			posd2_pic_1.osd2.buf = PICT1_NAME;
			posd2_pic_1.osd2.len = PICT1_SIZE;


			posd2_pic_2.osd2.blk_idx = 4;
			posd2_pic_2.osd2.start_x = 150;
			posd2_pic_2.osd2.start_y = 450;
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
		hal_video_print(0);
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

		for (int i = 0; i < 6; i++) {
			s_txt_info_iq_string[i].str = teststring;
			s_txt_info_iq_string[i].font = font;
			s_txt_info_iq_string[i].blk_idx = i;
			s_txt_info_iq_string[i].chn_id = ch;
			s_txt_info_iq_string[i].rotate = RT_ROTATE_0;
			s_txt_info_iq_string[i].start_x = 10;
			s_txt_info_iq_string[i].start_y = 10 + 10 + (txt_h + 5) * i;
		}

		rts_osd_set_info(rts_osd2_type_text, &s_txt_info_iq_string[0]);
		rts_osd_set_info(rts_osd2_type_text, &s_txt_info_iq_string[1]);
		rts_osd_set_info(rts_osd2_type_text, &s_txt_info_iq_string[2]);
		rts_osd_set_info(rts_osd2_type_text, &s_txt_info_iq_string[3]);
		//rts_osd_set_info(rts_osd2_type_text, &s_txt_info_iq_string[4]);
		//rts_osd_set_info(rts_osd2_type_text, &s_txt_info_iq_string[5]);

		printf("[osd] Heap available:%d\r\n", xPortGetFreeHeapSize());

		if (xTaskCreate(rts_osd_task, "OSD", 10 * 1024, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
			printf("\n\r%s xTaskCreate failed", __FUNCTION__);
		}
		if (xTaskCreate(iq_update_info, "osd_iq_update", 10 * 1024, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
			printf("\n\r%s xTaskCreate failed", __FUNCTION__);
		}
	}
}