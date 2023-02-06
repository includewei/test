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
		//printf("canvas_send_msg not ready\r\n");
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

int canvas_create_bitmap(int ch, int idx, int xmin, int ymin, int xmax, int ymax, enum rts_osd2_blk_fmt bmp_format)
{
	if (availible_block_num[ch] <= idx) {
		printf("canvas_create_bitmap: idx larger than available block\r\n");
		return 0;
	}
	canvas_msg_t canvas_msg;
	canvas_msg.ch = ch;
	canvas_msg.idx = idx;
	canvas_msg.msg_type = CANVAS_CREATE_BMP;
	canvas_msg.draw_data.bmp.bmp_format = bmp_format;
	canvas_msg.draw_data.bmp.start_point.x = xmin;
	canvas_msg.draw_data.bmp.start_point.y = ymin;
	canvas_msg.draw_data.bmp.end_point.x = xmax;
	canvas_msg.draw_data.bmp.end_point.y = ymax;
	canvas_send_msg(&canvas_msg);
	return 1;
}

int canvas_create_bitmap_all(int ch, int idx, int xmin, int ymin, int xmax, int ymax, enum rts_osd2_blk_fmt bmp_format)
{
	if (availible_block_num[ch] <= idx) {
		printf("canvas_create_bitmap: idx larger than available block\r\n");
		return 0;
	}
	canvas_msg_t canvas_msg;
	canvas_msg.ch = ch;
	canvas_msg.idx = idx;
	canvas_msg.msg_type = CANVAS_CREATE_BMP_ALL;
	canvas_msg.draw_data.bmp.bmp_format = bmp_format;
	canvas_msg.draw_data.bmp.start_point.x = xmin;
	canvas_msg.draw_data.bmp.start_point.y = ymin;
	canvas_msg.draw_data.bmp.end_point.x = xmax;
	canvas_msg.draw_data.bmp.end_point.y = ymax;
	canvas_send_msg(&canvas_msg);
	return 1;
}

int canvas_clean_all(int ch, int idx)
{
	if (availible_block_num[ch] <= idx) {
		printf("canvas_clean_all: idx larger than available block\r\n");
		return 0;
	}
	canvas_msg_t canvas_msg;
	canvas_msg.ch = ch;
	canvas_msg.idx = idx;
	canvas_msg.msg_type = CANVAS_MSG_CLEAN_ALL;
	canvas_send_msg(&canvas_msg);
	return 1;
}

int canvas_update(int ch, int idx, int ready2update)
{
	if (availible_block_num[ch] <= idx) {
		printf("canvas_update: idx larger than available block\r\n");
		return 0;
	}
	canvas_msg_t canvas_msg;
	canvas_msg.ch = ch;
	canvas_msg.idx = idx;
	canvas_msg.msg_type = CANVAS_MSG_DRAW;
	canvas_msg.draw_data.ready2update = ready2update;
	canvas_send_msg(&canvas_msg);
	return 1;
}

