#include "audio_tool_command.h"
#include "log_service.h"

mm_context_t	*audio_save_ctx		= NULL;
mm_context_t 	*null_save_ctx		= NULL;
mm_context_t 	*array_pcm_ctx		= NULL;
mm_context_t 	*pcm_tone_ctx		= NULL;
mm_context_t 	*afft_test_ctx		= NULL;
mm_siso_t	 	*siso_audio_null	= NULL;
mm_mimo_t	 	*mimo_aarray_audio	= NULL;


audio_params_t audio_save_params = {
	.sample_rate = ASR_16KHZ,
	.word_length = WL_16BIT,
	.mic_gain    = MIC_0DB,
	.dmic_l_gain    = DMIC_BOOST_0DB,
	.dmic_r_gain    = DMIC_BOOST_0DB,
	.use_mic_type   = USE_AUDIO_AMIC,
	.channel     = 1,
	.mix_mode = 0,
	.enable_aec  = 0,
	.enable_ns   = 3,
	.enable_agc  = 0,
	.NS_level	 = -1,
	.NS_level_SPK	= -1,
	.ADC_gain		= 0x66,
	.DAC_gain		= 0xAF,
	.mic_bias		= 0,
	.hpf_set		= 0,
};

array_params_t pcm16k_array_params = {
	.type = AVMEDIA_TYPE_AUDIO,
	.codec_id = AV_CODEC_ID_PCM_RAW,
	.mode = ARRAY_MODE_LOOP,
	.u = {
		.a = {
			.channel    = 1,
			.samplerate = 16000,
			.frame_size = 640,
		}
	}
};

tone_params_t pcm_tone_params = {
	.codec_id = AV_CODEC_ID_PCM_RAW,
	.mode = ARRAY_MODE_LOOP,
	.channel    = 1,
	.audiotonerate = 1000,
	.samplerate = 16000,
	.frame_size = 640,
};

afft_params_t afft_test_params = {
	.sample_rate = 16000,
	.channel    = 1,
};

int NS_level = -1;
int NS_level_SPK = -1;
int ADC_gain = 0x66;
int DAC_gain = 0xAF;
int frame_count = 0;
int record_frame_count = 0;
int record_state = 0; // 0 no record 1 start record 2 recording 3 record end
int reset_flag = 0;
int the_sample_rate = 16000;
int playing_sample_rate = 16000;
int mic_bias = 0; //0:0.9 1:0.86 2:0.75
int mic_gain = 0; //0:0dB 1:20dB 2:30dB 3:40dB
int hpf_set = 0; //0~7
int tx_mode = 0; //0: none 1: playtone 2: playback 3: playmusic
int audio_fft_show = 0;
int record_type = RECORD_RX_DATA;

uint8_t sdsave_status = 0x0;

#define MAX_RECORD_TIME     5*60000    //5 min        
#define MIN_RECORD_TIME     40         //40 ms     

FILE  *m_record_file_RX;
FILE  *m_record_file_TX;
FILE  *m_record_file_ASP;
char ram_record_file_RX[32];
char ram_record_file_TX[32];
char ram_record_file_ASP[32];
char file_name[20] = "test";
int recored_count = 0;
xSemaphoreHandle  ram_dump_sd_sema = NULL;

void audio_fatfs_drv_open(void)
{
	int res;
	vfs_init(NULL);

	res = vfs_user_register("audio_ram", VFS_FATFS, VFS_INF_RAM);
	if (res < 0) {
		printf("fatfs_ram_init fail (%d)\n\r", res);
		return ;
	} else {
		printf("fatfs_ram_init open\n\r");
	}

	res = vfs_user_register("audio_sd", VFS_FATFS, VFS_INF_SD);
	if (res < 0) {
		printf("fatfs_sd_init fail (%d)\n\r", res);
		return ;
	} else {
		printf("fatfs_sd_init open\n\r");
	}
	return ;
}

void audio_fatfs_drv_close(void)
{
	vfs_user_unregister("audio_ram", VFS_FATFS, VFS_INF_RAM);
	vfs_user_unregister("audio_sd", VFS_FATFS, VFS_INF_SD);
	printf("fatfs_ram_init close\n\r");
	vfs_deinit(NULL);
}

void audio_open_record_file(void)
{
	char path[64];


	printf("\n\r=== FATFS Example (RAM DISK) ===\n\r");
	printf("\n\r=== RAM FS Read/Write test ===\n\r");

	recored_count ++;
	//Record file in ram disk
	if (record_type & RECORD_RX_DATA) {
		snprintf(ram_record_file_RX, 63, "%s_RX%03d.bin", file_name, recored_count);
		printf("record file name: %s\n\r", ram_record_file_RX);
		memset(path, 0, sizeof(path));
		snprintf(path, sizeof(path), "audio_ram:/%s", ram_record_file_RX);
		printf("record file name: %s\n\r", path);

		m_record_file_RX = fopen(path, "w");  // if open successfully, f_open will returns 0
		printf("open record file name: %s\n\r", path);
		if (!m_record_file_RX) {
			printf("open file (%s) fail.\n\r", path);
			record_state = 0;
			return;
		}
		printf("record file name: %s\n\n\r", path);
	}
	if (record_type & RECORD_TX_DATA) {
		snprintf(ram_record_file_TX, 63, "%s_TX%03d.bin", file_name, recored_count);
		printf("record file name: %s\n\r", ram_record_file_TX);
		memset(path, 0, sizeof(path));
		snprintf(path, sizeof(path), "audio_ram:/%s", ram_record_file_TX);
		printf("record file name: %s\n\r", path);

		m_record_file_TX = fopen(path, "w");  // if open successfully, f_open will returns 0
		printf("open record file name: %s\n\r", path);
		if (!m_record_file_TX) {
			printf("open file (%s) fail.\n\r", path);
			record_state = 0;
			return;
		}
		printf("record file name: %s\n\n\r", path);
	}
	if (record_type & RECORD_ASP_DATA) {
		snprintf(ram_record_file_ASP, 63, "%s_ASP%03d.bin", file_name, recored_count);
		printf("record file name: %s\n\r", ram_record_file_ASP);
		memset(path, 0, sizeof(path));
		snprintf(path, sizeof(path), "audio_ram:/%s", ram_record_file_ASP);
		printf("record file name: %s\n\r", path);

		m_record_file_ASP = fopen(path, "w");  // if open successfully, f_open will returns 0
		printf("open record file name: %s\n\r", path);
		if (!m_record_file_ASP) {
			printf("open file (%s) fail.\n\r", path);
			record_state = 0;
			return;
		}
		printf("record file name: %s\n\n\r", path);
	}
	record_state = 2;


	return  ;
}

void audio_close_record_file(void)
{
	int res;
	//close Record
	if (record_type & RECORD_RX_DATA) {
		res = fclose(m_record_file_RX);
		if (res) {
			printf("close file (%s) fail.\n\r", ram_record_file_RX);
		} else {
			printf("close file (%s) success.\n\r", ram_record_file_RX);
		}
	}
	if (record_type & RECORD_TX_DATA) {
		res = fclose(m_record_file_TX);
		if (res) {
			printf("close file (%s) fail.\n\r", ram_record_file_TX);
		} else {
			printf("close file (%s) success.\n\r", ram_record_file_TX);
		}
	}
	if (record_type & RECORD_ASP_DATA) {
		res = fclose(m_record_file_ASP);
		if (res) {
			printf("close file (%s) fail.\n\r", ram_record_file_ASP);
		} else {
			printf("close file (%s) success.\n\r", ram_record_file_ASP);
		}
	}
	if (sdsave_status & SD_SAVE_EN) {
		xSemaphoreGive(ram_dump_sd_sema);
		sdsave_status |= SD_SAVE_START;
	}
	return  ;
}

