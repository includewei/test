#include <platform_stdlib.h>
#include <platform_opts.h>
#include <build_info.h>
#include "log_service.h"
#include "atcmd_isp.h"
#include "main.h"
#include "flash_api.h"
#include "hal_osd_util.h"
#include "hal_md_util.h"
#include "hal_video.h"
#include "osd/osd_custom.h"
#include "osd/osd_api.h"
#include "md/md2_api.h"


//-------- AT SYS commands ---------------------------------------------------------------
#define CMD_DATA_SIZE 65536

void myTaskDelay(uint32_t delay_ms)
{
	//vTaskDelay(delay_ms);
}
void fATIT(void *arg)
{
#if 1
	int i;
	int ccmd;
	int value;
	char *cmd_data;
	struct isp_tuning_cmd *iq_cmd;
	int argc = 0;
	char *argv[MAX_ARGC] = {0};

	if (!arg) {
		return;
	}


	argc = parse_param(arg, argv);
	if (argc < 1) {	// usage
		printf("iqtun cmd\r\n");
		printf("      0: rts_isp_tuning_get_iq\n");
		printf("      1 : rts_isp_tuning_set_iq\r\n");
		printf("      2 : rts_isp_tuning_get_statis\r\n");
		printf("      3 : rts_isp_tuning_get_param\r\n");
		printf("      4 : rts_isp_tuning_set_param\r\n");
		return;
	}
	ccmd = atoi(argv[1]);

	cmd_data = malloc(CMD_DATA_SIZE);
	if (cmd_data == NULL) {
		printf("malloc cmd buf fail\r\n");
		return;
	}
	iq_cmd = (struct isp_tuning_cmd *)cmd_data;

	if (ccmd == 0) {
		iq_cmd->addr = ISP_TUNING_IQ_TABLE_ALL;
		hal_video_isp_tuning(VOE_ISP_TUNING_GET_IQ, iq_cmd);
	} else if (ccmd == 1) {
		iq_cmd->addr = ISP_TUNING_IQ_TABLE_ALL;
		hal_video_isp_tuning(VOE_ISP_TUNING_SET_IQ, iq_cmd);
	} else if (ccmd == 2) {
		iq_cmd->addr = ISP_TUNING_STATIS_ALL;
		hal_video_isp_tuning(VOE_ISP_TUNING_GET_STATIS, iq_cmd);
	} else if (ccmd == 3) {
		iq_cmd->addr = ISP_TUNING_PARAM_ALL;
		hal_video_isp_tuning(VOE_ISP_TUNING_GET_PARAM, iq_cmd);
	} else if (ccmd == 4) {
		iq_cmd->addr = ISP_TUNING_PARAM_ALL;
		hal_video_isp_tuning(VOE_ISP_TUNING_SET_PARAM, iq_cmd);
	} else if (ccmd == 11) {
		iq_cmd->addr = atoi(argv[1]);
		iq_cmd->len  = atoi(argv[2]);
		hal_video_isp_tuning(VOE_ISP_TUNING_READ_VREG, iq_cmd);
		for (i = 0; i < iq_cmd->len; i++) {
			printf("virtual reg[%d]: 0x02%X.\r\n", i, iq_cmd->data[i]);
		}
		iq_cmd->data[i] = atoi(argv[3 + i]);
	} else if (ccmd == 12) {
		iq_cmd->addr = atoi(argv[1]);
		iq_cmd->len  = atoi(argv[2]);
		for (i = 0; i < iq_cmd->len; i++) {
			iq_cmd->data[i] = atoi(argv[3 + i]);
		}
		hal_video_isp_tuning(VOE_ISP_TUNING_WRITE_VREG, iq_cmd);
	}

	if (ccmd >= 0 && ccmd <= 4) {
		myTaskDelay(300);
		printf("len = %d.\r\n", iq_cmd->len);
	}

	if (cmd_data) {
		free(cmd_data);
	}

#else
	int type;
	int set_flag;
	int table_type;
	int set_value;
	int print_addr;
	int ret;
	int i;
	struct isp_tuning_cmd *cmd = (struct isp_tuning_cmd *)malloc(65536);
	int argc = 0;
	char *argv[MAX_ARGC] = {0};

	if (!arg) {
		return;
	}

	argc = parse_param(arg, argv);

	table_type = atoi(argv[1]);
	set_flag = atoi(argv[2]);
	type = atoi(argv[3]);
	print_addr = atoi(argv[4]);
	cmd->addr = type;
	printf("print_addr:%d  table_type:%d  set_flag:%d  cmd->addr=%d.\r\n", print_addr, table_type, set_flag, cmd->addr);
	if (argc >= 3) {
		if (table_type == 0) {
			//ISP_TUNING_IQ_TABLE_ALL
			if (set_flag == 0) {
				hal_video_isp_tuning(VOE_ISP_TUNING_GET_IQ, cmd);
			} else {
				hal_video_isp_tuning(VOE_ISP_TUNING_SET_IQ, cmd);
			}
		} else if (table_type == 1) {
			//ISP_TUNING_STATIS_ALL
			if (set_flag == 0) {
				hal_video_isp_tuning(VOE_ISP_TUNING_GET_STATIS, cmd);
			}
		} else if (table_type == 2) {
			//ISP_TUNING_PARAM_ALL
			if (set_flag == 0) {
				hal_video_isp_tuning(VOE_ISP_TUNING_GET_PARAM, cmd);
			} else {
				hal_video_isp_tuning(VOE_ISP_TUNING_SET_PARAM, cmd);
			}
		}
		myTaskDelay(50);
		printf("len = %d.\r\n", cmd->len);
		if (print_addr) {
			for (i = 0; i < cmd->len; i++) {
				if (i > 0 && i % 16 == 0) {
					printf("\r\n");
				}
				printf("0x%02X,", cmd->data[i]);
			}
		}
	}
	free(cmd);
#endif
}
void fATIC(void *arg)
{
	int i;
	int set_flag;
	int ret;
#if 1
	int id;
	int set_value;
	int argc = 0;
	char *argv[MAX_ARGC] = {0};

	if (!arg) {
		return;
	}

	argc = parse_param(arg, argv);

	if (argc >= 2) {
		set_flag = atoi(argv[1]);
		id = atoi(argv[2]);
		printf("[isp ctrl] set_flag:%d  id:%d.\r\n", set_flag, id);
		if (set_flag == 0) {
			hal_video_isp_ctrl(id, set_flag, &ret);
			if (ret != -1) {
				printf("result 0x%08x %d \r\n", ret, ret);
			} else {
				printf("isp_ctrl get error\r\n");
			}
		} else {

			if (argc >= 3) {
				hal_video_isp_ctrl(id, 0, &ret);
				if (ret != -1) {
					printf("before set result 0x%08x %d \r\n", ret, ret);
					myTaskDelay(20);
					set_value = atoi(argv[3]);
					printf("[isp ctrl] set_value:%d.\r\n", set_value);

					ret = hal_video_isp_ctrl(id, set_flag, &set_value);
					if (ret != 0) {
						printf("isp_ctrl set error\r\n");
					} else {
						myTaskDelay(20);
						hal_video_isp_ctrl(id, 0, &ret);
						if (ret != -1) {
							printf("check result 0x%08x %d \r\n", ret, ret);
						} else {
							printf("isp_ctrl get error\r\n");
						}
					}
				} else {
					printf("isp_ctrl get error\r\n");
				}

			} else {
				printf("isp_ctrl set error : need 3 argument: set_flag id  set_value\r\n");
			}
		}

	} else {
		printf("isp_ctrl  error : need 2~3 argument: set_flag id  [set_value] \r\n");
	}
#else
	printf("\r\n[ISP_CTRL_TEST GET]\r\n");
	set_flag = 0;
	int aRet[64];
	int test_value;
	for (i = 0; i < 44; i++) {
		aRet[i] = hal_video_isp_ctrl(i, set_flag, NULL);
		myTaskDelay(50);
	}
	myTaskDelay(1000);
	for (i = 0; i < 44; i++) {
		if (aRet[i] != -1) {
			printf("\r\n[RESULT ID %d]: 0x%08x %d ", i, aRet[i], aRet[i]);
		} else {
			printf("\r\n[RESULT ID %d]: GET_ERROR", i);
		}
	}
	myTaskDelay(1000);
	printf("\r\n\r\n[ISP_CTRL_TEST SET]\r\n");
	set_flag = 1;
	for (i = 0; i < 44; i++) {
		if (aRet[i] == 0) {
			test_value = 1;
		} else if (aRet[i] == 1) {
			test_value = 0;
		} else {
			test_value = aRet[i] + 1;
		}
		aRet[i] = hal_video_isp_ctrl(i, set_flag, test_value);
		myTaskDelay(50);
	}
	myTaskDelay(1000);
	for (i = 0; i < 44; i++) {
		if (aRet[i] != 0) {
			printf("\r\n[RESULT ID %d]: SET_ERROR", i);
		} else {
			printf("\r\n[RESULT ID %d]: SET_SUCCESS", i);
		}
	}










	myTaskDelay(1000);
	printf("\r\n[ISP_CTRL_TEST GET]\r\n");
	set_flag = 0;
	for (i = 0; i < 28; i++) {
		aRet[i] = hal_video_isp_ctrl(i + 0xF000, set_flag, NULL);
		myTaskDelay(50);
	}
	myTaskDelay(1000);
	for (i = 0; i < 28; i++) {
		if (aRet[i] != -1) {
			printf("\r\n[RESULT ID %d]: 0x%08x %d ", i, aRet[i], aRet[i]);
		} else {
			printf("\r\n[RESULT ID %d]: GET_ERROR", i);
		}
	}


	myTaskDelay(1000);
	printf("\r\n\r\n[ISP_CTRL_TEST SET]\r\n");
	set_flag = 1;
	for (i = 0; i < 28; i++) {
		if (aRet[i] == 0) {
			test_value = 1;
		} else if (aRet[i] == 1) {
			test_value = 0;
		} else {
			test_value = aRet[i] + 1;
		}
		aRet[i] = hal_video_isp_ctrl(i + 61440, set_flag, test_value);
		myTaskDelay(50);
	}
	myTaskDelay(1000);
	for (i = 0; i < 28; i++) {
		if (aRet[i] != 0) {
			printf("\r\n[RESULT ID %d]: SET_ERROR", i);
		} else {
			printf("\r\n[RESULT ID %d]: SET_SUCCESS", i);
		}
	}
#endif
}
void fATIX(void *arg)
{
	int i;
	int argc = 0;
	int addr = 0;
	int num = 0;
	int value32 = 0;
	short value16 = 0;
	char value8  = 0;
	char *argv[MAX_ARGC] = {0};

	if (!arg) {
		return;
	}

	argc = parse_param(arg, argv);
	if (strcmp(argv[1], "help") == 0) {
		printf("[ATIX] Usage: ATIX=FUNCTION,ADDRESS,NUMBER,VALUE1,VALUE2...\r\n");
		printf("--FUNCTION: read32,write32,read16,write16,read8,write8.\r\n");
		printf("--ADDRESS : register address.(2 bytes,exclude base-address)\r\n");
		printf("--NUMBER  : number of value.\r\n");
		printf("--VALUE#  : necessary if FUNCTION=write32, write16 or write8.\r\n");
	}

	num  = atoi(argv[3]);
	if (num <= 0) {
		return;
	}

	addr = atoi(argv[2]);
	if (strcmp(argv[1], "read32") == 0) {

		printf("[ISP]register read addr from 0x%X:\r\n", 0x40300000 | addr);
		for (i = 0; i < num; i++) {
			if (i > 0 && i % 8 == 0) {
				printf("\r\n");
			}
			printf("0x%X \r\n", HAL_READ32(0x40300000, addr + 4 * i));
		}
		printf("\r\n");
	} else if (strcmp(argv[1], "write32") == 0) {

		printf("[ISP]register write addr from 0x%X:\r\n", 0x40300000 | addr);
		for (i = 0; i < num; i++) {
			if (i > 0 && i % 8 == 0) {
				printf("\r\n");
			}
			value32 = atoi(argv[4 + i]);
			if ((addr + 4 * i) == 0x05bf4) {
				if (value32 == 1) {
					printf("Enter TNR Debug Mode.\r\n");
					HAL_WRITE32(0x40300000, 0x00004, 0x1f3bf);
					myTaskDelay(5);
					HAL_WRITE32(0x40300000, 0x05800, 0x4f);
					myTaskDelay(5);
					HAL_WRITE32(0x40300000, 0x05bf4, 0x0e);
					myTaskDelay(5);
					HAL_WRITE32(0x40300000, 0x00004, 0x3f3bf);
					myTaskDelay(5);
					HAL_WRITE32(0x40300000, 0x05800, 0x5f);
				} else {
					HAL_WRITE32(0x40300000, (addr + 4 * i), value32);
				}
			}
			printf("0x%X \r\n", value32);
		}
		printf("\r\n");
	} else if (strcmp(argv[1], "read16") == 0) {

		printf("[ISP]register read addr from 0x%X:\r\n", 0x40300000 | addr);
		for (i = 0; i < num; i++) {
			if (i > 0 && i % 8 == 0) {
				printf("\r\n");
			}
			printf("0x%X \r\n", HAL_READ16(0x40300000, addr + 2 * i));
		}
		printf("\r\n");
	} else if (strcmp(argv[1], "write16") == 0) {

		printf("[ISP]register write addr from 0x%X:\r\n", 0x40300000 | addr);
		for (i = 0; i < num; i++) {
			if (i > 0 && i % 8 == 0) {
				printf("\r\n");
			}
			value16 = (short)atoi(argv[4 + i]);
			HAL_WRITE16(0x40300000, (addr + 2 * i), value16);
			printf("0x%X \r\n", value16);
		}
		printf("\r\n");
	} else if (strcmp(argv[1], "read8") == 0) {

		printf("[ISP]register read addr from 0x%X:\r\n", 0x40300000 | addr);
		for (i = 0; i < num; i++) {
			if (i > 0 && i % 8 == 0) {
				printf("\r\n");
			}
			printf("0x%X \r\n", HAL_READ8(0x40300000, addr + 1 * i));
		}
		printf("\r\n");
	} else if (strcmp(argv[1], "write8") == 0) {

		printf("[ISP]register write addr from 0x%X:\r\n", 0x40300000 | addr);
		for (i = 0; i < num; i++) {
			if (i > 0 && i % 8 == 0) {
				printf("\r\n");
			}
			value8 = (char)atoi(argv[4 + i]);
			HAL_WRITE8(0x40300000, (addr + 1 * i), value8);
			printf("0x%X \r\n", value8);
		}
		printf("\r\n");
	}
}

