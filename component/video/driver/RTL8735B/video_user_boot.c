#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "platform_stdlib.h"
#include <unistd.h>
#include <sys/wait.h>
#include "base_type.h"
#include "cmsis.h"
#include "error.h"
#include "hal.h"
#include "hal_video.h"
#include "hal_voe.h"
#include "video_api.h"
#include "video_boot.h"
#include "hal_snand.h"
//#define USE_2K_SENSOR
#define CHANGE_PARAMETER
video_boot_stream_t video_boot_stream = {
	.video_params[STREAM_V1].stream_id = STREAM_V1,
	.video_params[STREAM_V1].type = CODEC_H264,
	.video_params[STREAM_V1].resolution = 0,
#ifdef USE_2K_SENSOR
	.video_params[STREAM_V1].width = 2560,
	.video_params[STREAM_V1].height = 1440,
#else
	.video_params[STREAM_V1].width = 1920,
	.video_params[STREAM_V1].height = 1080,
#endif
	.video_params[STREAM_V1].bps = 2 * 1024 * 1024,
	.video_params[STREAM_V1].fps = 15,
	.video_params[STREAM_V1].gop = 15,
	.video_params[STREAM_V1].rc_mode = 2,
	.video_params[STREAM_V1].jpeg_qlevel = 0,
	.video_params[STREAM_V1].rotation = 0,
	.video_params[STREAM_V1].out_buf_size = 0,
	.video_params[STREAM_V1].out_rsvd_size = 0,
	.video_params[STREAM_V1].direct_output = 0,
	.video_params[STREAM_V1].use_static_addr = 0,
	.video_snapshot[STREAM_V1] = 0,
	.video_drop_frame[STREAM_V1] = 0,
	.video_params[STREAM_V1].fcs = 1,//Enable the fcs for channel 1
	.video_params[STREAM_V2].stream_id = STREAM_V2,
	.video_params[STREAM_V2].type = CODEC_H264,
	.video_params[STREAM_V2].resolution = 0,
	.video_params[STREAM_V2].width = 1280,
	.video_params[STREAM_V2].height = 720,
	.video_params[STREAM_V2].bps = 1 * 1024 * 1024,
	.video_params[STREAM_V2].fps = 15,
	.video_params[STREAM_V2].gop = 15,
	.video_params[STREAM_V2].rc_mode = 0,
	.video_params[STREAM_V2].jpeg_qlevel = 0,
	.video_params[STREAM_V2].rotation = 0,
	.video_params[STREAM_V2].out_buf_size = 0,
	.video_params[STREAM_V2].out_rsvd_size = 0,
	.video_params[STREAM_V2].direct_output = 0,
	.video_params[STREAM_V2].use_static_addr = 0,
	.video_params[STREAM_V2].fcs = 0,
	.video_snapshot[STREAM_V2] = 0,
	.video_drop_frame[STREAM_V2] = 0,
	.video_params[STREAM_V3].stream_id = STREAM_V3,
	.video_params[STREAM_V3].type = CODEC_H264,
	.video_params[STREAM_V3].resolution = 0,
	.video_params[STREAM_V3].width = 0,
	.video_params[STREAM_V3].height = 0,
	.video_params[STREAM_V3].bps = 0,
	.video_params[STREAM_V3].fps = 0,
	.video_params[STREAM_V3].gop = 0,
	.video_params[STREAM_V3].rc_mode = 0,
	.video_params[STREAM_V3].jpeg_qlevel = 0,
	.video_params[STREAM_V3].rotation = 0,
	.video_params[STREAM_V3].out_buf_size = 0,
	.video_params[STREAM_V3].out_rsvd_size = 0,
	.video_params[STREAM_V3].direct_output = 0,
	.video_params[STREAM_V3].use_static_addr = 0,
	.video_params[STREAM_V3].fcs = 0,
	.video_snapshot[STREAM_V3] = 0,
	.video_drop_frame[STREAM_V3] = 0,
	.video_params[STREAM_V4].stream_id = STREAM_V4,
	.video_params[STREAM_V4].type = 0,
	.video_params[STREAM_V4].resolution = 0,
	.video_params[STREAM_V4].width = 640,
	.video_params[STREAM_V4].height = 480,
	.video_params[STREAM_V4].bps = 0,
	.video_params[STREAM_V4].fps = 0,
	.video_params[STREAM_V4].gop = 0,
	.video_params[STREAM_V4].rc_mode = 0,
	.video_params[STREAM_V4].jpeg_qlevel = 0,
	.video_params[STREAM_V4].rotation = 0,
	.video_params[STREAM_V4].out_buf_size = 0,
	.video_params[STREAM_V4].out_rsvd_size = 0,
	.video_params[STREAM_V4].direct_output = 0,
	.video_params[STREAM_V4].use_static_addr = 0,
	.video_params[STREAM_V4].fcs = 0,
	.video_enable[STREAM_V1] = 1,
	.video_enable[STREAM_V2] = 1,
	.video_enable[STREAM_V3] = 0,
	.video_enable[STREAM_V4] = 1,
	.fcs_isp_ae_enable = 0,
	.fcs_isp_ae_init_exposure = 0,
	.fcs_isp_ae_init_gain = 0,
	.fcs_isp_awb_enable = 0,
	.fcs_isp_awb_init_rgain = 0,
	.fcs_isp_awb_init_bgain = 0,
	.fcs_isp_init_daynight_mode = 0,
	.voe_heap_size = 0,
	.voe_heap_addr = 0,
#ifdef USE_2K_SENSOR
	.isp_info.sensor_width = 2560,
	.isp_info.sensor_height = 1440,
#else
	.isp_info.sensor_width = 1920,
	.isp_info.sensor_height = 1080,
#endif
	.isp_info.md_enable = 1,
	.isp_info.hdr_enable = 1,
	.isp_info.osd_enable = 1,
	.fcs_channel = 1,//FCS_TOTAL_NUMBER
	.fcs_status = 0,
	.fcs_setting_done = 0
};
#ifdef CHANGE_PARAMETER
extern hal_snafc_adaptor_t boot_snafc_adpt;
uint8_t snand_data[2112] __attribute__((aligned(32)));
#define NAND_FLASH_FCS 0x7080000 //900*128*1024 It msut be first page for the block
#define NAND_PAGE_SIZE 2048
#define NAND_PAGE_COUNT  64
#define NAND_BLOCK_COUNT 1024
#define NOR_FLASH_FCS  (0xF00000 + 0xD000)
#define NOR_FLASH_BASE 0x08000000
#endif
#ifdef CHANGE_PARAMETER
int boot_get_remap_table_index(unsigned char *buf, int block_index, int *remap_index)
{
	int i = 0;
	int ret = -1;
	int bad_block = 0;
	for (i = 1; i < NAND_PAGE_SIZE / 8; i++) {
		if ((buf[i * 8] == 'B') && (buf[i * 8 + 1] == 'B')) {
			if (buf[i * 8 + 6] != 'b' && buf[i * 8 + 7] != 'b') {
				break;//The remap table is not correct
			} else {
				bad_block = buf[i * 8 + 2] | buf[i * 8 + 3] << 8;
				if (bad_block == block_index) {
					*remap_index = buf[i * 8 + 4] | buf[i * 8 + 5] << 8;
					//dbg_printf("remap_index %d\r\n",*remap_index);
					ret = 0;
					break;
				}
			}
		}
	}
	return ret;
}

