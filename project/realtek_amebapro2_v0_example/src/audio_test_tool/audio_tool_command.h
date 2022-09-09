#ifndef __AUDIO_SAVETOOL_H__
#define __AUDIO_SAVETOOL_H__
#include <platform_stdlib.h>
#include "vfs.h"
#include "module_audio.h"
#include "module_null.h"
#include "module_array.h"
#include "module_tone.h"
#include "module_afft.h"
#include "mmf2_link.h"
#include "mmf2_siso.h"
#include "mmf2_mimo.h"
#include "avcodec.h"
#include <semphr.h>
//#include "pcm16K_std1ktone.h"
//#include "pcm8K_std1ktone.h"
#include "pcm16K_music.h"
#include "pcm8K_music.h"



#define SD_SAVE_EN  	0x01
#define SD_SAVE_START	0x02

#define FRAME_LEN			320        //each frame contain 320 * (word_lengh / 8) bytes, 8K 40ms 16K 20 ms 
#define RECORD_WORDS		80        //record 80 words (160 bytes) each time for SD card

#define RECORD_MIN			0x01
#define RECORD_RX_DATA		0x01
#define RECORD_TX_DATA		0x02
#define RECORD_ASP_DATA		0x04
#define RECORD_MAX			0x04

#if 1
extern 	mm_context_t	*audio_save_ctx;
extern 	mm_context_t 	*null_save_ctx;
extern	mm_context_t 	*array_pcm_ctx;
extern 	mm_context_t 	*pcm_tone_ctx;
extern 	mm_context_t 	*afft_test_ctx;
extern	mm_siso_t	 	*siso_audio_null;
extern	mm_mimo_t	 	*mimo_aarray_audio;
extern	xSemaphoreHandle  ram_dump_sd_sema;


extern audio_params_t audio_save_params;
extern array_params_t pcm16k_array_params;
extern tone_params_t pcm_tone_params;
extern afft_params_t afft_test_params;
extern int reset_flag;
extern char ram_record_file[32];
extern char file_name[20];
extern int recored_count;
extern int record_frame_count;
extern int record_type;

extern uint8_t sdsave_status;
#endif

void audio_fatfs_drv_open(void);
void audio_fatfs_drv_close(void);
void audio_open_record_file(void);
void audio_close_record_file(void);
void audio_record_write_file(int16_t *record);
void audio_mic_record(int16_t *speaker_data_TX, int16_t *mic_data_RX, int16_t *mic_data_ASP);
extern void audio_save_log_init(void);

#endif