void audio_record_write_file_RX(int16_t *record)
{

	fwrite(record, 1, 2 * FRAME_LEN, m_record_file_RX);

	return ;
}

void audio_record_write_file_TX(int16_t *record)
{

	fwrite(record, 1, 2 * FRAME_LEN, m_record_file_TX);

	return ;
}

void audio_record_write_file_ASP(int16_t *record)
{

	fwrite(record, 1, 2 * FRAME_LEN, m_record_file_ASP);

	return ;
}

void audio_mic_record(int16_t *speaker_data_TX, int16_t *mic_data_RX, int16_t *mic_data_ASP)
{
	static int record_percent;
	if (record_state == 3) {
		audio_close_record_file();
		record_state = 0;
	} else if (record_state == 2) {
		if (frame_count > 0) {
			frame_count --;
			if (record_frame_count >= 100) {
				if (((record_frame_count - frame_count) / (record_frame_count / 100)) >= (record_percent + 1)) {
					record_percent = (record_frame_count - frame_count) / (record_frame_count / 100);
					rt_printf("*");
					if ((record_percent % 10) == 0) {
						rt_printf(" %d%% is done!\n\r", record_percent);
					}
				}
			}
			if (record_type & RECORD_RX_DATA) {
				audio_record_write_file_RX(mic_data_RX);
			}
			if (record_type & RECORD_TX_DATA) {
				audio_record_write_file_TX(speaker_data_TX);
			}
			if (record_type & RECORD_ASP_DATA) {
				audio_record_write_file_ASP(mic_data_ASP);
			}
		} else if (frame_count == 0) {
			record_state = 3;
		}
	} else if (record_state == 1) {
		audio_open_record_file();
		record_percent = 0;
	}
}

#include "gpio_api.h"
gpio_t gpio_amp;
int pin_initialed = 0;
int set_pin;

int pinconvert(char *pin_stream, int *amp_pin)
{
	char *pin_id_ptr;
	int amp_pin_id = 0;

	if (!strncmp("PA_", pin_stream, 3)) {
		pin_id_ptr = pin_stream + 3;
		amp_pin_id = atoi(pin_id_ptr);
		if (amp_pin_id >= 0 && amp_pin_id <= 5) {
			printf("Set pin PA_%d ", amp_pin_id);
			*amp_pin = PIN_NAME(PORT_A, amp_pin_id);
			return 1;
		} else {
			return 0;
		}
	} else if (!strncmp("PB_", pin_stream, 3)) {
		pin_id_ptr = pin_stream + 3;
		amp_pin_id = atoi(pin_id_ptr);
		if (amp_pin_id >= 0 && amp_pin_id <= 2) {
			printf("Set pin PB_%d ", amp_pin_id);
			*amp_pin = PIN_NAME(PORT_B, amp_pin_id);
			return 1;
		} else {
			return 0;
		}
	} else if (!strncmp("PC_", pin_stream, 3)) {
		pin_id_ptr = pin_stream + 3;
		amp_pin_id = atoi(pin_id_ptr);
		if (amp_pin_id >= 0 && amp_pin_id <= 5) {
			printf("Set pin PC_%d ", amp_pin_id);
			*amp_pin = PIN_NAME(PORT_C, amp_pin_id);
			return 1;
		} else {
			return 0;
		}
	} else if (!strncmp("PD_", pin_stream, 3)) {
		pin_id_ptr = pin_stream + 3;
		amp_pin_id = atoi(pin_id_ptr);
#if IS_CUT_TEST(CONFIG_CHIP_VER)
		if (amp_pin_id >= 0 && amp_pin_id <= 16) {
#else
		if (amp_pin_id >= 0 && amp_pin_id <= 20) {
#endif
			printf("Set pin PD_%d ", amp_pin_id);
			*amp_pin = PIN_NAME(PORT_D, amp_pin_id);
			return 1;
		} else {
			return 0;
		}
	} else if (!strncmp("PE_", pin_stream, 3)) {
		pin_id_ptr = pin_stream + 3;
		amp_pin_id = atoi(pin_id_ptr);
#if IS_CUT_TEST(CONFIG_CHIP_VER)
		if (amp_pin_id >= 0 && amp_pin_id <= 10) {
#else
		if (amp_pin_id >= 0 && amp_pin_id <= 6) {
#endif
			printf("Set pin PE_%d ", amp_pin_id);
			*amp_pin = PIN_NAME(PORT_E, amp_pin_id);
			return 1;
		} else {
			return 0;
		}
	} else if (!strncmp("PF_", pin_stream, 3)) {
		pin_id_ptr = pin_stream + 3;
		//printf("pin_id_ptr = %s\r\n", pin_id_ptr);
		amp_pin_id = atoi(pin_id_ptr);
		if (amp_pin_id >= 0 && amp_pin_id <= 17) {
			printf("Set pin PF_%d ", amp_pin_id);
			*amp_pin = PIN_NAME(PORT_F, amp_pin_id);
			return 1;
		} else {
			return 0;
		}
	} else if (!strncmp("PS_", pin_stream, 3)) {
		pin_id_ptr = pin_stream + 3;
		amp_pin_id = atoi(pin_id_ptr);
		if (amp_pin_id >= 0 && amp_pin_id <= 6) {
			printf("Set pin PS_%d ", amp_pin_id);
			*amp_pin = PIN_NAME(PORT_S, amp_pin_id);
			return 1;
		} else {
			return 0;
		}
	}
	return 0;
}

//********************//
//MIC setting function
//********************//

//Set the audio mic mode
void fAUMMODE(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	if (!arg) {
		printf("\n\r[AUMMODE] Set the mic mode: AUMMODE=[mic_mode]\n");

		printf("  \r     [mic_mode]=amic/l_dmic/r_dmic/stereo_dmic\n");
		printf("  \r     Default is Amic. \r\nSet Left Dmic by AUMMODE=l_dmic\r\nSet Right Dmic by AUMMODE=r_dmic\r\nSet Stereo Dmic by AUMMODE=stereo_dmic\n");
		return;
	}

	argc = parse_param(arg, argv);
	if (argc) {
		//printf("argc = %d\r\n", argc);
		if (strncmp(argv[1], "amic", strlen("amic")) == 0) {
			printf("Set A mic \r\n");
			audio_save_params.use_mic_type = USE_AUDIO_AMIC;
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
		} else if (strncmp(argv[1], "l_dmic", strlen("l_dmic")) == 0) { //Set the left dmic
			printf("Set Left D mic \r\n");
			audio_save_params.use_mic_type = USE_AUDIO_LEFT_DMIC;
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
		} else if (strncmp(argv[1], "r_dmic", strlen("r_dmic")) == 0) { //Set the right dmic
			printf("Set Right D mic \r\n");
			audio_save_params.use_mic_type = USE_AUDIO_RIGHT_DMIC;
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
		} else if (strncmp(argv[1], "stereo_dmic", strlen("stereo_dmic")) == 0) { //Set the stereo dmic
			printf("Set Stereo D mic \r\n");
			audio_save_params.use_mic_type = USE_AUDIO_STEREO_DMIC;
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
		} else {
			printf("Unknown mic type\r\n");
		}

	}
}

//Set mic gain
void fAUMG(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	if (!arg) {
		printf("\n\r[AUMG] Set up MIC GAIN: AUMG=[mic_gain]\n");

		printf("  \r     [mic_gain]=0~3\n");
		printf("  \r     0: 0dB 1: 20dB 2: 30dB 3: 40dB\n");
		printf("  \r     Set MIC Gain 0dB by AUMG=0\n");
		return;
	}

	argc = parse_param(arg, argv);
	if (argc) {
		printf("argc = %d\r\n", argc);
		mic_gain = atoi(argv[1]);
		if (mic_gain < 0 || mic_gain > 3) {
			mic_gain = 0;
			printf("invalid mic_gain set default value: %d\r\n", mic_gain);
		} else {
			printf("Set mic gain value: %d\r\n", mic_gain);
		}
		audio_save_params.mic_gain = mic_gain;
		mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
	}
}

//Set mic bias
void fAUMB(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	if (!arg) {
		printf("\n\r[AUMB] Set up MIC BIAS: AUMB=[mic_bias]\n");

		printf("  \r     [mic_bias]=0~2\n");
		printf("  \r     0: 0.9 1: 0.86 2: 0.75\n");
		printf("  \r     Set MIC BIAS 0.9 by AUMB=0\n");
		return;
	}

	argc = parse_param(arg, argv);
	if (argc) {
		printf("argc = %d\r\n", argc);
		mic_bias = atoi(argv[1]);
		if (mic_bias < 0 || mic_bias > 2) {
			mic_bias = 0;
			printf("invalid mic_bias set default value: %d\r\n", mic_bias);
		} else {
			printf("Set mic bias value: %d\r\n", mic_bias);
		}
		audio_save_params.mic_bias = mic_bias;
		mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
	}
}

//Set left dmic gain
void fAUMLG(void *arg)
{
	int argc = 0;
	int left_dmic_gain = 0;
	char *argv[MAX_ARGC] = {0};
	if (!arg) {
		printf("\n\r[AUMLG] Set up left dmic MIC GAIN: AUMLG=[left_dmic_gain]\n");

		printf("  \r     [left_dmic_gain]=0~3\n");
		printf("  \r     0: 0dB 1: 12dB 2: 24dB 3: 36dB\n");
		printf("  \r     Set LEFT DMIC Gain 0dB by AUMLG=0\n");
		return;
	}

	argc = parse_param(arg, argv);
	if (argc) {
		printf("argc = %d\r\n", argc);
		left_dmic_gain = atoi(argv[1]);
		if (left_dmic_gain < 0 || left_dmic_gain > 3) {
			left_dmic_gain = 0;
			printf("invalid left_dmic_gain set default value: %d\r\n", left_dmic_gain);
		} else {
			printf("Set left_dmic gain value: %d\r\n", left_dmic_gain);
		}
		audio_save_params.dmic_l_gain = left_dmic_gain;
		mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
	}
}

//Set right dmic gain
void fAUMRG(void *arg)
{
	int argc = 0;
	int right_dmic_gain = 0;
	char *argv[MAX_ARGC] = {0};
	if (!arg) {
		printf("\n\r[AUMRG] Set up right dmic MIC GAIN: AUMRG=[right_dmic_gain]\n");

		printf("  \r     [right_dmic_gain]=0~3\n");
		printf("  \r     0: 0dB 1: 12dB 2: 24dB 3: 36dB\n");
		printf("  \r     Set LEFT DMIC Gain 0dB by AUMRG=0\n");
		return;
	}

	argc = parse_param(arg, argv);
	if (argc) {
		printf("argc = %d\r\n", argc);
		right_dmic_gain = atoi(argv[1]);
		if (right_dmic_gain < 0 || right_dmic_gain > 3) {
			right_dmic_gain = 0;
			printf("invalid right_dmic_gain set default value: %d\r\n", right_dmic_gain);
		} else {
			printf("Set right_dmic gain value: %d\r\n", right_dmic_gain);
		}
		audio_save_params.dmic_r_gain = right_dmic_gain;
		mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
	}
}

//Set the mic ADC gain
void fAUADC(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};

	if (!arg) {
		printf("\n\r[AUADC] Set up ADC gain: AUADC=[ADC_gain]\n");

		printf("  \r     [ADC_gain]=0x00~0x7F\n");
		printf("  \r     ADC_gain need in hex= 0x7F: 30dB, 0x5F: 18dB, 0x2F: 0dB, 0x00: -17.625dB, 0.375/step\n");
		printf("  \r     Set ADC_gain 18dB by AUADC=0x5F\n");
		return;
	}

	argc = parse_param(arg, argv);

	if (argc) {
		ADC_gain = (int)strtol(argv[1], NULL, 16);
		printf("get ADC gain = %d, 0x%x\r\n", ADC_gain, ADC_gain);
		if (ADC_gain >= 0x00 && ADC_gain <= 0x7F) {
			printf("get ADC gain = %d, 0x%x\r\n", ADC_gain, ADC_gain);
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_ADC_GAIN, ADC_gain);
		} else {
			ADC_gain = 0x5F;
			printf("invalid ADC set default gain = %d, 0x%x\r\n", ADC_gain, ADC_gain);
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_ADC_GAIN, ADC_gain);

		}
		audio_save_params.ADC_gain = ADC_gain;
		mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
	}
}

