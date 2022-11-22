#include "hal_osd_util.h"
#include "video_api.h"
#include "osd_custom.h"
#include "osd_api.h"
#include "osd_render.h"
#include "osdep_service.h"

#undef printf
#include <stdio.h>

static SemaphoreHandle_t osd_render_task_stop_sema = NULL;
static QueueHandle_t canvas_msg_queue = NULL;
static osd_render_info_t osd_render_info;
static int availible_block_num[OSD_OBJ_MAX_CH] = {0};
static int available_block_idx[OSD_OBJ_MAX_CH][OSD_OBJ_MAX_NUM] = {0};
static int osd_render_task_stop_flag = 1;

int canvas_send_msg(canvas_msg_t *canvas_msg)
{

	if (!canvas_msg_queue || osd_render_task_stop_flag) {
		printf("canvas_send_msg not ready\r\n");
		return 0;
	}
	if (!uxQueueSpacesAvailable(canvas_msg_queue)) {
		printf("canvas_send_msg not available\r\n");
		return 0;
	}
	if (xQueueSendToBack(canvas_msg_queue, (void *)canvas_msg, 100) != pdPASS) {
		printf("canvas_send_msg failed\r\n");
		return 0;
	}
	return 1;
}

int canvas_clean_all(int ch, int idx)
{
	canvas_msg_t canvas_msg;
	canvas_msg.ch = ch;
	canvas_msg.idx = idx;
	canvas_msg.msg_type = CANVAS_MSG_CLEAN_ALL;
	canvas_send_msg(&canvas_msg);
	return 1;
}

int canvas_update(int ch, int idx)
{
	canvas_msg_t canvas_msg;
	canvas_msg.ch = ch;
	canvas_msg.idx = idx;
	canvas_msg.msg_type = CANVAS_MSG_DRAW;
	canvas_send_msg(&canvas_msg);
	return 1;
}

int canvas_set_point(int ch, int idx, int xmin, int ymin, int point_width, uint32_t color)
{
	canvas_msg_t canvas_msg;
	canvas_msg.ch = ch;
	canvas_msg.idx = idx;
	canvas_msg.msg_type = CANVAS_MSG_POINT;
	canvas_msg.draw_data.point.x = xmin;
	canvas_msg.draw_data.point.y = ymin;
	canvas_msg.draw_data.point.pt_width = point_width;
	canvas_msg.color.argb_u32 = color;
	canvas_send_msg(&canvas_msg);
	return 1;
}

int canvas_set_rect(int ch, int idx, int xmin, int ymin, int xmax, int ymax, int line_width, uint32_t color)
{
	canvas_msg_t canvas_msg;
	canvas_msg.ch = ch;
	canvas_msg.idx = idx;
	canvas_msg.msg_type = CANVAS_MSG_RECT;
	canvas_msg.draw_data.rect.line_width = line_width;
	canvas_msg.draw_data.rect.start_point.x = xmin;
	canvas_msg.draw_data.rect.start_point.y = ymin;
	canvas_msg.draw_data.rect.end_point.x = xmax;
	canvas_msg.draw_data.rect.end_point.y = ymax;
	canvas_msg.color.argb_u32 = color;
	canvas_send_msg(&canvas_msg);
	return 1;
}

int canvas_set_text(int ch, int idx, int xmin, int ymin, char *text_string, uint32_t color)
{
	canvas_msg_t canvas_msg;
	canvas_msg.ch = ch;
	canvas_msg.idx = idx;
	canvas_msg.msg_type = CANVAS_MSG_TEXT;
	canvas_msg.draw_data.text.start_point.x = xmin;
	canvas_msg.draw_data.text.start_point.y = ymin;
	snprintf(canvas_msg.draw_data.text.text_str, sizeof(canvas_msg.draw_data.text.text_str), "%s", text_string);
	canvas_msg.color.argb_u32 = color;
	canvas_send_msg(&canvas_msg);
	return 1;
}

void osd_render_task_stop(void)
{
	osd_render_task_stop_flag = 1;

	if (xSemaphoreTake(osd_render_task_stop_sema, portMAX_DELAY) == pdTRUE) {
		printf("finish close nn osd\r\n");
		vSemaphoreDelete(osd_render_task_stop_sema);
		osd_render_task_stop_sema = NULL;
		return;
	}
}

