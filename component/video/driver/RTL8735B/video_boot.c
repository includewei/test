/******************************************************************************
*
* Copyright(c) 2021 - 2025 Realtek Corporation. All rights reserved.
*
******************************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "platform_stdlib.h"
#include "hal_video.h"
#include "hal_voe.h"
#include "video_boot.h"
#include "diag.h"
extern uint8_t __eram_end__[];
extern uint8_t __eram_start__[];
extern int __voe_code_start__[];

extern video_boot_stream_t video_boot_stream;

unsigned int video_boot_malloc(unsigned int size) // alignment 32 byte
{
	unsigned int heap_size = 0;
	unsigned int heap_addr = 0;
	if (size % 32 == 0) {
		heap_size = size;
	} else {
		heap_size = size + (32 - (size % 32));
	}
	//printf("__eram_end__ %x - __eram_start__ %x\r\n",__eram_end__,__eram_start__);
	heap_addr = (unsigned int)__eram_end__ - heap_size;
	//printf("heap_addr %x  size %d\r\n", heap_addr, heap_size);
	return heap_addr;
}

int video_boot_buf_calc(video_boot_stream_t vidoe_boot)
{
	int v3dnr_w = 2560;
	int v3dnr_h = 1440;

	int i = 0;

	if (vidoe_boot.isp_info.sensor_width && vidoe_boot.isp_info.sensor_height) {
		v3dnr_w = vidoe_boot.isp_info.sensor_width;
		v3dnr_h = vidoe_boot.isp_info.sensor_height;
	}
	//printf("v3dnr_w %d v3dnr_h %d\r\n", v3dnr_w, v3dnr_h);
	vidoe_boot.voe_heap_size += ((v3dnr_w * v3dnr_h * 3) / 2);

	//common buffer
	if (v3dnr_w >= 2560) {
		vidoe_boot.voe_heap_size += ISP_COMMON_BUF;
	}
	//vidoe_boot.voe_heap_size += ENC_COMMON_BUF;
	if (vidoe_boot.isp_info.osd_enable) {
		if (vidoe_boot.isp_info.osd_buf_size) {
			vidoe_boot.voe_heap_size += vidoe_boot.isp_info.osd_buf_size;
		} else {
			vidoe_boot.voe_heap_size += ENABLE_OSD_BUF;
		}
	}

	if (vidoe_boot.isp_info.md_enable) {
		if (vidoe_boot.isp_info.md_buf_size) {
			vidoe_boot.voe_heap_size += vidoe_boot.isp_info.md_buf_size;
		} else {
			vidoe_boot.voe_heap_size += ENABLE_MD_BUF;
		}
	}

	if (vidoe_boot.isp_info.hdr_enable) {
		vidoe_boot.voe_heap_size += ENABLE_HDR_BUF;
	}

	for (i = 0; i < 3; i++) {
		if (vidoe_boot.video_enable[i]) {
			vidoe_boot.voe_heap_size += ((vidoe_boot.video_params[i].width * vidoe_boot.video_params[i].height * 3) / 2) * 2;//isp_ch_buf_num[0];
			//ISP common
			vidoe_boot.voe_heap_size += ISP_CREATE_BUF;
			//enc ref
			vidoe_boot.voe_heap_size += ((vidoe_boot.video_params[i].width * vidoe_boot.video_params[i].height * 3) / 2) * 2;
			//enc common
			vidoe_boot.voe_heap_size += ENC_CREATE_BUF;
			//enc buffer
			vidoe_boot.voe_heap_size += ((vidoe_boot.video_params[i].width * vidoe_boot.video_params[i].height) / VIDEO_RSVD_DIVISION + (vidoe_boot.video_params[i].bps *
										 vidoe_boot.video_params[i].out_buf_size) / 8);
			//shapshot
			if (vidoe_boot.video_snapshot[i]) {
				vidoe_boot.voe_heap_size += ((vidoe_boot.video_params[i].width * vidoe_boot.video_params[i].height * 3) / 2) + SNAPSHOT_BUF;
			}
		}
	}
	if (vidoe_boot.video_enable[STREAM_V4]) { //For NN memory
		//ISP buffer
		vidoe_boot.voe_heap_size += vidoe_boot.video_params[i].width * vidoe_boot.video_params[STREAM_V4].height * 3 * 2;
		//ISP common
		vidoe_boot.voe_heap_size += ISP_CREATE_BUF;
	}
	if (vidoe_boot.voe_heap_size % 32 == 0) {
		vidoe_boot.voe_heap_size = vidoe_boot.voe_heap_size;
	} else {
		vidoe_boot.voe_heap_size = vidoe_boot.voe_heap_size + (32 - (vidoe_boot.voe_heap_size % 32));
	}
	return vidoe_boot.voe_heap_size;
}

int video_boot_open(video_boot_params_t *v_stream)
{
	int ch = v_stream->stream_id;
	int fcs_v = v_stream->fcs;
	int isp_fps = video_boot_stream.isp_info.sensor_fps;
	int fps = 30;
	int gop = 30;
	int rcMode = 1;
	int bps = 2 * 1024 * 1024;
	int minQp = 0;
	int maxQP = 51;
	int rotation = 0;
	int jpeg_qlevel = 5;
	int type;
	int res = 0;
	int codec = 0;
	int out_rsvd_size = (v_stream->width * v_stream->height) / VIDEO_RSVD_DIVISION;
	int out_buf_size = 0;
	int jpeg_out_buf_size = out_rsvd_size * 3;
	switch (ch) {
	case 0:
		out_buf_size = (v_stream->bps * V1_ENC_BUF_SIZE) / 8 + out_rsvd_size;
		break;
	case 1:
		out_buf_size = (v_stream->bps * V2_ENC_BUF_SIZE) / 8 + out_rsvd_size;
		break;
	case 2:
		out_buf_size = (v_stream->bps * V3_ENC_BUF_SIZE) / 8 + out_rsvd_size;
		break;
	}

	bps = video_boot_stream.video_params[ch].bps;

	if (video_boot_stream.video_params[ch].rc_mode) {
		rcMode = video_boot_stream.video_params[ch].rc_mode - 1;
		if (rcMode) {
			minQp = 25;
			maxQP = 48;
			bps = video_boot_stream.video_params[ch].bps / 2;
		}
	}

	hal_video_adapter_t *v_adp = hal_video_get_adp();
	v_adp->cmd[ch]->lumWidthSrc = video_boot_stream.video_params[ch].width;
	v_adp->cmd[ch]->lumHeightSrc = video_boot_stream.video_params[ch].height;
	v_adp->cmd[ch]->width = video_boot_stream.video_params[ch].width;
	v_adp->cmd[ch]->height = video_boot_stream.video_params[ch].height;

	v_adp->cmd[ch]->outputRateNumer = video_boot_stream.video_params[ch].fps;
	v_adp->cmd[ch]->inputRateNumer = isp_fps;//video_boot_stream.video_params[ch].fps;
	v_adp->cmd[ch]->intraPicRate = video_boot_stream.video_params[ch].gop;
	v_adp->cmd[ch]->rotation = video_boot_stream.video_params[ch].rotation;
	v_adp->cmd[ch]->bitPerSecond = bps;//video_boot_stream.video_params[ch].bps;
	v_adp->cmd[ch]->qpMin = minQp;
	v_adp->cmd[ch]->qpMax = maxQP;

	v_adp->cmd[ch]->vbr = rcMode;

	v_adp->cmd[ch]->CodecType = v_stream->type;
	v_adp->cmd[ch]->fcs = 1;
	v_adp->cmd[ch]->EncMode = MODE_QUEUE;
	v_adp->cmd[ch]->outputFormat     = VCENC_VIDEO_CODEC_H264;
	v_adp->cmd[ch]->max_cu_size      = 16;
	v_adp->cmd[ch]->min_cu_size      = 8;
	v_adp->cmd[ch]->max_tr_size      = 16;
	v_adp->cmd[ch]->min_tr_size      = 4;
	v_adp->cmd[ch]->tr_depth_intra   = 1;
	v_adp->cmd[ch]->tr_depth_inter   = 2;
	v_adp->cmd[ch]->level            = VCENC_H264_LEVEL_5_1;
	v_adp->cmd[ch]->profile          = VCENC_H264_HIGH_PROFILE;	// default is HEVC HIGH profile
	v_adp->cmd[ch]->byteStream = VCENC_BYTE_STREAM;
	v_adp->cmd[ch]->gopSize = 1;
	v_adp->cmd[ch]->ch = v_stream->stream_id;
	if (video_boot_stream.isp_info.osd_enable) {
		v_adp->cmd[ch]->osd = 1;
	}

	if (video_boot_stream.video_snapshot[ch]) {
		v_adp->cmd[ch]->CodecType = v_stream->type | CODEC_JPEG;
		v_adp->cmd[ch]->JpegMode = MODE_SNAPSHOT;
		v_adp->cmd[ch]->jpg_buf_size = jpeg_out_buf_size;////hal_video_jpg_buf(ch, jpeg_out_buf_size, out_rsvd_size);
		v_adp->cmd[ch]->jpg_rsvd_size = out_rsvd_size;
	}

	if (video_boot_stream.fcs_channel == 1 && ch != 0) { //Disable the fcs for channel 1
		v_adp->cmd[0]->fcs = 0;//Remove the fcs setup for channel one
		dcache_clean_invalidate_by_addr((uint32_t *)v_adp->cmd[0], sizeof(commandLine_s));
	}
	v_adp->cmd[ch]->out_buf_size  = out_buf_size;
	v_adp->cmd[ch]->out_rsvd_size = out_rsvd_size;
	v_adp->cmd[ch]->isp_buf_num = 2;

	if (video_boot_stream.fcs_isp_ae_enable) {
		v_adp->cmd[ch]->set_AE_init_flag = 1;
		v_adp->cmd[ch]->all_init_iq_set_flag = 1;
		v_adp->cmd[ch]->direct_i2c_mode = 1;
		v_adp->cmd[ch]->init_exposure = video_boot_stream.fcs_isp_ae_init_exposure;
		v_adp->cmd[ch]->init_gain = video_boot_stream.fcs_isp_ae_init_gain;
	}
	if (video_boot_stream.fcs_isp_awb_enable) {
		v_adp->cmd[ch]->set_AWB_init_flag = 1;
		v_adp->cmd[ch]->all_init_iq_set_flag = 1;
		v_adp->cmd[ch]->init_r_gain = video_boot_stream.fcs_isp_awb_init_rgain;
		v_adp->cmd[ch]->init_b_gain = video_boot_stream.fcs_isp_awb_init_bgain;
	}

	if (video_boot_stream.fcs_isp_init_daynight_mode) {
		v_adp->cmd[ch]->all_init_iq_set_flag = 1;
		v_adp->cmd[ch]->init_daynight_mode = video_boot_stream.fcs_isp_init_daynight_mode;
	}

	if (video_boot_stream.video_drop_frame[ch]) {
		v_adp->cmd[ch]->all_init_iq_set_flag = 1;
		v_adp->cmd[ch]->drop_frame_num = video_boot_stream.video_drop_frame[ch];
	}

	dcache_clean_invalidate_by_addr((uint32_t *)v_adp->cmd[ch], sizeof(commandLine_s));

	//v_stream->out_buf_size = out_buf_size;
	//v_stream->out_rsvd_size = out_rsvd_size;
	return OK;
}

/*** KM BOOT LOADER handling ***/
#define WAIT_FCS_DONE_TIMEOUT 	1000000