int canvas_set_point(int ch, int idx, int xmin, int ymin, int point_width, uint32_t color)
{
	if (availible_block_num[ch] <= idx) {
		printf("canvas_set_point: idx larger than available block\r\n");
		return 0;
	}
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

int canvas_set_line(int ch, int idx, int xmin, int ymin, int xmax, int ymax, int line_width, uint32_t color)
{
	if (availible_block_num[ch] <= idx) {
		printf("canvas_set_point: idx larger than available block\r\n");
		return 0;
	}
	canvas_msg_t canvas_msg;
	canvas_msg.ch = ch;
	canvas_msg.idx = idx;
	canvas_msg.msg_type = CANVAS_MSG_LINE;
	canvas_msg.draw_data.line.line_width = line_width;
	canvas_msg.draw_data.line.start_point.x = xmin;
	canvas_msg.draw_data.line.start_point.y = ymin;
	canvas_msg.draw_data.line.end_point.x = xmax;
	canvas_msg.draw_data.line.end_point.y = ymax;
	canvas_msg.color.argb_u32 = color;
	canvas_send_msg(&canvas_msg);
	return 1;
}

int canvas_set_rect(int ch, int idx, int xmin, int ymin, int xmax, int ymax, int line_width, uint32_t color)
{
	if (availible_block_num[ch] <= idx) {
		printf("canvas_set_rect: idx larger than available block\r\n");
		return 0;
	}
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
	if (availible_block_num[ch] <= idx) {
		printf("canvas_set_text: idx larger than available block\r\n");
		return 0;
	}
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
	if (!osd_render_task_stop_flag) {
		osd_render_task_stop_flag = 1;
	} else {
		printf("osd_render_task already closing.\r\n");
		return;
	}

	if (xSemaphoreTake(osd_render_task_stop_sema, portMAX_DELAY) == pdTRUE) {
		printf("finish close nn osd\r\n");
		vSemaphoreDelete(osd_render_task_stop_sema);
		osd_render_task_stop_sema = NULL;
		return;
	}
	return;
}

void osd_render_task(void *arg)
{
	int i, j;
	canvas_msg_t cavas_msg_recieve;

#if defined(configENABLE_TRUSTZONE) && (configENABLE_TRUSTZONE == 1)
	rtw_create_secure_context(2048);
#endif

	osd_render_info.ready2draw = 1;
	while (!osd_render_task_stop_flag) {
		if (xQueueReceive(canvas_msg_queue, &cavas_msg_recieve, 100) == pdPASS) {
			int ch = cavas_msg_recieve.ch;
			int block_idx = cavas_msg_recieve.idx;
			int pic_idx = ch * OSD_OBJ_MAX_NUM + available_block_idx[ch][block_idx];
			osd_render_obj_t *obj = &osd_render_info.render_obj[pic_idx];
			osd_pict_st *osd2_pic = &osd_render_info.render_obj[pic_idx].osd2_pic;
			int bimap_index = obj->buff_used_index;
			uint8_t **buff_bmp = &(obj->bitmap[bimap_index].buff);
			if (availible_block_num[ch] && (rts_osd_get_status(ch)) && (video_get_stream_info(ch))) {
				if (cavas_msg_recieve.msg_type == CANVAS_CREATE_BMP_ALL) {
					obj->bitmap[0].start_point.x = cavas_msg_recieve.draw_data.bmp.start_point.x & (~7);
					obj->bitmap[0].start_point.y = cavas_msg_recieve.draw_data.bmp.start_point.y & (~7);
					obj->bitmap[0].end_point.x = cavas_msg_recieve.draw_data.bmp.end_point.x & (~7);
					obj->bitmap[0].end_point.y = cavas_msg_recieve.draw_data.bmp.end_point.y & (~7);
					obj->bitmap[0].bmp_format = cavas_msg_recieve.draw_data.bmp.bmp_format;

					obj->bitmap[1].start_point.x = obj->bitmap[0].start_point.x;
					obj->bitmap[1].start_point.y = obj->bitmap[0].start_point.y;
					obj->bitmap[1].end_point.x = obj->bitmap[0].end_point.x;
					obj->bitmap[1].end_point.y = obj->bitmap[0].end_point.y;
					obj->bitmap[1].bmp_format = obj->bitmap[0].bmp_format;

					if (obj->bitmap[0].bmp_format == RTS_OSD2_BLK_FMT_RGBA2222) {
						obj->bitmap[0].buff_len = (obj->bitmap[0].end_point.x - obj->bitmap[0].start_point.x) * (obj->bitmap[0].end_point.y - obj->bitmap[0].start_point.y);
						obj->bitmap[1].buff_len = obj->bitmap[0].buff_len;
					} else if (obj->bitmap[0].bmp_format == RTS_OSD2_BLK_FMT_1BPP) {
						obj->bitmap[0].buff_len = (((obj->bitmap[0].end_point.x - obj->bitmap[0].start_point.x + 63) & (~63)) / 8) * (obj->bitmap[0].end_point.y -
												  obj->bitmap[0].start_point.y);
						obj->bitmap[1].buff_len = obj->bitmap[0].buff_len;
					}

					if (obj->bitmap[0].buff) {
						free(obj->bitmap[0].buff);
						obj->bitmap[0].buff = NULL;
					}
					if (!obj->bitmap[0].buff) {
						obj->bitmap[0].buff = (uint8_t *)malloc(obj->bitmap[0].buff_len);
						//printf("osd_render_task: id%d malloc(%d)\r\n", ch, buff_len[0]);
					} else {
						printf("osd_render_task: id%d malloc(%d) failed\r\n", ch, obj->bitmap[0].buff_len);
					}

					if (obj->bitmap[1].buff) {
						free(obj->bitmap[1].buff);
						obj->bitmap[1].buff = NULL;
					}
					if (!obj->bitmap[1].buff) {
						obj->bitmap[1].buff = (uint8_t *)malloc(obj->bitmap[1].buff_len);
						//printf("osd_render_task: id%d malloc(%d)\r\n", ch, buff_len[1] );
					} else {
						printf("osd_render_task: id%d malloc(%d) failed\r\n", ch, obj->bitmap[1].buff_len);
					}
				} else if (cavas_msg_recieve.msg_type == CANVAS_CREATE_BMP) {
					obj->bitmap[bimap_index].start_point.x = cavas_msg_recieve.draw_data.bmp.start_point.x & (~7);
					obj->bitmap[bimap_index].start_point.y = cavas_msg_recieve.draw_data.bmp.start_point.y & (~7);
					obj->bitmap[bimap_index].end_point.x = cavas_msg_recieve.draw_data.bmp.end_point.x & (~7);
					obj->bitmap[bimap_index].end_point.y = cavas_msg_recieve.draw_data.bmp.end_point.y & (~7);
					obj->bitmap[bimap_index].bmp_format = cavas_msg_recieve.draw_data.bmp.bmp_format;

					if (obj->bitmap[bimap_index].bmp_format == RTS_OSD2_BLK_FMT_RGBA2222) {
						obj->bitmap[bimap_index].buff_len = (obj->bitmap[bimap_index].end_point.x - obj->bitmap[bimap_index].start_point.x) *
															(obj->bitmap[bimap_index].end_point.y - obj->bitmap[bimap_index].start_point.y);
					} else if (obj->bitmap[bimap_index].bmp_format == RTS_OSD2_BLK_FMT_1BPP) {
						obj->bitmap[bimap_index].buff_len = (((obj->bitmap[bimap_index].end_point.x - obj->bitmap[bimap_index].start_point.x + 63) & (~63)) / 8) *
															(obj->bitmap[bimap_index].end_point.y - obj->bitmap[bimap_index].start_point.y);
					} else {
						printf("osd_render_task CANVAS_CREATE_BMP failed: not suppoted bitmap format\r\n");
						return;
					}

					if (obj->bitmap[bimap_index].buff) {
						free(obj->bitmap[bimap_index].buff);
						obj->bitmap[bimap_index].buff = NULL;
					}
					if (!obj->bitmap[bimap_index].buff) {
						obj->bitmap[bimap_index].buff = (uint8_t *)malloc(obj->bitmap[bimap_index].buff_len);
						//printf("osd_render_task: ch%d id%d w %d h %d\r\n", ch, pic_idx, (obj->bitmap[bimap_index].end_point.x - obj->bitmap[bimap_index].start_point.x), (obj->bitmap[bimap_index].end_point.y - obj->bitmap[bimap_index].start_point.y));
						//printf("osd_render_task: ch%d id%d malloc(%d)\r\n", ch, pic_idx, obj->bitmap[bimap_index].buff_len);
					} else {
						printf("osd_render_task: ch%d id%d malloc(%d) failed\r\n", ch, pic_idx, obj->bitmap[bimap_index].buff_len);
					}

				} else {
					if (*buff_bmp) {
						uint32_t tick1 = xTaskGetTickCount();
						switch (cavas_msg_recieve.msg_type) {
						case CANVAS_MSG_DRAW:
							//update osd
							osd2_pic->osd2.start_x = obj->bitmap[bimap_index].start_point.x;
							osd2_pic->osd2.start_y = obj->bitmap[bimap_index].start_point.y;
							osd2_pic->osd2.end_x = obj->bitmap[bimap_index].end_point.x;
							osd2_pic->osd2.end_y = obj->bitmap[bimap_index].end_point.y;
							osd2_pic->osd2.blk_fmt = obj->bitmap[bimap_index].bmp_format;
							osd2_pic->osd2.color_1bpp = obj->bitmap[bimap_index].color_1bpp;
							osd2_pic->osd2.buf = *buff_bmp;
							osd2_pic->osd2.len = obj->bitmap[bimap_index].buff_len;

							rts_osd_bitmap_update(osd2_pic->chn_id, &(osd2_pic->osd2), cavas_msg_recieve.draw_data.ready2update);
							obj->buff_used_index = obj->buff_used_index ^ 0x01;
							//printf("\r\nupdate after %dms.\n", (xTaskGetTickCount()-tick1));
							break;
						case CANVAS_MSG_CLEAN_ALL:
							memset(*buff_bmp, 0x00, obj->bitmap[bimap_index].buff_len);
							//printf("\r\nclean all after %dms.\n", (xTaskGetTickCount()-tick1));
							break;
						case CANVAS_MSG_TEXT:
							cavas_msg_recieve.draw_data.text.start_point.x = cavas_msg_recieve.draw_data.text.start_point.x & (~7);
							cavas_msg_recieve.draw_data.text.start_point.y = cavas_msg_recieve.draw_data.text.start_point.y & (~7);
							//draw_text_on_bitmap(*buff_bmp, width, height, ch, &cavas_msg_recieve.draw_data.text, &cavas_msg_recieve.color);
							draw_text_on_bitmap(&obj->bitmap[bimap_index], ch, &cavas_msg_recieve.draw_data.text, &cavas_msg_recieve.color);
							break;
						case CANVAS_MSG_RECT:
							cavas_msg_recieve.draw_data.rect.start_point.x = cavas_msg_recieve.draw_data.rect.start_point.x & (~7);
							cavas_msg_recieve.draw_data.rect.end_point.x = cavas_msg_recieve.draw_data.rect.end_point.x & (~7);
							cavas_msg_recieve.draw_data.rect.start_point.y = cavas_msg_recieve.draw_data.rect.start_point.y & (~7);
							cavas_msg_recieve.draw_data.rect.end_point.y = cavas_msg_recieve.draw_data.rect.end_point.y & (~7);
							draw_rect_on_bitmap(&obj->bitmap[bimap_index], &cavas_msg_recieve.draw_data.rect, &cavas_msg_recieve.color);
							//printf("\r\ndraw_rect_on_bitmap after %dms.\n", (xTaskGetTickCount()-tick1));
							break;
						case CANVAS_MSG_LINE:
							cavas_msg_recieve.draw_data.line.start_point.x = cavas_msg_recieve.draw_data.line.start_point.x & (~7);
							cavas_msg_recieve.draw_data.line.end_point.x = cavas_msg_recieve.draw_data.line.end_point.x & (~7);
							cavas_msg_recieve.draw_data.line.start_point.y = cavas_msg_recieve.draw_data.line.start_point.y & (~7);
							cavas_msg_recieve.draw_data.line.end_point.y = cavas_msg_recieve.draw_data.line.end_point.y & (~7);
							draw_line_on_bitmap(&obj->bitmap[bimap_index], &cavas_msg_recieve.draw_data.line, &cavas_msg_recieve.color);
							//printf("\r\draw_line_on_bitmaps after %dms.\n", (xTaskGetTickCount()-tick1));
							break;
						case CANVAS_MSG_POINT:
							cavas_msg_recieve.draw_data.point.x = cavas_msg_recieve.draw_data.point.x & (~7);
							cavas_msg_recieve.draw_data.point.y = cavas_msg_recieve.draw_data.point.y & (~7);
							draw_point_on_bitmap(&obj->bitmap[bimap_index], &cavas_msg_recieve.draw_data.point, &cavas_msg_recieve.color);
							//printf("\r\draw_point_on_bitmaps after %dms.\n", (xTaskGetTickCount()-tick1));
							break;
						default:
							break;
						}
					} else {
						printf("osd_render_task: bitmap not create\r\n");
					}
				}
			}
		}
	}

	printf("clear all the block when close\r\n");
	//clear all the block when close
	for (i = 0; i < OSD_OBJ_MAX_CH; i++) {
		for (j = 0; j < availible_block_num[i]; j++) {
			int pic_idx = i * OSD_OBJ_MAX_NUM + available_block_idx[i][j];
			osd_render_info.render_obj[pic_idx].buff_used_index = osd_render_info.render_obj[pic_idx].buff_used_index ^ 0x01;
			int bimap_index = osd_render_info.render_obj[pic_idx].buff_used_index;
			osd_pict_st *osd2_pic = &osd_render_info.render_obj[pic_idx].osd2_pic;
			uint8_t **buff_bmp = &(osd_render_info.render_obj[pic_idx].bitmap[bimap_index].buff);
			if (*buff_bmp && (rts_osd_get_status(i)) && (video_get_stream_info(i))) {
				osd2_pic->osd2.len = 8 * 8;
				osd2_pic->osd2.start_x = 0;
				osd2_pic->osd2.start_y = 0;
				osd2_pic->osd2.end_x = 8;
				osd2_pic->osd2.end_y = 8;
				osd2_pic->osd2.blk_fmt = RTS_OSD2_BLK_FMT_1BPP;
				memset(*buff_bmp, 0, osd2_pic->osd2.len);
				osd2_pic->osd2.buf = *buff_bmp;
				rts_osd_bitmap_update(i, &osd2_pic->osd2, 1);
				rts_osd_hide_bitmap(i, &osd2_pic->osd2);
			}
		}
	}

	for (i = 0; i < OSD_OBJ_MAX_CH; i++) {
		for (j = 0; j < availible_block_num[i]; j++) {
			int pic_idx = i * OSD_OBJ_MAX_NUM + available_block_idx[i][j];
			if (osd_render_info.render_obj[pic_idx].bitmap[0].buff) {
				free(osd_render_info.render_obj[pic_idx].bitmap[0].buff);
			}
			osd_render_info.render_obj[pic_idx].bitmap[0].buff = NULL;
			if (osd_render_info.render_obj[pic_idx].bitmap[1].buff) {
				free(osd_render_info.render_obj[pic_idx].bitmap[1].buff);
			}
			osd_render_info.render_obj[pic_idx].bitmap[1].buff = NULL;

		}
	}

	vQueueDelete(canvas_msg_queue);
	canvas_msg_queue = NULL;

	xSemaphoreGive(osd_render_task_stop_sema);

	vTaskDelete(NULL);
}

void osd_render_task_start(int *ch_visible, int *ch_width, int *ch_height)
{
	if (!osd_render_task_stop_flag || osd_render_task_stop_sema) {
		printf("osd_render_task start failed: task is not close or closing.\r\n");
		return;
	}

	for (int i = 0; i < OSD_OBJ_MAX_CH; i++) {
		if (ch_visible[i]) {
			if (!rts_osd_get_status(i)) {
				printf("osd_render_task start failed: Osd ch %d not init.\r\n", i);
				return;
			}
			osd_render_info.channel_en[i] = ch_visible[i];
			osd_render_info.channel_xmax[i] = ch_width[i] & (~7);
			osd_render_info.channel_ymax[i] = ch_height[i] & (~7);
			rts_osd_get_available_block(i, &availible_block_num[i], available_block_idx[i]);
			if (availible_block_num[i] == 0) {
				printf("osd_render_task start failed: Osd ch %d no block availible.\r\n", i);
				return;
			}
			printf("osd ch %d e%d num %d (%d, %d, %d)\r\n", i, ch_visible[i], availible_block_num[i], available_block_idx[i][0], available_block_idx[i][1],
				   available_block_idx[i][2]);

			//set osd boundary check
			rts_osd_set_frame_size(i, osd_render_info.channel_xmax[i], osd_render_info.channel_ymax[i]);
		}
	}

	osd_render_task_stop_flag = 0;
	osd_render_info.ready2draw = 0;

	printf("osd_render_task start\r\n");

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
			memset(osd2_pic, 0, sizeof(osd_pict_st));
			osd_render_info.render_obj[pic_idx].bitmap[0].buff = NULL;
			osd_render_info.render_obj[pic_idx].bitmap[0].buff_len = 0;
			osd_render_info.render_obj[pic_idx].bitmap[1].buff = NULL;
			osd_render_info.render_obj[pic_idx].bitmap[1].buff_len = 0;
			osd2_pic->chn_id = i;
			osd2_pic->osd2.blk_idx = j;
			osd2_pic->osd2.blk_fmt = disp_format;
			osd2_pic->show = 0;
		}
	}

	if (xTaskCreate(osd_render_task, "osd_render_task", 10 * 1024, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
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