void osd_render_task(void *arg)
{
	int i, j;

#if defined(configENABLE_TRUSTZONE) && (configENABLE_TRUSTZONE == 1)
	rtw_create_secure_context(2048);
#endif

	canvas_msg_t cavas_msg_recieve;
	osd_render_info.ready2draw = 1;
	while (!osd_render_task_stop_flag) {
		if (xQueueReceive(canvas_msg_queue, &cavas_msg_recieve, 100) == pdPASS) {
			int ch = cavas_msg_recieve.ch;
			int block_idx = cavas_msg_recieve.idx;
			int pic_idx = ch * OSD_OBJ_MAX_NUM + block_idx;
			int bimap_index = osd_render_info.render_obj[pic_idx].buff_used_index;
			osd_pict_st *osd2_pic = &osd_render_info.render_obj[pic_idx].osd2_pic;
			uint8_t **buff_bmp = &osd_render_info.render_obj[pic_idx].buff_bmp[bimap_index];
			if (availible_block_num[ch] && (rts_osd_get_status(ch)) && (video_get_stream_info(ch))) {
				if (!*buff_bmp) {
					*buff_bmp = (uint8_t *)malloc(osd_render_info.channel_xmax[ch] * osd_render_info.channel_ymax[ch]);
					printf("id: %d, malloc(%d* %d)\r\n", ch, osd_render_info.channel_xmax[ch], osd_render_info.channel_ymax[ch]);
				}
				if (*buff_bmp) {
					uint32_t tick1 = xTaskGetTickCount();
					switch (cavas_msg_recieve.msg_type) {
					case CANVAS_MSG_DRAW:
						//update osd
						osd2_pic->osd2.start_x = 0;
						osd2_pic->osd2.start_y = 0;
						osd2_pic->osd2.end_x = osd2_pic->osd2.start_x + osd_render_info.channel_xmax[ch];
						osd2_pic->osd2.end_y = osd2_pic->osd2.start_y + osd_render_info.channel_ymax[ch];
						osd2_pic->osd2.len = osd_render_info.channel_xmax[ch] * osd_render_info.channel_ymax[ch];
						osd2_pic->osd2.buf = *buff_bmp;
						rts_osd_bitmap_update(osd2_pic->chn_id, &(osd2_pic->osd2), 1);
						osd_render_info.render_obj[pic_idx].buff_used_index = osd_render_info.render_obj[pic_idx].buff_used_index ^ 0x01;
						//printf("\r\nupdate after %dms.\n", (xTaskGetTickCount()-tick1));
						break;
					case CANVAS_MSG_CLEAN_ALL:
						memset(*buff_bmp, 0x00, osd_render_info.channel_xmax[ch] * osd_render_info.channel_ymax[ch]);
						//printf("\r\nclean all after %dms.\n", (xTaskGetTickCount()-tick1));
						break;
					case CANVAS_MSG_TEXT:
						cavas_msg_recieve.draw_data.text.start_point.x = cavas_msg_recieve.draw_data.text.start_point.x & (~7);
						cavas_msg_recieve.draw_data.text.start_point.y = cavas_msg_recieve.draw_data.text.start_point.y & (~7);
						//printf("update text\r\n");
						//printf("coor: %d, %d\r\n",cavas_msg_recieve.draw_data.text.start_point.x, cavas_msg_recieve.draw_data.text.start_point.y);
						draw_text_on_bitmap(*buff_bmp, osd_render_info.channel_xmax[ch], osd_render_info.channel_ymax[ch], ch,
											&cavas_msg_recieve.draw_data.text, &cavas_msg_recieve.color);
						break;
					case CANVAS_MSG_RECT:
						cavas_msg_recieve.draw_data.rect.start_point.x = cavas_msg_recieve.draw_data.rect.start_point.x & (~7);
						cavas_msg_recieve.draw_data.rect.end_point.x = cavas_msg_recieve.draw_data.rect.end_point.x & (~7);
						cavas_msg_recieve.draw_data.rect.start_point.y = cavas_msg_recieve.draw_data.rect.start_point.y & (~7);
						cavas_msg_recieve.draw_data.rect.end_point.y = cavas_msg_recieve.draw_data.rect.end_point.y & (~7);
						//printf("coor: %d, %d, %d, %d\r\n",cavas_msg_recieve.draw_data.rect.start_point.x, cavas_msg_recieve.draw_data.rect.start_point.y, cavas_msg_recieve.draw_data.rect.end_point.x, cavas_msg_recieve.draw_data.rect.end_point.y);
						//printf("color: %d, %d, %d\r\n", cavas_msg_recieve.color.argb.r, cavas_msg_recieve.color.argb.g, cavas_msg_recieve.color.argb.b);
						draw_rect_on_bitmap(*buff_bmp, osd_render_info.channel_xmax[ch], osd_render_info.channel_ymax[ch], &cavas_msg_recieve.draw_data.rect,
											&cavas_msg_recieve.color);
						//printf("\r\ndraw_rect_on_bitmap after %dms.\n", (xTaskGetTickCount()-tick1));
						break;
					case CANVAS_MSG_POINT:
						cavas_msg_recieve.draw_data.point.x = cavas_msg_recieve.draw_data.point.x & (~7);
						cavas_msg_recieve.draw_data.point.y = cavas_msg_recieve.draw_data.point.y & (~7);
						draw_point_on_bitmap(*buff_bmp, osd_render_info.channel_xmax[ch], osd_render_info.channel_ymax[ch], &cavas_msg_recieve.draw_data.point,
											 &cavas_msg_recieve.color);
						break;
					default:
						break;
					}
				}

			}


		}
	}

	printf("clear all the block when close\r\n");
	//clear all the block when close
	for (i = 0; i < OSD_OBJ_MAX_CH; i++) {
		for (j = 0; j < OSD_OBJ_MAX_NUM; j++) {
			int pic_idx = i * OSD_OBJ_MAX_NUM + j;
			osd_render_info.render_obj[pic_idx].buff_used_index = osd_render_info.render_obj[pic_idx].buff_used_index ^ 0x01;
			int bimap_index = osd_render_info.render_obj[pic_idx].buff_used_index;
			osd_pict_st *osd2_pic = &osd_render_info.render_obj[pic_idx].osd2_pic;
			uint8_t **buff_bmp = &osd_render_info.render_obj[pic_idx].buff_bmp[bimap_index];
			int num = availible_block_num[i];
			if (num && (rts_osd_get_status(i)) && (video_get_stream_info(i))) {
				if (*buff_bmp) {
					osd2_pic->osd2.len = 8 * 8;
					osd2_pic->osd2.start_x = 0;
					osd2_pic->osd2.start_y = 0;
					osd2_pic->osd2.end_x = 8;
					osd2_pic->osd2.end_y = 8;
					memset(*buff_bmp, 0, osd2_pic->osd2.len);
				}
				if (*buff_bmp) {
					osd2_pic->osd2.buf = *buff_bmp;
					rts_osd_bitmap_update(i, &osd2_pic->osd2, 1);
					rts_osd_hide_bitmap(i, &osd2_pic->osd2);
				}
			}
		}
	}

	for (i = 0; i < OSD_OBJ_MAX_CH * OSD_OBJ_MAX_NUM; i++) {
		if (osd_render_info.render_obj[i].buff_bmp[0]) {
			free(osd_render_info.render_obj[i].buff_bmp[0]);
		}
		osd_render_info.render_obj[i].buff_bmp[0] = NULL;
		if (osd_render_info.render_obj[i].buff_bmp[1]) {
			free(osd_render_info.render_obj[i].buff_bmp[1]);
		}
		osd_render_info.render_obj[i].buff_bmp[1] = NULL;
	}

	vQueueDelete(canvas_msg_queue);
	canvas_msg_queue = NULL;

	xSemaphoreGive(osd_render_task_stop_sema);

	vTaskDelete(NULL);
}