#include "isp_ctrl_api.h"
void fATII(void *arg)
{
	int i;
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	int stream_id = 0;
	int bayer_index = 0;

	if (!arg) {
		return;
	}

	argc = parse_param(arg, argv);

	if (argc < 3) {
		return;
	}

	if (strcmp(argv[1], "bayer") == 0) {
		stream_id = atoi(argv[2]);
		bayer_index = atoi(argv[3]);
		if (bayer_index < 0 || bayer_index > 9) {
			return;
		}
		if (bayer_index == 7 || bayer_index == 8 || bayer_index == 9) {
			HAL_WRITE32(0x50000000, 0x0918, 0x3733);
		}
		hal_video_isp_set_rawfmt(stream_id, bayer_index);
	} else if (strcmp(argv[1], "gray") == 0) {
		int mode = -1;
		isp_set_gray_mode(atoi(argv[2]));
		isp_get_gray_mode(&mode);
		printf("isp gray mode: %d.\r\n", mode);
	} else if (strcmp(argv[1], "flash") == 0) {
		flash_t flash;
		int fw_size = 0;
		if (strcmp(argv[2], "read") == 0) {
			flash_stream_read(&flash, 0x400000, sizeof(int), (u8 *) &fw_size);
			printf("IQ FW size: 0x%04X.\r\n", fw_size);
		} else if (strcmp(argv[2], "erase") == 0) {
			int *iq_tmp = malloc(CMD_DATA_SIZE);
			if (iq_tmp) {
				memset(iq_tmp, 0, CMD_DATA_SIZE);
				flash_erase_block(&flash, 0x400000);
				flash_stream_write(&flash, 0x400000, CMD_DATA_SIZE, iq_tmp);
				free(iq_tmp);
			}
		}
	} else if (strcmp(argv[1], "i2c") == 0) {
		struct rts_isp_i2c_reg reg;
		int ret;
		reg.addr = atoi(argv[3]);
		if (strcmp(argv[2], "read") == 0) {
			reg.data = 0;
			ret = hal_video_i2c_read(&reg);
			printf("ret: %d, read addr:0x%04X, data:0x%04X.\r\n", ret, reg.addr, reg.data);
		} else if (strcmp(argv[2], "write") == 0) {
			reg.data = atoi(argv[4]);
			printf("write addr:0x%04X, data:0x%04X.\r\n", reg.addr, reg.data);
			ret = hal_video_i2c_write(&reg);
			printf("ret: %d, .\r\n", ret);

			reg.data = 0;
			ret = hal_video_i2c_read(&reg);
			printf("ret: %d, read addr:0x%04X, data:0x%04X.\r\n", ret, reg.addr, reg.data);
		}
	} else {

	}

}