//Set the mic HPF
void fAUHPF(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	if (!arg) {
		printf("\n\r[AUHPF] AUDIO HPF Usage: AUHPF=[cutoff num]\n");

		printf("  \r     [cutoff num]=0~8\n");
		printf("  \r     fc: cutoff frequency, fs: sample frequency \n");
		printf("  \r     fc ~= 5e-3 / (cutoff num + 1) * fs \n");
		printf("  \r     Set HPF by AUHPF=0\n");
		return;
	}

	argc = parse_param(arg, argv);
	if (argc) {
		printf("argc = %d\r\n", argc);
		hpf_set = atoi(argv[1]);
		if (hpf_set < 0 || hpf_set > 7) {
			hpf_set = 5;
			printf("invalid hpf_set set default value: %d\r\n", hpf_set);
		} else {
			printf("Set HPF value: %d\r\n", hpf_set);
		}
		audio_save_params.hpf_set = hpf_set;
		mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
	}
}

//set the left EQ for mic
void fAUMLEQ(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	uint8_t EQ_num = 0;
	if (!arg) {
		printf("\n\r[AUMLEQ] LEFT MIC EQ Usage: AUMLEQ=[eq num],[register h0],[register a1],[register a2],[register b1],[register b2]\n");
		printf("  \r[AUMLEQ] LEFT MIC EQ DISABLE Usage: AUMLEQ=[eq num]\n");
		printf("  \r     Enter the register coefficients obtained from biquad calculator\n");

		printf("  \r     [eq num]=0~4\n");
		printf("  \r     [register]=<num in hex> \n");
		printf("  \r     OPEN EQ by AUMLEQ=0,0x01,0x02,0x03,x04,0x05\n");
		printf("  \r     CLOSE EQ by AUMLEQ=0\n");
		return;
	}

	argc = parse_param(arg, argv);
	if (argc == 7) {
		EQ_num = atoi(argv[1]);
		if (EQ_num >= 0 && EQ_num <= 4) {
			printf("ENABLE LEFT DMIC OR AMIC EQ: %d \r\n", EQ_num);
			audio_save_params.mic_l_eq[EQ_num].eq_enable = 1;
			audio_save_params.mic_l_eq[EQ_num].eq_h0 = (int)strtol(argv[2], NULL, 16);
			audio_save_params.mic_l_eq[EQ_num].eq_a1 = (int)strtol(argv[3], NULL, 16);
			audio_save_params.mic_l_eq[EQ_num].eq_a2 = (int)strtol(argv[4], NULL, 16);
			audio_save_params.mic_l_eq[EQ_num].eq_b1 = (int)strtol(argv[5], NULL, 16);
			audio_save_params.mic_l_eq[EQ_num].eq_b2 = (int)strtol(argv[6], NULL, 16);
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
		} else {
			printf(" Set Wrong EQ: %d Not between 0~4\r\n", EQ_num);
		}
	} else if (argc == 2) {
		EQ_num = atoi(argv[1]);
		if (EQ_num >= 0 && EQ_num <= 4) {
			printf("DISABLE LEFT DMIC OR AMIC EQ: %d \r\n", EQ_num);
			audio_save_params.mic_l_eq[EQ_num].eq_enable = 0;
			audio_save_params.mic_l_eq[EQ_num].eq_h0 = 0;
			audio_save_params.mic_l_eq[EQ_num].eq_a1 = 0;
			audio_save_params.mic_l_eq[EQ_num].eq_a2 = 0;
			audio_save_params.mic_l_eq[EQ_num].eq_b1 = 0;
			audio_save_params.mic_l_eq[EQ_num].eq_b2 = 0;
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
		} else {
			printf(" Set Wrong EQ: %d Not between 0~4\r\n", EQ_num);
		}
	} else {
		printf(" argument number = %d \r\n", argc - 1);
		printf(" MISS some arguments AUMLEQ=[eq num],[register h0],[register a1],[register a2],[register b1],[register b2]\r\n");
	}
}