void osd_render_task_start(int *ch_visible, int *ch_width, int *ch_height)
{
	if (!osd_render_task_stop_flag) {
		printf("NN_OSD_DRAW start failed: task is not close or closing.\r\n");
		return;
	}

	for (int i = 0; i < OSD_OBJ_MAX_CH; i++) {
		if (ch_visible[i]) {
			if (!rts_osd_get_status(i)) {
				printf("NN_OSD_DRAW start failed: Osd ch %d not init.\r\n", i);
				return;
			}
			osd_render_info.channel_en[i] = ch_visible[i];
			osd_render_info.channel_xmax[i] = ch_width[i] & (~7);
			osd_render_info.channel_ymax[i] = ch_height[i] & (~7);
			rts_osd_get_available_block(i, &availible_block_num[i], available_block_idx[i]);
			if (availible_block_num[i] == 0) {
				printf("NN_OSD_DRAW start failed: Osd ch %d no block availible.\r\n", i);
				return;
			} else {
				printf("NN_OSD_DRAW start: set availible_block_num[%d] from %d to 1\r\n", i, availible_block_num[i]);
				availible_block_num[i] = 1;
			}

			printf("osd ch %d e%d num %d (%d, %d, %d)\r\n", i, ch_visible[i], availible_block_num[i], available_block_idx[i][0], available_block_idx[i][1],
				   available_block_idx[i][2]);
		}
	}

	osd_render_task_stop_flag = 0;
	osd_render_info.ready2draw = 0;

	printf("NN OSD Draw start\r\n");

	canvas_msg_queue = xQueueCreate(100, sizeof(canvas_msg_t));
	if (canvas_msg_queue == NULL) {
		printf("%s: canvas_msg_queue create fail \r\n", __FUNCTION__);
		return;
	}

	osd_render_task_stop_sema = xSemaphoreCreateBinary();
	if (osd_render_task_stop_sema == NULL) {
		printf("%s: osd_render_task_stop_sema create fail \r\n", __FUNCTION__);
		return;
	}

	enum rts_osd2_blk_fmt disp_format = RTS_OSD2_BLK_FMT_RGBA2222;
	memset(&osd_render_info.render_obj[0], 0, sizeof(osd_render_obj_t) * OSD_OBJ_MAX_CH * OSD_OBJ_MAX_NUM);
	for (int i = 0; i < OSD_OBJ_MAX_CH; i++) {
		for (int j = 0; j < OSD_OBJ_MAX_NUM; j++) {
			int pic_idx = i * OSD_OBJ_MAX_NUM + j;
			osd_pict_st *osd2_pic = &osd_render_info.render_obj[pic_idx].osd2_pic;
			osd_render_info.render_obj[pic_idx].buff_bmp[0] = NULL;
			osd_render_info.render_obj[pic_idx].buff_bmp[1] = NULL;
			osd2_pic->chn_id = i;
			osd2_pic->osd2.blk_idx = available_block_idx[i][j];
			osd2_pic->osd2.blk_fmt = disp_format;
			osd2_pic->show = 0;
		}
	}

	if (xTaskCreate(osd_render_task, "NN_OSD_DRAW", 10 * 1024, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
		printf("\n\r%s xTaskCreate failed", __FUNCTION__);
	}
	while (!osd_render_info.ready2draw) { //wait for task ready
		vTaskDelay(10);
	}
	return;
}

void osd_render_dev_init(int *ch_enable, int *char_resize_w, int *char_resize_h)
{
	int char_w, char_h;
	for (int i = 0; i < OSD_OBJ_MAX_CH; i++) {
		if (ch_enable[i]) {
			char_w = (char_resize_w[i] + 7) & (~7);
			char_h = (char_resize_h[i] + 7) & (~7);
			//hal_video_osd_enc_enable(i, 1);
			rts_osd_init(i, char_w, char_h, (int)(8.0f * 3600));
			rts_osd_release_init_protect();
		}
	}
}

void osd_render_dev_deinit(int ch)
{
	if (rts_osd_get_status(ch)) {
		rts_osd_deinit(ch);
	}
}

void osd_render_dev_deinit_all()
{
	for (int i = 0; i < OSD_OBJ_MAX_CH; i++) {
		if (rts_osd_get_status(i)) {
			rts_osd_deinit(i);
		}
	}

}