int boot_get_fcs_remap_block(unsigned int address, unsigned int *remap_value)
{
	int i = 0;
	int ret = -1;
	int remap_index = 0;
	for (i = (NAND_BLOCK_COUNT - 4); i < NAND_BLOCK_COUNT; i++) {
		hal_snand_page_read(&boot_snafc_adpt, &snand_data[0], NAND_PAGE_SIZE + 32, i * NAND_PAGE_COUNT);
		if (snand_data[NAND_PAGE_SIZE] == 0xff) {
			ret = boot_get_remap_table_index(&snand_data[0], address / (NAND_PAGE_SIZE * NAND_PAGE_COUNT), &remap_index);
			if (ret >= 0) {
				*remap_value = remap_index;
				//dbg_printf("remap_value %d\r\n",*remap_value);
				ret = 0;
				break;
			}
		} else {
			//dbg_printf("bad block %d\r\n",i);
		}
	}
	return ret;
}
#endif
void user_boot_config_init(void *parm)
{
	//Insert your code into here
	//dbg_printf("user_boot_config_init\r\n");
#ifdef CHANGE_PARAMETER
	video_boot_stream_t *fcs_data = NULL;
	if (hal_sys_get_boot_select() == 1) { //For nand flash
		hal_snand_page_read(&boot_snafc_adpt, &snand_data[0], 2048 + 32, NAND_FLASH_FCS / NAND_PAGE_SIZE);
		if (snand_data[2048] != 0xff) { //Check the bad block
			int ret = 0;
			int remap_value = 0;
			ret = boot_get_fcs_remap_block(NAND_FLASH_FCS, &remap_value);
			if (ret >= 0) {
				//dbg_printf("Find remap value %d\r\n",remap_value);
				hal_snand_page_read(&boot_snafc_adpt, &snand_data[0], NAND_PAGE_SIZE + 32, remap_value * NAND_PAGE_COUNT);
			} else {
				dbg_printf("It can't get the remap table\r\n");
				return;
			}
		}
	} else {
		dcache_invalidate_by_addr((uint32_t *)NOR_FLASH_BASE + NOR_FLASH_FCS, 2048);
		memcpy(snand_data, (void *)(NOR_FLASH_BASE + NOR_FLASH_FCS), 2048);
	}

	if (snand_data[0] == 'F' && snand_data[1] == 'C' && snand_data[2] == 'S' && snand_data[3] == 'D') {
		unsigned int checksum_tag = snand_data[4] | snand_data[5] << 8 | snand_data[6] << 16 | snand_data[7] << 24;
		unsigned int checksum_value = 0;
		for (int i = 0; i < sizeof(video_boot_stream_t); i++) {
			checksum_value += snand_data[i + 8];
		}
		if (checksum_tag == checksum_value) {
			fcs_data = (video_boot_stream_t *)(snand_data + 8);
			if ((video_boot_stream.isp_info.sensor_width == fcs_data->isp_info.sensor_width) &&
				(video_boot_stream.isp_info.sensor_height == fcs_data->isp_info.sensor_height)) {
				memcpy(&video_boot_stream, fcs_data, sizeof(video_boot_stream_t));
				//dbg_printf("Update parameter\r\n");
			}
		} else {
			dbg_printf("Check sum fail\r\n");
		}
	} else {
		dbg_printf("Can't find %x %x %x %x\r\n", snand_data[0], snand_data[1], snand_data[2], snand_data[3]);
	}
#endif
}