//set the right EQ for mic
void fAUMREQ(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	uint8_t EQ_num = 0;
	if (!arg) {
		printf("\n\r[AUMREQ] RIGHT MIC EQ Usage: AUMREQ=[eq num],[register h0],[register a1],[register a2],[register b1],[register b2]\n");
		printf("  \r[AUMREQ] RIGHT MIC EQ DISABLE Usage: AUMREQ=[eq num]\n");
		printf("  \r     Enter the register coefficients obtained from biquad calculator\n");

		printf("  \r     [eq num]=0~4\n");
		printf("  \r     [register]=<num in hex> \n");
		printf("  \r     OPEN EQ by AUMREQ=0,0x01,0x02,0x03,x04,0x05\n");
		printf("  \r     CLOSE EQ by AUMREQ=0\n");
		return;
	}

	argc = parse_param(arg, argv);
	if (argc == 7) {
		EQ_num = atoi(argv[1]);
		if (EQ_num >= 0 && EQ_num <= 4) {
			printf("ENABLE RIGHT DMIC EQ: %d \r\n", EQ_num);
			audio_save_params.mic_r_eq[EQ_num].eq_enable = 1;
			audio_save_params.mic_r_eq[EQ_num].eq_h0 = (int)strtol(argv[2], NULL, 16);
			audio_save_params.mic_r_eq[EQ_num].eq_a1 = (int)strtol(argv[3], NULL, 16);
			audio_save_params.mic_r_eq[EQ_num].eq_a2 = (int)strtol(argv[4], NULL, 16);
			audio_save_params.mic_r_eq[EQ_num].eq_b1 = (int)strtol(argv[5], NULL, 16);
			audio_save_params.mic_r_eq[EQ_num].eq_b2 = (int)strtol(argv[6], NULL, 16);
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
		} else {
			printf(" Set Wrong EQ: %d Not between 0~4\r\n", EQ_num);
		}
	} else if (argc == 2) {
		EQ_num = atoi(argv[1]);
		if (EQ_num >= 0 && EQ_num <= 4) {
			printf("DISABLE RIGHT DMIC EQ: %d \r\n", EQ_num);
			audio_save_params.mic_r_eq[EQ_num].eq_enable = 0;
			audio_save_params.mic_r_eq[EQ_num].eq_h0 = 0;
			audio_save_params.mic_r_eq[EQ_num].eq_a1 = 0;
			audio_save_params.mic_r_eq[EQ_num].eq_a2 = 0;
			audio_save_params.mic_r_eq[EQ_num].eq_b1 = 0;
			audio_save_params.mic_r_eq[EQ_num].eq_b2 = 0;
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
		} else {
			printf(" Set Wrong EQ: %d Not between 0~4\r\n", EQ_num);
		}
	} else {
		printf(" argument number = %d \r\n", argc - 1);
		printf(" MISS some arguments AUMREQ=[eq num],[register h0],[register a1],[register a2],[register b1],[register b2]\r\n");
	}
}

//Set the AEC
void fAUAEC(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	if (!arg) {
		printf("\n\r[AUAEC] Enable AEC or not: AUAEC=[enable]\n");

		printf("  \r     [enable]=0 or 1\n");
		printf("  \r     OPEN AEC by AUAEC=1\n");
		printf("  \r     CLOSE AEC by AUAEC=0\n");
		return;
	}

	argc = parse_param(arg, argv);
	if (argc) {
		printf("argc = %d\r\n", argc);
		if (atoi(argv[1]) == 0) {
			audio_save_params.enable_aec = 0;
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
		} else {
			audio_save_params.enable_aec = 1;
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
		}
	}
}

//Set mic NS level
void fAUNS(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};

	if (!arg) {
		printf("\n\r[AUNS] Set up NS module: AUNS=[NS_level]\n");

		printf("  \r     [NS_level]=-1~3\n");
		printf("  \r     NS_level=-1 to disable NS\n");
		printf("  \r     NS_level=0~3 to set the NS level, more NS level more aggressive\n");
		printf("  \r     Set mic NS by AUNS=0\n");
		printf("  \r     Disable mic NS by AUNS=-1\n");
		return;
	}

	argc = parse_param(arg, argv);

	if (argc) {
		NS_level = atoi(argv[1]);
		if (NS_level < 0 || NS_level > 3) {
			NS_level = -1;
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);

		} else {
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
		}
		audio_save_params.NS_level = NS_level;
		mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
	}
}

//********************//
//Speaker setting function
//********************//
//Set speaker NS level
void fAUSPNS(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};

	if (!arg) {
		printf("\n\r[AUSPNS] Set up NS module: AUSPNS=[NS_level_SPK]\n");

		printf("  \r     [NS_level_SPK]=-1~3\n");
		printf("  \r     NS_level_SPK=-1 to disable NS\n");
		printf("  \r     NS_level_SPK=0~3 to set the NS level, more NS level more aggressive\n");
		printf("  \r     Set speaker NS by AUSPNS=0\n");
		printf("  \r     Disable speaker NS by AUSPNS=-1\n");
		return;
	}

	argc = parse_param(arg, argv);

	if (argc) {
		NS_level_SPK = atoi(argv[1]);
		if (NS_level_SPK < 0 || NS_level_SPK > 3) {
			NS_level_SPK = -1;
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);

		} else {
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
		}
		audio_save_params.NS_level_SPK = NS_level_SPK;
		mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
	}
}