_WEAK void user_boot_config_init(void *parm)
{

}

extern uint8_t bl4voe_shared_test[];
int video_btldr_process(voe_fcs_load_ctrl_t *pvoe_fcs_ld_ctrl, int *code_start)
{
	int ret = OK;
	unsigned int addr = 0;
	uint8_t *p_fcs_data = NULL, *p_iq_data = NULL, *p_sensor_data = NULL;
	uint8_t fcs_id;
	voe_cpy_t isp_cpy = NULL;
	isp_multi_fcs_ld_info_t *p_fcs_ld_info = NULL;
	int i = 0;
	int video_boot_struct_size = 0;

	if (NULL == pvoe_fcs_ld_ctrl) {
		dbg_printf("voe FCS ld ctrl is NULL \n");
		ret = FCS_CPY_FUNC_ERR;
		return ret;
	} else {
		isp_cpy       = pvoe_fcs_ld_ctrl->isp_cpy;
		p_fcs_ld_info = pvoe_fcs_ld_ctrl->p_fcs_ld_info;
	}

	fcs_id = p_fcs_ld_info->fcs_id;
	p_fcs_data    = (uint8_t *)((p_fcs_ld_info->fcs_hdr_start) + (p_fcs_ld_info->sensor_set[fcs_id].fcs_data_offset));
	p_iq_data     = (uint8_t *)(p_fcs_data + (p_fcs_ld_info->sensor_set[fcs_id].iq_start_addr));
	p_sensor_data = (uint8_t *)(p_fcs_data + (p_fcs_ld_info->sensor_set[fcs_id].sensor_start_addr));
	if (hal_voe_fcs_check_OK()) {
		user_boot_config_init(pvoe_fcs_ld_ctrl->p_fcs_para_raw);
		if ((video_boot_stream.fcs_isp_iq_id != 0) && (video_boot_stream.fcs_isp_iq_id < p_fcs_ld_info->multi_fcs_cnt)) {
			p_fcs_data    = (uint8_t *)((p_fcs_ld_info->fcs_hdr_start) + (p_fcs_ld_info->sensor_set[video_boot_stream.fcs_isp_iq_id].fcs_data_offset));
			p_iq_data     = (uint8_t *)(p_fcs_data + (p_fcs_ld_info->sensor_set[video_boot_stream.fcs_isp_iq_id].iq_start_addr));
			p_sensor_data = (uint8_t *)(p_fcs_data + (p_fcs_ld_info->sensor_set[video_boot_stream.fcs_isp_iq_id].sensor_start_addr));
		}
		hal_video_load_iq((voe_cpy_t)isp_cpy, (int *) p_iq_data, (int *) __voe_code_start__);
		hal_video_load_sensor((voe_cpy_t)isp_cpy, (int *) p_sensor_data, (int *) __voe_code_start__);
		int fcs_ch = -1;
		for (i = 0; i < MAX_FCS_CHANNEL; i++) {
			if (video_boot_stream.video_params[i].fcs) {
				video_boot_open(&video_boot_stream.video_params[i]);
			}
			if ((fcs_ch == -1) && (video_boot_stream.video_params[i].fcs == 1)) { //Get the first start channel
				fcs_ch = i;
			}
		}
		__DSB();

		pvoe_fcs_peri_info_t fcs_peri_info_for_ram = pvoe_fcs_ld_ctrl->p_fcs_peri_info;


		if (fcs_peri_info_for_ram->i2c_id <= 3) {
			hal_video_isp_set_i2c_id(fcs_ch, fcs_peri_info_for_ram->i2c_id);
		}

		if (fcs_peri_info_for_ram->fcs_data_verion == 0x1) {
			if (fcs_peri_info_for_ram->gpio_cnt > 3) {
				hal_video_isp_set_sensor_gpio(fcs_ch, fcs_peri_info_for_ram->gpio_list[3], fcs_peri_info_for_ram->gpio_list[2], fcs_peri_info_for_ram->gpio_list[0]);
				//dbg_printf("snr_pwr 0x%02x pwdn 0x%02x rst 0x%02x i2c_id %d \n", fcs_peri_info_for_ram->gpio_list[0], fcs_peri_info_for_ram->gpio_list[2], fcs_peri_info_for_ram->gpio_list[3], fcs_peri_info_for_ram->i2c_id);
			}
		} else {
			if (fcs_peri_info_for_ram->gpio_cnt > 2) {
				hal_video_isp_set_sensor_gpio(fcs_ch, fcs_peri_info_for_ram->gpio_list[1], fcs_peri_info_for_ram->gpio_list[2], fcs_peri_info_for_ram->gpio_list[0]);
				//dbg_printf("snr_pwr 0x%02x pwdn 0x%02x rst 0x%02x i2c_id %d \n", fcs_peri_info_for_ram->gpio_list[0], fcs_peri_info_for_ram->gpio_list[2], fcs_peri_info_for_ram->gpio_list[1], fcs_peri_info_for_ram->i2c_id);

			}
		}

		int voe_heap_size = video_boot_buf_calc(video_boot_stream);
		addr = video_boot_malloc(voe_heap_size);
		video_boot_stream.voe_heap_addr = addr;
		video_boot_stream.voe_heap_size = voe_heap_size;
		if (video_boot_stream.fcs_start_time) { //Measure the fcs time
			video_boot_stream.fcs_voe_time = (* ((volatile uint32_t *) 0xe0001004)) / (500000) + video_boot_stream.fcs_start_time;
		}
		ret = hal_video_init((long unsigned int *)addr, voe_heap_size);
		video_boot_stream.fcs_status = 1;
	}
	video_boot_stream.fcs_voe_fw_addr = (int)pvoe_fcs_ld_ctrl->fw_addr;
	memcpy(&video_boot_stream.p_fcs_ld_info, p_fcs_ld_info, sizeof(isp_multi_fcs_ld_info_t));
	video_boot_struct_size = sizeof(video_boot_stream_t);
	if (video_boot_struct_size <= VIDEO_BOOT_STRUCT_MAX_SIZE) {
		memcpy(bl4voe_shared_test, &video_boot_stream, sizeof(video_boot_stream_t));
	} else {
		memcpy(bl4voe_shared_test, &video_boot_stream, VIDEO_BOOT_STRUCT_MAX_SIZE);
	}
	return ret;
}