#include "../video/osd2/isp_osd_example.h"
#include "osd_api.h"
#include <sntp/sntp.h>
void fATIO(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	argc = parse_param(arg, argv);
	if (strcmp(argv[1], "size") == 0) {
		rts_set_char_size(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
	} else if (strcmp(argv[1], "block") == 0) {
		if (atoi(argv[4]) == 1) {
			rts_osd_block_show(atoi(argv[2]), atoi(argv[3]));
		} else {
			rts_osd_block_hide(atoi(argv[2]), atoi(argv[3]));
		}
	} else if (strcmp(argv[1], "task") == 0) {
		example_isp_osd(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
	} else if (strcmp(argv[1], "timezone") == 0) {
		rts_osd_set_timezone(atoi(argv[2]));
		printf("Current time-zone:%d.\r\n", rts_osd_get_timezone());
	} else if (strcmp(argv[1], "close") == 0) {
		rts_osd_deinit(atoi(argv[2]));
	}
}

extern void md_output_cb(void *param1, void  *param2, uint32_t arg);

void fATID(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	argc = parse_param(arg, argv);

	if (strcmp(argv[1], "start") == 0) {
		if (hal_video_md_cb(md_output_cb) != OK) {
			printf("hal_video_md_cb_register fail\n");
			return NULL;
		}
		md_start();
	} else if (strcmp(argv[1], "set") == 0) {
		uint32_t buf[10];

		if (strcmp(argv[2], "roi") == 0) {
			buf[0] = atoi(argv[3]);
			buf[1] = atoi(argv[4]);
			buf[2] = atoi(argv[5]);
			buf[3] = atoi(argv[6]);
			buf[4] = atoi(argv[7]);
			buf[5] = atoi(argv[8]);
			md_set(MD2_PARAM_ROI, &buf[0]);
		} else if (strcmp(argv[2], "sstt") == 0) {
			buf[0] = atoi(argv[3]);
			md_set(MD2_PARAM_SENSITIVITY, &buf[0]);
		}
	} else if (strcmp(argv[1], "stop") == 0) {
		md_stop();
	}
}

void fATIR(void *arg)
{
	volatile int argc, error_no = 0;
	char *argv[MAX_ARGC] = {0};
	int mode;

	if (!arg) {
		AT_DBG_MSG(AT_FLAG_WIFI, AT_DBG_ERROR,
				   "\r\n[ATPW] Usage : ATPW=<mode>");
		error_no = 1;
		goto EXIT;
	}

	argc = parse_param(arg, argv);


	// Init LED control pin
	if (strcmp(argv[1], "init") == 0) {
		ir_cut_init(NULL);
		ir_ctrl_init(NULL);
		ambient_light_sensor_init(NULL);
		ambient_light_sensor_power(1);
		printf("ir_cut_init/ir_ctrl_init/ambient_light_sensor_init.\r\n");
	} else if (strcmp(argv[1], "enable") == 0) {
		mode = atoi(argv[2]);
		if (mode == 1) {
			ir_cut_enable(0);
			vTaskDelay(1000);
			ir_cut_enable(1);
			printf("IR Cut On\r\n");
		} else if (mode == 0) {
			ir_cut_enable(1);
			vTaskDelay(1000);
			ir_cut_enable(0);
			printf("IR Cut Off\r\n");
		}
	} else if (strcmp(argv[1], "getlight") == 0) {
		int lux = ambient_light_sensor_get_lux(0);
		printf("light_sensor: %d \r\n", lux);
	} else if (strcmp(argv[1], "setlight") == 0) {
		int dValue1 = atoi(argv[2]);
		float fBright = (float)dValue1 / 100.0f;
		if (dValue1 > 100) {
			dValue1 = 100;
		}
		if (dValue1 < 0) {
			dValue1 = 0;
		}

		printf("led_brightness: %f \r\n", fBright);
		ir_ctrl_set_brightness(fBright);
		ir_ctrl_set_brightness_d(dValue1);
	} else if (strcmp(argv[1], "service") == 0) {
		init_sensor_service();
	}

	return;
EXIT:
	printf("error at command format\r\n");
}


log_item_t at_isp_items[] = {
	{"ATIT", fATIT,},
	{"ATIC", fATIC,},
	{"ATIX", fATIX,},
	{"ATII", fATII,},
	{"ATIO", fATIO,},
	{"ATID", fATID,},
	{"ATIR", fATIR,},
};

void at_isp_init(void)
{
	log_service_add_table(at_isp_items, sizeof(at_isp_items) / sizeof(at_isp_items[0]));
}

#if SUPPORT_LOG_SERVICE
log_module_init(at_isp_init);
#endif