//Set the speaker EQ
void fAUSPEQ(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	uint8_t EQ_num = 0;
	if (!arg) {
		printf("\n\r[AUSPEQ] Speaker EQ Usage: AUSPEQ=[eq num],[register h0],[register a1],[register a2],[register b1],[register b2]\n");
		printf("  \r[AUSPEQ] Speaker EQ DISABLE Usage: AUSPEQ=[eq num]\n");
		printf("  \r     Enter the register coefficients obtained from biquad calculator\n");

		printf("  \r     [eq num]=0~4\n");
		printf("  \r     [register]=<num in hex> \n");
		printf("  \r     OPEN EQ by AUSPEQ=0,0x01,0x02,0x03,x04,0x05\n");
		printf("  \r     CLOSE EQ by AUSPEQ=0\n");
		return;
	}

	argc = parse_param(arg, argv);
	if (argc == 7) {
		EQ_num = atoi(argv[1]);
		if (EQ_num >= 0 && EQ_num <= 4) {
			printf("ENABLE EQ: %d \r\n", EQ_num);
			audio_save_params.spk_l_eq[EQ_num].eq_enable = 1;
			audio_save_params.spk_l_eq[EQ_num].eq_h0 = (int)strtol(argv[2], NULL, 16);
			audio_save_params.spk_l_eq[EQ_num].eq_a1 = (int)strtol(argv[3], NULL, 16);
			audio_save_params.spk_l_eq[EQ_num].eq_a2 = (int)strtol(argv[4], NULL, 16);
			audio_save_params.spk_l_eq[EQ_num].eq_b1 = (int)strtol(argv[5], NULL, 16);
			audio_save_params.spk_l_eq[EQ_num].eq_b2 = (int)strtol(argv[6], NULL, 16);
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
		} else {
			printf(" Set Wrong EQ: %d Not between 0~4\r\n", EQ_num);
		}
	} else if (argc == 2) {
		EQ_num = atoi(argv[1]);
		if (EQ_num >= 0 && EQ_num <= 4) {
			printf("DISABLE EQ: %d \r\n", EQ_num);
			audio_save_params.spk_l_eq[EQ_num].eq_enable = 0;
			audio_save_params.spk_l_eq[EQ_num].eq_h0 = 0;
			audio_save_params.spk_l_eq[EQ_num].eq_a1 = 0;
			audio_save_params.spk_l_eq[EQ_num].eq_a2 = 0;
			audio_save_params.spk_l_eq[EQ_num].eq_b1 = 0;
			audio_save_params.spk_l_eq[EQ_num].eq_b2 = 0;
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
		} else {
			printf(" Set Wrong EQ: %d Not between 0~4\r\n", EQ_num);
		}
	} else {
		printf(" argument number = %d \r\n", argc - 1);
		printf(" MISS some arguments AUSPEQ=[eq num],[register h0],[register a1],[register a2],[register b1],[register b2]\r\n");
	}
}

//Set the speaker DAC gain
void fAUDAC(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};

	if (!arg) {
		printf("\n\r[AUDAC] Set up DAC gain: AUDAC=[DAC_gain]\n");

		printf("  \r     [DAC_gain]=0x00~0x7F\n");
		printf("  \r     DAC_gain need in hex= 0xAF: 0dB, 0x87: -15dB, 0x00: -65dB, 0.375/step\n");
		printf("  \r     Set DAC_gain -15dB by AUADC=0x87\n");
		return;
	}

	argc = parse_param(arg, argv);

	if (argc) {
		//ADC_gain = atohex(argv[1]);
		DAC_gain = (int)strtol(argv[1], NULL, 16);
		printf("get DAC gain = %d, 0x%x\r\n", DAC_gain, DAC_gain);
		if (DAC_gain >= 0x00 && DAC_gain <= 0xAF) {
			printf("get DAC gain = %d, 0x%x\r\n", DAC_gain, DAC_gain);
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_DAC_GAIN, DAC_gain);
		} else {
			DAC_gain = 0xAF;
			printf("invalid DAC set default gain = %d, 0x%x\r\n", DAC_gain, DAC_gain);
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_DAC_GAIN, DAC_gain);
		}
		audio_save_params.DAC_gain = DAC_gain;
		mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
	}
}

//Set speaker mute
void fAUSPM(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	int spk_mute = 0;
	if (!arg) {
		printf("\n\r[AUSPM] Mute SPEAKER or not: AUSPM=[enable_mute]\n");

		printf("  \r     [enable_mute]=0 or 1\n");
		printf("  \r     MUTE SPEAKER by AUSPM=1\n");
		printf("  \r     UNMUTE SPEAKER by AUSPM=0\n");
		return;
	}

	argc = parse_param(arg, argv);
	if (argc) {
		printf("argc = %d\r\n", argc);
		spk_mute = atoi(argv[1]);
		if (spk_mute == 0) {
			printf("Disable Speaker Mute\r\n");
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_SPK_ENABLE, 1);
		} else {
			printf("Enable Speaker Mute\r\n");
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_SPK_ENABLE, 0);
		}
	}
}

