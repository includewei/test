#ifndef _MODULE_TONE_H
#define _MODULE_TONE_H

#include <FreeRTOS.h>
#include <freertos_service.h>
#include <task.h>
#include <stdint.h>
#include "timer_api.h"

#define DEFAULT_TONE_LEN				96000*2 // total frame size 10s, word length = DEFAULT_TONE_LEN / 2

#define TONE_MODE_ONCE		0
#define TONE_MODE_LOOP		1

#define CMD_TONE_SET_PARAMS			MM_MODULE_CMD(0x00)  // set parameter
#define CMD_TONE_GET_PARAMS			MM_MODULE_CMD(0x01)  // get parameter
#define CMD_TONE_SET_AUDIOTONE		MM_MODULE_CMD(0x02)  // set audio tone rate
#define CMD_TONE_SET_SAMPLERATE		MM_MODULE_CMD(0x03)  // set samplerate
#define CMD_TONE_GET_STATE			MM_MODULE_CMD(0x04)
#define CMD_TONE_STREAMING			MM_MODULE_CMD(0x05)
#define CMD_TONE_RECOUNT_PERIOD		MM_MODULE_CMD(0x06)


#define CMD_TONE_APPLY				MM_MODULE_CMD(0x20)  // for hardware module

typedef struct tone_param_s {
	uint32_t codec_id;
	uint8_t mode;
	uint32_t channel;

	uint32_t audiotonerate;
	uint32_t samplerate;
	uint32_t sample_bit_length;
	uint32_t frame_size;

} tone_params_t;


typedef struct tone_ctx_s {
	void *parent;
	TaskHandle_t task;
	_sema up_sema;
	gtimer_t frame_timer;
	uint32_t frame_timer_period; // us
	uint32_t audio_timer_delay_ms;
	uint32_t tone_data_offset;

	tone_params_t params;
	// flag
	int stop;
} tone_ctx_t;

extern mm_module_t tone_module;
#endif