//Set speaker tx mode
void fAUTXMODE(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	array_t array;

	if (!arg) {
		printf("\n\r[AUTXMODE] Mute SPEAKER or not: AUTXMODE=[tx_mode],[audio_tone(Hz)]\n");

		printf("  \r     [tx_mode]=playmusic/playback/playtone/playmusic\n");
		printf("  \r     [audio_tone(Hz)]=audio tone for play tone mode, only for play tone mode. Default is 1k\n");
		printf("  \r     playmusic mode by AUTXMODE=playmusic\n");
		printf("  \r     playback mode by AUTXMODE=playback\n");
		printf("  \r     playmusic mode by AUTXMODE=playmusic\n");
		printf("  \r     playtone mode 2K tone by AUTXMODE=playtone,2000\n");
		printf("  \r     playtone mode (default 1k) by AUTXMODE=playtone\n");


		printf("  \r     no mode by AUTXMODE=noplay\n");
		return;
	}
	//reset array data
	argc = parse_param(arg, argv);
	if (argc) {
		printf("argc = %d\r\n", argc);

		if (strcmp(argv[1], "playmusic") == 0) {
			if (tx_mode != 3) {
				if (playing_sample_rate <= 16000) {
					printf("Set playmusic mode\r\n");
					mm_module_ctrl(array_pcm_ctx, CMD_ARRAY_STREAMING, 0);	// streamming off
					mm_module_ctrl(pcm_tone_ctx, CMD_TONE_STREAMING, 0);	// streamming off
					//printf("mm_module_ctrl(array_pcm_ctx, CMD_ARRAY_STREAMING, 0)\r\n");
					mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_TRX, 0);
					//printf("mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_TRX, 0)\r\n");

					//set the array audio data
					if (playing_sample_rate == 16000) {
						array.data_addr = (uint32_t) music_sr16k;
						array.data_len = (uint32_t) music_sr16k_pcm_len;
						pcm16k_array_params.u.a.samplerate = 16000;
					} else if (playing_sample_rate == 8000) {
						array.data_addr = (uint32_t) music_sr8k;
						array.data_len = (uint32_t) music_sr8k_pcm_len;
						pcm16k_array_params.u.a.samplerate = 8000;
					}
					mm_module_ctrl(array_pcm_ctx, CMD_ARRAY_SET_PARAMS, (int)&pcm16k_array_params);
					mm_module_ctrl(array_pcm_ctx, CMD_ARRAY_SET_ARRAY, (int)&array);
					mm_module_ctrl(array_pcm_ctx, CMD_ARRAY_RECOUNT_PERIOD, 0);
					vTaskDelay(10);
					mimo_resume(mimo_aarray_audio);
					//printf("mimo_resume(mimo_aarray_audio)\r\n");
					mimo_pause(mimo_aarray_audio, MM_OUTPUT1 | MM_OUTPUT3);
					//printf("mimo_pause(mimo_aarray_audio, MM_OUTPUT1)\r\n");
					mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_DAC_GAIN, 0x00);
					mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_TRX, 1);
					mm_module_ctrl(array_pcm_ctx, CMD_ARRAY_STREAMING, 1);	// streamming on
					vTaskDelay(40);
					mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_DAC_GAIN, DAC_gain);

					tx_mode = 3;
				} else {
					printf("Playmusic mode is not supported sample rate: %d \r\n", playing_sample_rate);
				}
			} else {
				printf("Now is in playmusic mode\r\n");
			}
		} else if (strcmp(argv[1], "playback") == 0) {
			if (tx_mode != 2) {
				printf("Change playback mode\r\n");
				mm_module_ctrl(array_pcm_ctx, CMD_ARRAY_STREAMING, 0);	// streamming off
				mm_module_ctrl(pcm_tone_ctx, CMD_TONE_STREAMING, 0);	// streamming off
				//printf("mm_module_ctrl(array_pcm_ctx, CMD_ARRAY_STREAMING, 0)\r\n");
				mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_TRX, 0);
				//printf("mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_TRX, 0)\r\n");
				vTaskDelay(10);
				mimo_resume(mimo_aarray_audio);
				//printf("mimo_resume(mimo_aarray_audio)\r\n");
				mimo_pause(mimo_aarray_audio, MM_OUTPUT0 | MM_OUTPUT2 | MM_OUTPUT3);
				//printf("mimo_pause(mimo_aarray_audio, MM_OUTPUT0 | MM_OUTPUT2)\r\n");
				mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_DAC_GAIN, 0x00);
				mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_TRX, 1);
				vTaskDelay(40);
				mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_DAC_GAIN, DAC_gain);

				tx_mode = 2;
			} else {
				printf("Now is in playback mode\r\n");
			}
		} else if (strcmp(argv[1], "playtone") == 0) {
			printf("Set playtone mode\r\n");
			mm_module_ctrl(array_pcm_ctx, CMD_ARRAY_STREAMING, 0);	// streamming off
			mm_module_ctrl(pcm_tone_ctx, CMD_TONE_STREAMING, 0);	// streamming off
			//printf("mm_module_ctrl(array_pcm_ctx, CMD_ARRAY_STREAMING, 0)\r\n");
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_TRX, 0);
			//printf("mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_TRX, 0)\r\n");

			if (argv[2]) {
				mm_module_ctrl(pcm_tone_ctx, CMD_TONE_SET_AUDIOTONE, atoi(argv[2]));
			} else {
				mm_module_ctrl(pcm_tone_ctx, CMD_TONE_SET_AUDIOTONE, 1000);
			}

			vTaskDelay(10);
			mimo_resume(mimo_aarray_audio);
			//printf("mimo_resume(mimo_aarray_audio)\r\n");
			mimo_pause(mimo_aarray_audio, MM_OUTPUT1 | MM_OUTPUT2);
			//printf("mimo_pause(mimo_aarray_audio, MM_OUTPUT1)\r\n");
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_DAC_GAIN, 0x00);
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_TRX, 1);
			mm_module_ctrl(pcm_tone_ctx, CMD_TONE_STREAMING, 1);	// streamming on
			vTaskDelay(40);
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_DAC_GAIN, DAC_gain);

			tx_mode = 1;
		} else if (strcmp(argv[1], "noplay") == 0) {
			if (tx_mode != 0) {
				printf("Set noplay mode\r\n");
				mm_module_ctrl(array_pcm_ctx, CMD_ARRAY_STREAMING, 0);	// streamming off
				mm_module_ctrl(pcm_tone_ctx, CMD_TONE_STREAMING, 0);	// streamming off
				//printf("mm_module_ctrl(array_pcm_ctx, CMD_ARRAY_STREAMING, 0)\r\n");
				mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_SPK_ENABLE, 0);
				mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_TRX, 0);
				//printf("mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_TRX, 0)\r\n");
				vTaskDelay(10);
				mimo_resume(mimo_aarray_audio);
				//printf("mimo_resume(mimo_aarray_audio)\r\n");
				mimo_pause(mimo_aarray_audio, MM_OUTPUT1 | MM_OUTPUT2 | MM_OUTPUT3); //enable audio playtone
				//printf("mimo_pause(mimo_aarray_audio, MM_OUTPUT1 | MM_OUTPUT2)\r\n");
				mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_DAC_GAIN, 0x00);
				mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_TRX, 1);
				vTaskDelay(40);
				mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_DAC_GAIN, DAC_gain);

				tx_mode = 0;
			} else {
				printf("Now is in noplay mode\r\n");
			}
		} else {
			if (tx_mode == 3) {
				printf("Unknown mode keep playmusic mode\r\n");
			} else if (tx_mode == 2) {
				printf("Unknown mode keep playback mode\r\n");
			} else if (tx_mode == 1) {
				printf("Unknown mode keep playtone mode\r\n");
			} else {
				printf("Unknown mode keep noplay mode\r\n");
			}
		}
	}
}

//Set speaker amplifier pin
extern void gpio_deinit(gpio_t *obj);
void fAUAMPIN(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	int res = 0;
	int a_amp_pin;


	if (!arg) {
		printf("\n\r[AUAMPIN] set amp pin on or not: AUAMPIN=[pin_name],[on/off]\n");

		printf("  \r     [pin_name]=pin name\n");
		printf("  \r     [on/off]=1/0\n");
		printf("  \r     Set PF_15 on by AUAMPIN=PF_15,1\n");
		printf("  \r     Set PF_15 off by AUAMPIN=PF_15,0\n");
		return;
	}

	argc = parse_param(arg, argv);
	if (argc) {
		printf("argc = %d\r\n", argc);
		res = pinconvert(argv[1], &a_amp_pin);
		if (res) {
			// Init LED control pin
			if (pin_initialed == 0) {
				gpio_init(&gpio_amp, a_amp_pin);
				pin_initialed = 1;
				set_pin = a_amp_pin;
			}
			if (set_pin == a_amp_pin) {
				gpio_dir(&gpio_amp, PIN_OUTPUT);        // Direction: Output
				gpio_mode(&gpio_amp, PullNone);         // No pull
				gpio_write(&gpio_amp, !!atoi(argv[2]));
				if (!!atoi(argv[2])) {
					printf("to ON \r\n");
				} else {
					printf("to OFF \r\n");
				}
			} else {
				if (!!atoi(argv[2])) {
					printf("to ON \r\n");
				} else {
					printf("to OFF \r\n");
				}
				printf("Not the same. Deinit previous pin\r\n");
				gpio_deinit(&gpio_amp);
				gpio_init(&gpio_amp, a_amp_pin);
				gpio_dir(&gpio_amp, PIN_OUTPUT);        // Direction: Output
				gpio_mode(&gpio_amp, PullNone);         // No pull
				gpio_write(&gpio_amp, !!atoi(argv[2]));
				pin_initialed = 1;
				set_pin = a_amp_pin;
			}
			//printf("\r\na_amp_pin = %d\r\n", a_amp_pin);

		} else {
			printf("Unkown pin\r\n");
		}
	}
}

//********************//
//Audio setting function
//********************//
//Set audio sample rate
void fAUSR(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	if (!arg) {
		printf("\n\r[AUSR] Set up SAMPLE RATE: AUSR=[sample_rate]\n");

		printf("  \r     [sample_rate]=8000,16000,32000,44100,48000,88200,96000\n");
		printf("  \r     sample rate = 8000, 16000\n");
		printf("  \r     Set SAMPLE RATE 8000 by AUSR=8000\n");
		return;
	}

	argc = parse_param(arg, argv);
	if (argc) {
		printf("argc = %d\r\n", argc);
		the_sample_rate = atoi(argv[1]);
		if (the_sample_rate == 96000) {
			printf("Set sample rate 96K\r\n");
			audio_save_params.sample_rate = ASR_96KHZ;
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
		} else if (the_sample_rate == 88200) {
			printf("Set sample rate 88.2K\r\n");
			audio_save_params.sample_rate = ASR_88p2KHZ;
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
		} else if (the_sample_rate == 48000) {
			printf("Set sample rate 48K\r\n");
			audio_save_params.sample_rate = ASR_48KHZ;
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
		} else if (the_sample_rate == 44100) {
			printf("Set sample rate 44.1K\r\n");
			audio_save_params.sample_rate = ASR_44p1KHZ;
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
		} else if (the_sample_rate == 32000) {
			printf("Set sample rate 32K\r\n");
			audio_save_params.sample_rate = ASR_32KHZ;
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
		} else if (the_sample_rate == 16000) {
			printf("Set sample rate 16K\r\n");
			audio_save_params.sample_rate = ASR_16KHZ;
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
		} else if (the_sample_rate == 8000) {
			printf("Set sample rate 8K\r\n");
			audio_save_params.sample_rate = ASR_8KHZ;
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
		} else {
			printf("Set sample rate 8K\r\n");
			audio_save_params.sample_rate = ASR_8KHZ;
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_PARAMS, (int)&audio_save_params);
			the_sample_rate = 8000;
		}
	}
}

//Set audio TRX enable
void fAUTRX(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	if (!arg) {
		printf("\n\r[AUTRX] Enable TRX or not: AUTRX=[enable]\n");

		printf("  \r     [enable]=0 or 1\n");
		printf("  \r     OPEN TRX by AUTRX=1\n");
		printf("  \r     CLOSE TRX by AUTRX=0\n");
		return;
	}

	argc = parse_param(arg, argv);
	if (argc) {
		if (atoi(argv[1]) == 0) {
			if (tx_mode == 3) {
				mm_module_ctrl(array_pcm_ctx, CMD_ARRAY_STREAMING, 0);	// streamming off
			} else if (tx_mode == 1) {
				mm_module_ctrl(pcm_tone_ctx, CMD_TONE_STREAMING, 0);	// streamming off
			}
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_TRX, 0);

		} else {
			if (tx_mode == 3) {
				mm_module_ctrl(array_pcm_ctx, CMD_ARRAY_STREAMING, 1);	// streamming on
			} else if (tx_mode == 1) {
				mm_module_ctrl(pcm_tone_ctx, CMD_TONE_STREAMING, 1);	// streamming on
			}
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_TRX, 1);
		}
	}
}

void fAURES(void *arg)
{
	//int argc = 0;
	//char *argv[MAX_ARGC] = {0};
	if (!arg) {
		printf("\n\r[AURES] Reset the audio for setting the previous audio configuration: AURES=1\n");

		printf("  \r     Reset Audio by AURES=1\n");
		return;
	}
	//argc = parse_param(arg, argv);
	printf("Reset audio flow \r\n");
	reset_flag = 1;

	//choose new array data
	array_t array;
	if (the_sample_rate == 16000) {
		if (tx_mode == 3) {//play music mode
			array.data_addr = (uint32_t) music_sr16k;
			array.data_len = (uint32_t) music_sr16k_pcm_len;
		}
		pcm16k_array_params.u.a.samplerate = 16000;
	} else if (the_sample_rate == 8000) {
		if (tx_mode == 3) {//play music mode
			array.data_addr = (uint32_t) music_sr8k;
			array.data_len = (uint32_t) music_sr8k_pcm_len;
		}
		pcm16k_array_params.u.a.samplerate = 8000;
	}
	//reset array data
	mm_module_ctrl(array_pcm_ctx, CMD_ARRAY_STREAMING, 0);	// streamming off
	mm_module_ctrl(array_pcm_ctx, CMD_ARRAY_SET_PARAMS, (int)&pcm16k_array_params);
	mm_module_ctrl(array_pcm_ctx, CMD_ARRAY_SET_ARRAY, (int)&array);
	mm_module_ctrl(array_pcm_ctx, CMD_ARRAY_RECOUNT_PERIOD, 0);

	//reset audio
	mm_module_ctrl(pcm_tone_ctx, CMD_TONE_STREAMING, 0);	// streamming off
	mm_module_ctrl(pcm_tone_ctx, CMD_TONE_SET_SAMPLERATE, the_sample_rate);
	mm_module_ctrl(pcm_tone_ctx, CMD_TONE_RECOUNT_PERIOD, 0);
	mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_RESET, 0);

	//restart if play music mode
	if (tx_mode == 3) {
		if (the_sample_rate <= 16000) {
			mm_module_ctrl(array_pcm_ctx, CMD_ARRAY_STREAMING, 1);	// streamming on
		} else {
			printf("playmusic ot playtone mode are not support in sample rate %d. Set to no play mode\r\n", the_sample_rate);
			mm_module_ctrl(array_pcm_ctx, CMD_ARRAY_STREAMING, 0);	// streamming off
			//printf("mm_module_ctrl(array_pcm_ctx, CMD_ARRAY_STREAMING, 0)\r\n");
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_SPK_ENABLE, 0);
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_TRX, 0);
			//printf("mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_TRX, 0)\r\n");
			vTaskDelay(10);
			mimo_resume(mimo_aarray_audio);
			//printf("mimo_resume(mimo_aarray_audio)\r\n");
			mimo_pause(mimo_aarray_audio, MM_OUTPUT1 | MM_OUTPUT2); //enable audio playtone
			//printf("mimo_pause(mimo_aarray_audio, MM_OUTPUT1 | MM_OUTPUT2)\r\n");
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_DAC_GAIN, 0x00);
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_TRX, 1);
			vTaskDelay(40);
			mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_DAC_GAIN, DAC_gain);

			tx_mode = 0;
		}
	} else if (tx_mode == 1) { //if play tone mode
		mm_module_ctrl(pcm_tone_ctx, CMD_TONE_STREAMING, 1);	// streamming off
	}
	playing_sample_rate = the_sample_rate;

	if (tx_mode != 2) {
		mm_module_ctrl(afft_test_ctx, CMD_AFFT_SAMPLERATE, playing_sample_rate);
	}
	//reset the audio ADC and DAC gain
	mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_ADC_GAIN, ADC_gain);
	mm_module_ctrl(audio_save_ctx, CMD_AUDIO_SET_DAC_GAIN, DAC_gain);
}

//Set show the mic audio fft result in function
void fAUFFTS(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};

	if (!arg) {
		printf("\n\r[AUFFTS] Enable Print FFT result (playback mode will not print): AUFFTS=[FFT_EN]\n");

		printf("  \r     [FFT_EN]=0 or 1\n");
		printf("  \r     Enable Print FFT result (playback mode will not print) by AUFFTS=1\n");
		return;
	}

	argc = parse_param(arg, argv);

	if (argc) {
		if (atoi(argv[1])) {
			printf("Start FFT Result Print\r\n");
			audio_fft_show = 1;
			mm_module_ctrl(afft_test_ctx, CMD_AFFT_SHOWN, 1);
		} else {
			printf("Stop FFT Result Print\r\n");
			audio_fft_show = 0;
			mm_module_ctrl(afft_test_ctx, CMD_AFFT_SHOWN, 0);
		}
	}
}

//********************//
//Audio record function
//********************//
//Set audio save file name
void fAUFILE(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};

	if (!arg) {
		printf("\n\r[AUFILE] Set up record file name: AUFILE=[filename]\n");

		printf("  \r     [filename] length needs to smaller than 20\n");
		printf("  \r     Set FILE NAME by AUFILE=AUDIO_TEST\n");
		return;
	}

	argc = parse_param(arg, argv);

	if (argc) {
		memset(file_name, 0, 20);
		strncpy(file_name, argv[1], 20);
	}
}

//record file time
void fAUREC(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	int rtime = 0;
	int p_min = 0;
	int p_sec = 0;
	int p_msec = 0;

	if (!arg) {
		printf("\n\r[fAUREC] SET AUDIO RECORD TIME: AUREC=[record_time],[RECORD_TYPE1],[RECORD_TYPE2],[RECORD_TYPE3]\n");

		printf("  \r     %d <= [record_time] <= %d in ms\n", MIN_RECORD_TIME, MAX_RECORD_TIME);
		printf("  \r     [RECORD_TYPE] = RX, TX, ASP\n");
		printf("  \r     RX: record mic RX data before ASP, TX: speaker data, ASP: mic data after ASP\n");
		printf("  \r     if [RECORD_TYPE] not set, it will record mic RX data\n");
		printf("  \r     record 1 min data fAUREC=600000\n");
		printf("  \r     record 1 min data for RX, TX, ASP fAUREC=600000,RX,TX,ASP\n");
		return;
	}
	argc = parse_param(arg, argv);
	rtime = atoi(argv[1]);
	if (record_state == 0 || (sdsave_status & SD_SAVE_START)) {
		if (argc) {
			record_type = 0;
			if (argc > 2) {
				for (int i = 2; i < argc; i++) {
					if (strncmp(argv[i], "RX", 2) == 0) {
						printf("Record mic RX DATA\r\n");
						record_type |= RECORD_RX_DATA;
					} else if (strncmp(argv[i], "TX", 2) == 0) {
						printf("Record mic TX DATA\r\n");
						record_type |= RECORD_TX_DATA;
					} else if (strncmp(argv[i], "ASP", 3) == 0) {
						printf("Record mic ASP DATA\r\n");
						record_type |= RECORD_ASP_DATA;
					}
				}
			}

			if (record_type == 0) {
				printf("Default Record mic RX DATA\r\n");
				record_type |= RECORD_RX_DATA;
			}
			if (rtime > MAX_RECORD_TIME) {
				printf("The record time > MAX_RECORD_TIME(%d), set to max record time \r\n", MAX_RECORD_TIME);
				rtime = MAX_RECORD_TIME;
			} else if (rtime < MIN_RECORD_TIME) {
				printf("The record time > MIN_RECORD_TIME(%d), set to min record time \r\n", MIN_RECORD_TIME);
				rtime = MIN_RECORD_TIME;
			} else {
				printf("Set the record time (%d) \r\n", rtime);
			}
			p_min = rtime / 60000;
			p_sec = (rtime - p_min * 60000) / 1000;
			p_msec = rtime - p_min * 60000 - p_sec * 1000;

			printf("Record %d m %d s %d ms length data\r\n", p_min, p_sec, p_msec);
			//count the number of frame need to save
			frame_count = rtime / (FRAME_LEN / (the_sample_rate / 1000));
			record_frame_count = frame_count;

			printf("Sample rate: %dkHZ, frame length: %d\r\n", the_sample_rate / 1000, frame_count);
			record_state = 1;
		}
	} else {
		printf("previous record not end\r\n");
	}
}

void fAUSDL(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	if (!arg) {
		printf("\n\r[AUSDL] download to sd or not: AUSDL=[enable_sd_download]\n");

		printf("  \r     [enable_sd_download]=0 or 1\n");
		printf("  \r     enable download by AUSDL=1\n");
		printf("  \r     disable download by AUSDL=0\n");
		return;
	}

	argc = parse_param(arg, argv);
	if (argc) {
		printf("argc = %d\r\n", argc);
		if (atoi(argv[1])) {
			sdsave_status |= SD_SAVE_EN;
			sdsave_status &= ~SD_SAVE_START;
			printf("set up SD DUMP every_record\r\n");
		} else {
			sdsave_status &= ~SD_SAVE_EN;
			sdsave_status &= ~SD_SAVE_START;
			printf("disable SD DUMP every_record\r\n");
		}
	}
}


log_item_t at_audio_save_items[ ] = {
	//For Audio mic
	{"AUMMODE",	fAUMMODE, 	{NULL, NULL}}, //set the mic mode
	{"AUMG",	fAUMG,  	{NULL, NULL}}, //adjust analog mic gain
	{"AUMB",	fAUMB,  	{NULL, NULL}}, //adjust analog mic bias
	{"AUMLG",	fAUMLG, 	{NULL, NULL}}, //adjust left digital mic gain
	{"AUMRG",	fAUMRG, 	{NULL, NULL}}, //adjust right digital mic gain
	{"AUADC",	fAUADC, 	{NULL, NULL}}, //adjust the ADC gain for mic
	{"AUHPF",	fAUHPF, 	{NULL, NULL}}, //adjust the mic HPF (this HPF is before EQ)
	{"AUMLEQ",	fAUMLEQ,	{NULL, NULL}}, //adjust the left mic (analog mic) EQ
	{"AUMREQ",	fAUMREQ,	{NULL, NULL}}, //adjust the right mic EQ
	{"AUAEC",	fAUAEC, 	{NULL, NULL}}, //open/close the SW AEC
	{"AUNS",	fAUNS, 		{NULL, NULL}}, //open and adjust the SW NS for mic
	//{"AUAGC",	fAUAGC, 	{NULL, NULL}}, //open/close the SW AGC
	//For Audio speaker
	{"AUSPNS",	fAUSPNS, 	{NULL, NULL}}, //open and adjust the SW NS for speaker
	//{"AUSPAGC",	fAUSPAGC, 	{NULL, NULL}}, //open and adjust the SW NS for speaker
	{"AUSPEQ",	fAUSPEQ, 	{NULL, NULL}}, //adjust the speaker EQ
	{"AUDAC",	fAUDAC, 	{NULL, NULL}}, //adjust the DAC dain for speaker
	{"AUSPM",	fAUSPM, 	{NULL, NULL}}, //enable/disable to mute the speaker
	{"AUTXMODE", fAUTXMODE, {NULL, NULL}}, //adjust speaker output to playtone/playback/noplay
	{"AUAMPIN",	fAUAMPIN, 	{NULL, NULL}}, //select the speaker amplifier pin
	//For Audio TRX
	{"AUSR",	fAUSR,  	{NULL, NULL}}, //set the audio sample rate for both TRX
	{"AUTRX",	fAUTRX, 	{NULL, NULL}}, //enable/disable the audio TRX
	{"AURES",	fAURES, 	{NULL, NULL}}, //reset the audio TRX to enable the previous setting
	{"AUFFTS",	fAUFFTS,	{NULL, NULL}}, //set shown the audio fft result
	//For Audio Recording
	{"AUFILE",	fAUFILE,	{NULL, NULL}}, //set the record file name (need less than 23)
	{"AUSDL",	fAUSDL, 	{NULL, NULL}}, //enable/disable for RAM data dump to SD card
	{"AUREC",	fAUREC, 	{NULL, NULL}}, //record file for the setting time
};

void audio_save_log_init(void)
{
	log_service_add_table(at_audio_save_items, sizeof(at_audio_save_items) / sizeof(at_audio_save_items[0]));
}

log_module_init(audio_save_log_init);