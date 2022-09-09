/******************************************************************************
*
* Copyright(c) 2007 - 2018 Realtek Corporation. All rights reserved.
*
******************************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>

#include "mmf2_module.h"
#include "audio_api.h"
#include "module_audio.h"
#include "avcodec.h"


//------------------------------------------------------------------------------
#if defined(CONFIG_PLATFORM_8195BHP) || defined(CONFIG_PLATFORM_8735B)

#if defined(CONFIG_PLATFORM_8735B)
#if IS_CUT_TEST(CONFIG_CHIP_VER)
#define DMIC_CLK_PIN    PE_2 //PE_0
#define DMIC_DATA_PIN   PE_4
#else
#define DMIC_CLK_PIN    PD_16 //PD_14
#define DMIC_DATA_PIN   PD_18
#endif
#define AUDIO_DMA_PAGE_NUM 4
#define TX_CACHE_DEPTH	16
#define RX_CACHE_DEPTH	(AUDIO_DMA_PAGE_NUM*2)
#else
#define AUDIO_DMA_PAGE_NUM 2
#define RX_CACHE_DEPTH	(AUDIO_DMA_PAGE_NUM*2)
#endif

#if defined(SAVE_AUDIO_DATA) && SAVE_AUDIO_DATA
#define AUDIO_DMA_PAGE_SIZE (640)	// 16KHz 20ms
#else
#define AUDIO_DMA_PAGE_SIZE (320)	// 20ms: 160 samples, 16bit
//#define AUDIO_DMA_PAGE_SIZE (640)	// 16KHz 20ms
#endif
#define AUDIO_AEC_PAGE_SIZE (320)

#else
#define AUDIO_DMA_PAGE_NUM 4
#define AUDIO_DMA_PAGE_SIZE (320)	// 20ms: 160 samples, 16bit
#define TX_CACHE_DEPTH	16
#endif

#define TX_PAGE_SIZE 	AUDIO_DMA_PAGE_SIZE //64*N bytes, max: 4095. 128, 4032 
#define TX_PAGE_NUM 	AUDIO_DMA_PAGE_NUM
#define RX_PAGE_SIZE 	AUDIO_DMA_PAGE_SIZE //64*N bytes, max: 4095. 128, 4032
#define RX_PAGE_NUM 	AUDIO_DMA_PAGE_NUM

static uint8_t dma_txdata[TX_PAGE_SIZE * TX_PAGE_NUM]__attribute__((aligned(0x20)));
static uint8_t dma_rxdata[RX_PAGE_SIZE * RX_PAGE_NUM]__attribute__((aligned(0x20)));

#define AUDIO_TX_PCM_QUEUE_LENGTH (20)

typedef struct pcm_tx_cache_s {
	xQueueHandle queue;
	uint16_t idx;
	uint8_t  buffer[AUDIO_DMA_PAGE_SIZE];	// for sw output cache handler
	uint8_t  txbuf[AUDIO_DMA_PAGE_SIZE];	// for interrupt
} pcm_tx_cache_t;

static pcm_tx_cache_t	*pcm_tx_cache = NULL;


#define LOGIC_INPUT_NUM	4
static int logic_input_num = 1;		// for non-mix mode

#if LOGIC_INPUT_NUM==2
#warning ****VERY IMPORTANT : AUDIO must connect to MIMO/MISO INPUT 0 and 1 ****
// TODO : remove this limitation
#endif

#if LOGIC_INPUT_NUM!=4 && LOGIC_INPUT_NUM!=2
#error ONLY SUPPORT 4 and 2
#endif
//-------------AEC interrupt handler ------------------------------------------
#if ENABLE_SPEEX_AEC==1
#include "speex/speex_echo.h"
#include "speex/speex_preprocess.h"

#define SPEEX_SAMPLE_RATE (8000)
#define NN (AUDIO_DMA_PAGE_SIZE/2)
#define TAIL_LENGTH_IN_MILISECONDS (20)
#define TAIL (TAIL_LENGTH_IN_MILISECONDS * (SPEEX_SAMPLE_RATE / 1000) )
static uint8_t last_tx_buf[AUDIO_DMA_PAGE_SIZE];
static uint8_t last_rx_buf[AUDIO_DMA_PAGE_SIZE];
#endif //ENABLE_SPEEX_AEC

#if ENABLE_ASP==1

static uint8_t last_tx_buf[AUDIO_DMA_PAGE_SIZE];

static xQueueHandle audio_rx_buf = NULL;

int last_tx_ts, proc_tx_ts;
int last_rx_ts, proc_rx_ts;

static uint8_t proc_tx_buf[AUDIO_DMA_PAGE_SIZE];
static uint8_t proc_rx_buf[AUDIO_DMA_PAGE_SIZE];

#define M 8
#define NN (AUDIO_AEC_PAGE_SIZE/2)
#define TAIL_LENGTH_IN_MILISECONDS (20*M)
#define TAIL(rate) (TAIL_LENGTH_IN_MILISECONDS * (rate / 1000) )

#include "AEC.h"
#endif //ENABLE_ASP

#if CONFIG_MMF_AUDIO_DEBUG
#include <audio_debug.h>
int audio_tx_debug_cnt = 0x7fffffff;
int audio_rx_debug_cnt = 0x7fffffff;
int audio_rx_aec_cnt = 0x7fffffff;
int audio_rx_debug_mode = 0;
#endif

#if CONFIG_MMF_AUDIO_ATAF
/* for ATCMD control
 * use ATAF=[aec,vad],[0 or 1]
 *     ATAF=[ns,agc],[0 ~ 3]
 * Here default all on to prevent turn on in parameter but still feature off
 */
int module_audio_aec = 1;
int module_audio_vad = 1;
int module_audio_agc = 3;
int module_audio_ns = 3;


#define ATAF_AEC_CTRL module_audio_aec
#define ATAF_AGC_CTRL module_audio_agc
#define ATAF_VAD_CTRL module_audio_vad
#define ATAF_NS_CTRL module_audio_ns
#else
#define ATAF_AEC_CTRL 1
#define ATAF_AGC_CTRL 3
#define ATAF_VAD_CTRL 1
#define ATAF_NS_CTRL 3
#endif

static int NS_MODE = 3;			// 0~3 ns aggressive level
static int VAD_MODE = 1;		// 0 : normal 1:low rate 2: aggrasive 3: very aggrasive

static int audio_get_samplerate(audio_sr rate)
{
	switch (rate) {
	case ASR_8KHZ:
		return 8000;
	case ASR_16KHZ:
		return 16000;
	case ASR_32KHZ:
		return 32000;
	case ASR_44p1KHZ:
		return 44100;
	case ASR_48KHZ:
		return 48000;
	case ASR_88p2KHZ:
		return 88200;
	case ASR_96KHZ:
		return 96000;
	default:
		printf("wrong rate index %d\n\r", rate);
		while (1);
	}
}
static void audio_tx_complete(uint32_t arg, uint8_t *pbuf)
{
	audio_ctx_t *ctx = (audio_ctx_t *)arg;
	audio_t *obj = (audio_t *)ctx->audio;
	uint8_t *ptx_buf;
#if ENABLE_ASP==1
	last_tx_ts = xTaskGetTickCountFromISR();
	memcpy(last_tx_buf, pbuf, AUDIO_DMA_PAGE_SIZE);
#elif ENABLE_SPEEX_AEC==1
	memcpy(last_tx_buf, pbuf, AUDIO_DMA_PAGE_SIZE);
#endif

	ptx_buf = (uint8_t *)audio_get_tx_page_adr(obj);
#if LOGIC_INPUT_NUM==4 || LOGIC_INPUT_NUM==2
	if (ctx->params.mix_mode) {
		for (int i = 0; i < logic_input_num; i++) {
			if (xQueueReceiveFromISR(pcm_tx_cache[i].queue, pcm_tx_cache[i].txbuf, NULL) != pdPASS) {
				memset(pcm_tx_cache[i].txbuf, 0, AUDIO_DMA_PAGE_SIZE);
			}
		}
		if (ctx->params.word_length == WL_16BIT) {
			int16_t *ptx_tmp = (int16_t *)ptx_buf;
			int16_t *cache_txbuf0 = (int16_t *)pcm_tx_cache[0].txbuf;
			int16_t *cache_txbuf1 = (int16_t *)pcm_tx_cache[1].txbuf;
#if LOGIC_INPUT_NUM==4
			int16_t *cache_txbuf2 = (int16_t *)pcm_tx_cache[2].txbuf;
			int16_t *cache_txbuf3 = (int16_t *)pcm_tx_cache[3].txbuf;
#endif
			for (int i = 0; i < AUDIO_DMA_PAGE_SIZE / 2; i++) {
				ptx_tmp[i] = cache_txbuf0[i] / LOGIC_INPUT_NUM + cache_txbuf1[i] / LOGIC_INPUT_NUM
#if LOGIC_INPUT_NUM==4
							 + cache_txbuf2[i] / LOGIC_INPUT_NUM + cache_txbuf3[i] / LOGIC_INPUT_NUM
#endif
							 ;
			}
		} else {
			for (int i = 0; i < AUDIO_DMA_PAGE_SIZE; i++) {
				ptx_buf[i] = (int8_t)pcm_tx_cache[0].txbuf[i] / LOGIC_INPUT_NUM + (int8_t)pcm_tx_cache[1].txbuf[i] / LOGIC_INPUT_NUM
#if LOGIC_INPUT_NUM==4
							 + (int8_t)pcm_tx_cache[2].txbuf[i] / LOGIC_INPUT_NUM + (int8_t)pcm_tx_cache[3].txbuf[i] / LOGIC_INPUT_NUM
#endif
							 ;
			}
		}
	} else
#endif
	{
		if (xQueueReceiveFromISR(pcm_tx_cache[0].queue, ptx_buf, NULL) != pdPASS) {
			memset(ptx_buf, 0, AUDIO_DMA_PAGE_SIZE);
		}
	}

#if defined(AUDIO_DEBUG_H)
	if (audio_tx_debug_ena == 1) {
		printf("start tx playback debug\n\r");
		audio_tx_debug_cnt = 0;
		audio_tx_debug_ena = 0;
	}

	if (audio_tx_debug_cnt < audio_tx_debug_len) {
		memcpy(ptx_buf, &audio_tx_debug_buffer[audio_tx_debug_cnt], AUDIO_DMA_PAGE_SIZE);
		audio_tx_debug_cnt += AUDIO_DMA_PAGE_SIZE;
		if (audio_tx_debug_cnt >= audio_tx_debug_len) {
			printf("done for tx playback debug\n\r");
		}

	}
#endif

	audio_set_tx_page(obj, (uint8_t *)ptx_buf);
}

static void audio_rx_complete(uint32_t arg, uint8_t *pbuf)
{
	audio_ctx_t *ctx = (audio_ctx_t *)arg;
	audio_t *obj = (audio_t *)ctx->audio;
	uint32_t audio_rx_ts = xTaskGetTickCountFromISR();
	int is_output_ready = 0;
	// set timestamp to 1st sample
	audio_rx_ts -= 1000 * (AUDIO_DMA_PAGE_SIZE / ctx->word_length) / ctx->sample_rate;

#if AUDIO_LOOPBACK
	uint8_t *ptx_addre;
	ptx_addre = audio_get_tx_page_adr(obj);
	memcpy((void *)ptx_addre, (void *)pbuf, TX_PAGE_SIZE);
	audio_set_tx_page(obj, ptx_addre);
#endif

#if AUDIO_DEBUG_SHOW
	if (ctx->timer_2 == 0) {
		ctx->timer_1 = xTaskGetTickCountFromISR();
		ctx->timer_2 = ctx->timer_1;
	} else {
		ctx->timer_2 = xTaskGetTickCountFromISR();
		//print every 1 min
		if ((ctx->timer_2 - ctx->timer_1) >= 60000) {
			printf("audio:%ld drop:%ld audio_total:%ld drop_total:%ld\r\n", ctx->audio_frame, ctx->drop_frame, ctx->audio_frame_total, ctx->drop_frame_total);
			ctx->timer_2 = 0;
			ctx->drop_frame = 0;
			ctx->audio_frame = 0;
		}
	}
#endif

#if ENABLE_ASP==1
	if (ctx->params.enable_aec || (ctx->params.enable_ns & 0x2) || ctx->params.enable_vad || (ctx->params.enable_agc & 0x2)) {
		last_rx_ts = xTaskGetTickCountFromISR();
		BaseType_t xHigherPriorityTaskWoken_AEC = pdFALSE;

		is_output_ready = xQueueSendFromISR(audio_rx_buf, (void *)pbuf, &xHigherPriorityTaskWoken_AEC);
#if AUDIO_DEBUG_SHOW
		if (is_output_ready != pdTRUE) {
			ctx->drop_frame++;
			ctx->drop_frame_total++;
		} else {
			ctx->audio_frame++;
			ctx->audio_frame_total++;
		}
#endif
		audio_set_rx_page(obj);

	} else

#elif ENABLE_SPEEX_AEC==1
	if (ctx->params.enable_aec) {
		memcpy(last_rx_buf, pbuf, AUDIO_DMA_PAGE_SIZE);

		if (ctx->aec_rx_done_sema) {
			xSemaphoreGiveFromISR(ctx->aec_rx_done_sema, NULL);
		}
	} else
#endif
	{
		BaseType_t xTaskWokenByReceive = pdFALSE;
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;

		mm_context_t *mctx = (mm_context_t *)ctx->parent;
		mm_queue_item_t *output_item;
		if (mctx->output_recycle && (xQueueReceiveFromISR(mctx->output_recycle, &output_item, &xTaskWokenByReceive) == pdTRUE)) {

			memcpy((void *)output_item->data_addr, (void *)pbuf, RX_PAGE_SIZE);
			output_item->size = RX_PAGE_SIZE;
			output_item->timestamp = audio_rx_ts;
			output_item->type = AV_CODEC_ID_PCM_RAW;
#if AUDIO_DEBUG_SHOW
			ctx->audio_frame++;
			ctx->audio_frame_total++;
#endif
			xQueueSendFromISR(mctx->output_ready, (void *)&output_item, &xHigherPriorityTaskWoken);
		}
#if AUDIO_DEBUG_SHOW
		else {
			ctx->drop_frame++;
			ctx->drop_frame_total++;
		}
#endif
		audio_set_rx_page(obj);

		if (xHigherPriorityTaskWoken || xTaskWokenByReceive) {
			taskYIELD();
		}
	}
}

static int fade_level = 0;
#define FADE_IN 1
#define FADE_OUT 0

#define FADE_RATIO	16
#define FADE_LEVEL(curr_level) (curr_level/FADE_RATIO)
#define FADE_SILENCE_LEVEL	16*FADE_RATIO

#define MODE_INSTANT 0
#define MODE_SLOPE 1
#define FADE_IN_MODE MODE_SLOPE

/* zero -> 16*FADE_RATIO, suppress  x*FADE_RATIO = 3*x dB */
#define FADE_MAX_LEVEL 16*FADE_RATIO	// 16, silence

#if FADE_IN_MODE==MODE_INSTANT
int last_fade_state = 0;
#endif

static void audio_fade_signal(int fade_state, int16_t *data, int num)
{
	if (fade_state == FADE_OUT) {	// fade out
		if (fade_level < FADE_MAX_LEVEL) {
			int __level = FADE_LEVEL(fade_level);
			for (int i = 0; i < num; i++) {
				data[i] >>= __level;
			}

			fade_level++;
		} else {
			if (FADE_MAX_LEVEL == FADE_SILENCE_LEVEL) {
				memset((int16_t *)data, 0, num * sizeof(int16_t));
			} else {
				int __level = FADE_LEVEL(FADE_MAX_LEVEL);
				for (int i = 0; i < num; i++) {
					data[i] >>= __level;
				}
			}
		}
	} else {	// fade in
#if FADE_IN_MODE==MODE_INSTANT
		int __level = FADE_LEVEL(fade_level);
		int step = 16 - __level > 0 ? num / (16 - __level) : num / 16;

		if (last_fade_state == 0) {
			for (int i = 0; i < (16 - __level); i++) {
				for (int j = 0; j < step; j++) {
					data[i * step + j] >>= (16 - __level - i);
				}
			}
		}
		fade_level = 0;
#else // SLOPE
		if (fade_level > 0) {
			int __level = FADE_LEVEL(fade_level);
			for (int i = 0; i < num; i++) {
				data[i] >>= __level;
			}

			// 8 times speed to fade in
			fade_level -= 8;
		} else {
			// do nothing
			fade_level = 0;
		}
#endif
	}
#if FADE_IN_MODE==MODE_INSTANT
	last_fade_state = fade_state;
#endif
}

#if ENABLE_ASP==1
static void audio_rx_handle_thread(void *param)
{
	audio_ctx_t *ctx = (audio_ctx_t *)param;
	mm_context_t *mctx = (mm_context_t *)ctx->parent;
	mm_queue_item_t *output_item;

	if (ctx->params.sample_rate == ASR_8KHZ || ctx->params.sample_rate == ASR_16KHZ) {
		int sampleRate = audio_get_samplerate(ctx->params.sample_rate);

		printf("sample rate: %d \r\n", sampleRate);
		if (ctx->params.enable_aec) {
			//AEC_init(NN, sampleRate, WEBRTC_AECM, TAIL(sampleRate), 1, 18, 0, NS_MODE, 1.0f);
			AEC_init(NN, sampleRate, WEBRTC_AECM, TAIL(sampleRate), 2, 18, 0, NS_MODE, 1.0f);
			printf("Inintial AEC \r\n");
			ctx->inited_aec = 1;
			ctx->run_aec = 1;
		}

		if (ctx->params.enable_ns & 0x2) {
#if defined(SAVE_AUDIO_DATA) && SAVE_AUDIO_DATA
			if (ctx->params.NS_level != -1) {

				NS2_init(sampleRate, ctx->params.NS_level);
				ctx->inited_ns |= 0x2;
				ctx->run_ns |= 0x2;
				printf("Set MIC NS level %d\r\n", ctx->params.NS_level);
			} else {
				printf("NO MIC NS SET\r\n");
			}
#else
			printf("Inintial NS2 %d\r\n", NS_MODE);
			NS2_init(sampleRate, NS_MODE);
			ctx->inited_ns |= 0x2;
			ctx->run_ns |= 0x2;
#endif
		}

		if (ctx->params.enable_agc & 0x2) {
			AGC2_init(sampleRate, 2, 24, 0);
			ctx->inited_agc |= 0x2;
			ctx->run_agc |= 0x2;
		}

		if (ctx->params.enable_vad) {
			VAD_init(sampleRate, VAD_MODE);
			ctx->inited_vad = 1;
			ctx->run_vad = 1;
		}
	}
	while (1) {
		if (!mctx->output_recycle || !audio_rx_buf) {
			// drop current frame
			// NOT SEND
#if AUDIO_DEBUG_SHOW
			rt_printf("[Audio AEC] NO audio output_recycle queue block here for debug\r\n");
#endif
			while (1);
		}
		if (xQueueReceive(audio_rx_buf, proc_rx_buf, 0xFFFFFFFF) != pdTRUE) {
#if AUDIO_DEBUG_SHOW
			//rt_printf("[Audio AEC] Can not get audio buffer\r\n");
#endif
			continue;
		}
		uint32_t audio_rx_ts = xTaskGetTickCount();
		memcpy(proc_tx_buf, last_tx_buf, AUDIO_DMA_PAGE_SIZE);
		// last tx rx buffer may be corrupted by interrupt, backup processing data first
		proc_tx_ts = last_tx_ts;
		proc_rx_ts = last_rx_ts;
		if (xQueueReceive(mctx->output_recycle, &output_item, 0xFFFFFFFF) == pdTRUE) {
#if defined(AUDIO_DEBUG_H)
			if (audio_rx_debug_ena != 0) {
#if defined(AUDIO_DEBUG_ENABLE)
				printf("start rx recording\n\r");
#endif
				audio_rx_debug_cnt = 0;
				audio_rx_debug_ena = 0;
			}
			if (audio_rx_debug_cnt < audio_rx_debug_len) {
				memcpy(&audio_rx_debug_buffer[audio_rx_debug_cnt], last_rx_buf, AUDIO_DMA_PAGE_SIZE);
				audio_rx_debug_cnt += AUDIO_DMA_PAGE_SIZE;
				if (audio_rx_debug_cnt >= audio_rx_debug_len) {
#if defined(AUDIO_DEBUG_ENABLE)
					printf("done for rx (mic) recording\n\r");
#endif
				}
			}
#endif
			int aec_proc_time = xTaskGetTickCount();
			if (ctx->params.sample_rate == ASR_8KHZ || ctx->params.sample_rate == ASR_16KHZ) {
				if (ctx->inited_aec && ctx->run_aec && ATAF_AEC_CTRL) {
					AEC_process((int16_t *)(proc_tx_buf), (int16_t *)(proc_rx_buf), (int16_t *)output_item->data_addr);
					if (AUDIO_DMA_PAGE_SIZE == 640) {
						AEC_process((int16_t *)(proc_tx_buf) + 160, (int16_t *)(proc_rx_buf) + 160, (int16_t *)output_item->data_addr + 160);
					}
				} else {
					memcpy((int16_t *)output_item->data_addr, (int16_t *)(proc_rx_buf), AUDIO_DMA_PAGE_SIZE);
				}
				if ((ctx->inited_ns & 0x2) && (ctx->run_ns & 0x2) && (ATAF_NS_CTRL & 0x2)) {
					NS2_process(AUDIO_DMA_PAGE_SIZE / sizeof(int16_t), (int16_t *)output_item->data_addr);
				}
				if ((ctx->inited_agc & 0x2) && (ctx->run_agc & 0x2) && (ATAF_AGC_CTRL & 0x2)) {
					AGC2_process(AUDIO_DMA_PAGE_SIZE / sizeof(int16_t), (int16_t *)output_item->data_addr);
				}
				if (ctx->inited_vad && ctx->run_vad && (ATAF_VAD_CTRL)) {
					int vad_state = VAD_process(AUDIO_DMA_PAGE_SIZE / sizeof(int16_t), (int16_t *)output_item->data_addr);
					if (vad_state == 0) {
						audio_fade_signal(FADE_OUT, (int16_t *)output_item->data_addr, AUDIO_DMA_PAGE_SIZE / sizeof(int16_t));
					} else {
						audio_fade_signal(FADE_IN, (int16_t *)output_item->data_addr, AUDIO_DMA_PAGE_SIZE / sizeof(int16_t));
					}
				}
			} else {
				memcpy((int16_t *)output_item->data_addr, (int16_t *)(proc_rx_buf), AUDIO_DMA_PAGE_SIZE);
			}
#if defined(SAVE_AUDIO_DATA) && SAVE_AUDIO_DATA
			if (ctx->mic_record_file) {
				ctx->mic_record_file((int16_t *)(proc_tx_buf), (int16_t *)(proc_rx_buf), (int16_t *)(output_item->data_addr));
			}
#endif
			aec_proc_time = xTaskGetTickCount() - aec_proc_time;
			if (aec_proc_time > 20) {
				printf("AEC proc execution too long %dms\n\r", aec_proc_time);
#if defined(AUDIO_DEBUG_ENABLE)
				printf("AEC proc execution too long %dms\n\r", aec_proc_time);
#endif
			}
#if defined(AUDIO_DEBUG_H)
			if (audio_rx_aec_ena != 0) {
				printf("start rx aec recording\n\r");
				audio_rx_aec_cnt = 0;
				audio_rx_aec_ena = 0;
			}
			if (audio_rx_aec_cnt < audio_rx_aec_len) {
				memcpy(&audio_rx_aec_buffer[audio_rx_aec_cnt], (void *)output_item->data_addr, AUDIO_DMA_PAGE_SIZE);
				audio_rx_aec_cnt += AUDIO_DMA_PAGE_SIZE;
				if (audio_rx_aec_cnt >= audio_rx_aec_len) {
#if defined(AUDIO_DEBUG_ENABLE)
					printf("done for rx (aec) recording\n\r");
#endif
				}
			}
#endif
			if (proc_tx_ts != last_tx_ts) {
#if defined(AUDIO_DEBUG_ENABLE)
				printf("TX buffer update when AEC processing\n\r");
#endif
				if (last_tx_ts - proc_tx_ts > 20) {
#if defined(AUDIO_DEBUG_ENABLE)
					printf("last_tx_ts, proc_tx_ts diff %d > 20ms\n\r", last_tx_ts - proc_tx_ts);
#endif
				}
			}
			if (proc_rx_ts != last_rx_ts) {
#if defined(AUDIO_DEBUG_ENABLE)
				printf("RX buffer update when AEC processing\n\r");
#endif
				if (last_rx_ts - proc_rx_ts > 20) {
#if defined(AUDIO_DEBUG_ENABLE)
					printf("last_rx_ts, proc_rx_ts diff %d > 20ms\n\r", last_rx_ts - proc_rx_ts);
#endif
				}
			}
			output_item->size = RX_PAGE_SIZE;
			output_item->timestamp = audio_rx_ts;
			output_item->type = AV_CODEC_ID_PCM_RAW;
			xQueueSend(mctx->output_ready, (void *)&output_item, 0xFFFFFFFF);
		}
	}
}
#elif ENABLE_SPEEX_AEC==1
static void audio_rx_handle_thread(void *param)
{
	audio_ctx_t *ctx = (audio_ctx_t *)param;
	mm_context_t *mctx = (mm_context_t *)ctx->parent;
	mm_queue_item_t *output_item;

	audio_t *obj = (audio_t *)ctx->audio;

	SpeexEchoState *st;
	SpeexPreprocessState *den;
	int sampleRate = SPEEX_SAMPLE_RATE;

	st = speex_echo_state_init(NN, TAIL);
	den = speex_preprocess_state_init(NN, sampleRate);
	speex_echo_ctl(st, SPEEX_ECHO_SET_SAMPLING_RATE, &sampleRate);
	speex_preprocess_ctl(den, SPEEX_PREPROCESS_SET_ECHO_STATE, st);


	while (1) {
		xSemaphoreTake(ctx->aec_rx_done_sema, portMAX_DELAY);

		uint32_t audio_rx_ts = xTaskGetTickCountFromISR();

		if (!mctx->output_recycle || (xQueueReceive(mctx->output_recycle, &output_item, 0xFFFFFFFF) != pdTRUE)) {
			// drop current frame
			// NOT SEND
			audio_set_rx_page(obj);
			continue;
		}

		speex_echo_cancellation(st, (void *)(last_rx_buf), (void *)(last_tx_buf), (void *)output_item->data_addr);
		speex_preprocess_run(den, (void *)output_item->data_addr);

		output_item->size = RX_PAGE_SIZE;
		output_item->timestamp = audio_rx_ts;
		output_item->type = AV_CODEC_ID_PCM_RAW;

		xQueueSend(mctx->output_ready, (void *)&output_item, 0xFFFFFFFF);

		audio_set_rx_page(obj);
	}
}
#endif

int audio_control(void *p, int cmd, int arg)
{
	audio_ctx_t *ctx = (audio_ctx_t *)p;
	int sample_rate = 8000;
	switch (cmd) {
	case CMD_AUDIO_SET_ADC_GAIN:
		audio_adc_digital_vol(ctx->audio, arg);
		printf("ADC Gain: 0x%x.\r\n", arg);
		break;
	case CMD_AUDIO_SET_DAC_GAIN:
		audio_dac_digital_vol(ctx->audio, arg);
		printf("DAC Gain: 0x%x.\r\n", arg);
		break;
	case CMD_AUDIO_SET_PARAMS:
		memcpy(&ctx->params, (void *)arg, sizeof(audio_params_t));
		break;
	case CMD_AUDIO_GET_PARAMS:
		memcpy((void *)arg, &ctx->params, sizeof(audio_params_t));
		break;
#if defined(CONFIG_PLATFORM_8195BHP) || defined(CONFIG_PLATFORM_8735B)
#if ENABLE_ASP==1
	case CMD_AUDIO_SET_NS_ENABLE:
		if (arg > 3 || arg < 0)	{
			arg = 0;
		}
		ctx->params.enable_ns = arg;
		break;
	case CMD_AUDIO_SET_AEC_ENABLE:
		if (arg > 1 || arg < 0)	{
			arg = 0;
		}
		printf("AEC Enable: %d.\r\n", arg);
		ctx->params.enable_aec = arg;
		break;
	case CMD_AUDIO_SET_AGC_ENABLE:
		if (arg > 3 || arg < 0)	{
			arg = 0;
		}
		ctx->params.enable_agc = arg;
		break;
	case CMD_AUDIO_SET_VAD_ENABLE:
		if (arg > 1 || arg < 0)	{
			arg = 0;
		}
		ctx->params.enable_vad = arg;
		break;
	case CMD_AUDIO_SET_SPK_ENABLE:
		if (arg == 0) {
			audio_dac_digital_mute(ctx->audio, 1);
			printf("Speaker Disable\r\n");
		} else {
			audio_dac_digital_mute(ctx->audio, 0);
			printf("Speaker Enable\r\n");
		}
		break;
	case CMD_AUDIO_RUN_NS:
		if (arg > 3 || arg < 0)	{
			arg = 0;
		}
		ctx->run_ns = arg;
		break;
	case CMD_AUDIO_RUN_AEC:
		if (arg > 1 || arg < 0)	{
			arg = 0;
		}
		ctx->run_aec = arg;
		break;
	case CMD_AUDIO_RUN_AGC:
		if (arg > 3 || arg < 0)	{
			arg = 0;
		}
		ctx->run_agc = arg;
		break;
	case CMD_AUDIO_RUN_VAD:
		if (arg > 1 || arg < 0)	{
			arg = 0;
		}
		ctx->run_vad = arg;
		break;
	case CMD_AUDIO_SET_AEC_LEVEL:
		AEC_destory();
		sample_rate = audio_get_samplerate(ctx->params.sample_rate);
		AEC_init(NN, sample_rate, WEBRTC_AECM, TAIL(sample_rate), 1, 18, 0, NS_MODE, 1.0f);
		if (AEC_set_level(arg) != 0) {
			printf("Set AEC level fail.\r\n");
		}
		break;
#endif
	case CMD_AUDIO_SET_SAMPLERATE:
		ctx->params.sample_rate = (audio_sr)arg;
		break;
	case CMD_AUDIO_SET_TRX:
		if (arg == TRUE) {
			audio_trx_start(ctx->audio);
			printf("START TRX\r\n");
		} else {
			audio_trx_stop(ctx->audio);
			printf("STOP TRX\r\n");
		}
		break;
#if defined(SAVE_AUDIO_DATA) && SAVE_AUDIO_DATA
	case CMD_AUDIO_SET_MIC_RECORD_FUN:
		ctx->mic_record_file = (void (*)(void *))arg;
		break;
#endif
	case CMD_AUDIO_SET_RESET:
#if ENABLE_ASP==1
		if (ctx->params.enable_aec || (ctx->params.enable_ns & 0x2) || ctx->params.enable_vad || (ctx->params.enable_agc & 0x2) && audio_rx_buf) {
			xQueueReset(audio_rx_buf);
		}

		if (ctx->params.sample_rate == ASR_8KHZ || ctx->params.sample_rate == ASR_16KHZ) {
			sample_rate = audio_get_samplerate(ctx->params.sample_rate);
			/* reset all voice algorithm */
			if (ctx->inited_ns & 0x1) {
				NS_destory();
			}
			if (ctx->inited_ns & 0x2) {
				NS2_destory();
			}
			ctx->inited_ns = 0;
			ctx->run_ns = 0;
			if (ctx->params.enable_ns & 0x1) {
#if defined(SAVE_AUDIO_DATA) && SAVE_AUDIO_DATA
				if (ctx->params.NS_level_SPK != -1) {
					NS_init(sample_rate, ctx->params.NS_level_SPK);
					printf("Inintial NS \r\n");
					ctx->inited_ns |= 0x1;
					ctx->run_ns |= 0x1;
					printf("Set Speaker NS level %d\r\n", ctx->params.NS_level_SPK);
				} else {
					printf("NO Speaker NS SET\r\n");
				}
#else
				NS_init(sample_rate, NS_MODE);
				printf("Inintial NS \r\n");
				ctx->inited_ns |= 0x1;
				ctx->run_ns |= 0x1;
#endif
			}
			if (ctx->params.enable_ns & 0x2) {
#if defined(SAVE_AUDIO_DATA) && SAVE_AUDIO_DATA
				if (ctx->params.NS_level != -1) {
					NS2_init(sample_rate, ctx->params.NS_level);
					ctx->inited_ns |= 0x2;
					ctx->run_ns |= 0x2;
					printf("Set MIC NS level %d\r\n", ctx->params.NS_level);
				} else {
					printf("NO MIC NS SET\r\n");
				}
#else
				NS2_init(sample_rate, NS_MODE);
				ctx->inited_ns |= 0x2;
				ctx->run_ns |= 0x2;
#endif
			}
			if (ctx->inited_agc & 0x1) {
				AGC_destory();
			}
			if (ctx->inited_agc & 0x2) {
				AGC2_destory();
			}
			ctx->inited_agc = 0;
			ctx->run_agc = 0;
			if (ctx->params.enable_agc & 0x1) {
				AGC_init(sample_rate, 2, 24, 0);
				ctx->inited_agc |= 0x1;
				ctx->run_agc |= 0x1;
			}
			if (ctx->params.enable_agc & 0x2) {
				AGC2_init(sample_rate, 2, 24, 0);
				ctx->inited_agc |= 0x2;
				ctx->run_agc |= 0x2;
			}
			if (ctx->inited_aec) {
				AEC_destory();
			}
			ctx->inited_aec = 0;
			ctx->run_aec = 0;
			if (ctx->params.enable_aec) {
				AEC_init(NN, sample_rate, WEBRTC_AECM, TAIL(sample_rate), 1, 18, 0, NS_MODE, 1.0f);
				ctx->inited_aec = 0x1;
				ctx->run_aec = 0x1;
			}
			if (ctx->inited_vad) {
				VAD_destory();
			}
			ctx->inited_vad = 0;
			ctx->run_vad = 0;
			if (ctx->params.enable_vad) {
				VAD_init(sample_rate, VAD_MODE);
				ctx->inited_vad = 0x1;
				ctx->run_vad = 0x1;
			}
		}
#endif
		printf("[16k_noise_issue]reset ns=%d agc=%d aec=%d.\r\n", ctx->params.enable_ns, ctx->params.enable_agc, ctx->params.enable_aec);
		audio_tx_stop(ctx->audio);
		audio_rx_stop(ctx->audio);
		audio_deinit(ctx->audio);
#if defined(CONFIG_PLATFORM_8735B)
		if (ctx->params.use_mic_type == USE_AUDIO_AMIC) {
			audio_init(ctx->audio, OUTPUT_SINGLE_EDNED, MIC_SINGLE_EDNED, AUDIO_CODEC_2p8V);
			printf("Init AMIC \r\n");
		} else if (ctx->params.use_mic_type == USE_AUDIO_LEFT_DMIC) {
			if (ctx->dmic_pin_set == 0) {
				audio_dmic_pinmux(ctx->audio, DMIC_CLK_PIN, DMIC_DATA_PIN);
				ctx->dmic_pin_set = 1;
			}
			audio_init(ctx->audio, OUTPUT_SINGLE_EDNED, AUDIO_LEFT_DMIC, AUDIO_CODEC_2p8V);
			printf("Init Left DMIC \r\n");
		} else if (ctx->params.use_mic_type == USE_AUDIO_RIGHT_DMIC) {
			if (ctx->dmic_pin_set == 0) {
				audio_dmic_pinmux(ctx->audio, DMIC_CLK_PIN, DMIC_DATA_PIN);
				ctx->dmic_pin_set = 1;
			}
			audio_init(ctx->audio, OUTPUT_SINGLE_EDNED, AUDIO_RIGHT_DMIC, AUDIO_CODEC_2p8V);
			printf("Init Right DMIC \r\n");
		} else if (ctx->params.use_mic_type == USE_AUDIO_STEREO_DMIC) {
			if (ctx->dmic_pin_set == 0) {
				audio_dmic_pinmux(ctx->audio, DMIC_CLK_PIN, DMIC_DATA_PIN);
				ctx->dmic_pin_set = 1;
			}
			audio_init(ctx->audio, OUTPUT_SINGLE_EDNED, AUDIO_STEREO_DMIC, AUDIO_CODEC_2p8V);
			printf("Init Stereo DMIC \r\n");
		} else {
			printf("unsupported MIC type \r\n");
		}
#if defined(SAVE_AUDIO_DATA) && SAVE_AUDIO_DATA
		audio_adc_digital_vol(ctx->audio, ctx->params.ADC_gain);
		audio_dac_digital_vol(ctx->audio, ctx->params.DAC_gain);
#else
		audio_adc_digital_vol(ctx->audio, 0x7F);	//Mic DGain 30dB
		audio_dac_digital_vol(ctx->audio, 0xAF);	//Spk DGain 0dB
#endif

		//audio_headphone_analog_mute(ctx->audio, 1);
		audio_set_dma_buffer(ctx->audio, dma_txdata, dma_rxdata, AUDIO_DMA_PAGE_SIZE, AUDIO_DMA_PAGE_NUM);
#else
		audio_init(ctx->audio, OUTPUT_SINGLE_EDNED, MIC_SINGLE_EDNED, AUDIO_CODEC_2p8V);
		//audio_mic_analog_gain(ctx->audio, 1, AUDIO_MIC_40DB);
		//audio_adc_digital_vol(ctx->audio, 0x7F);
		audio_dac_digital_vol(ctx->audio, 0xAF);
		//audio_headphone_analog_mute(ctx->audio, 1);

		//Init RX dma
		audio_set_rx_dma_buffer(ctx->audio, dma_rxdata, RX_PAGE_SIZE);
		audio_rx_irq_handler(ctx->audio, audio_rx_complete, (uint32_t)ctx);
		//Init TX dma
		audio_set_tx_dma_buffer(ctx->audio, dma_txdata, TX_PAGE_SIZE);
		audio_tx_irq_handler(ctx->audio, audio_tx_complete, (uint32_t)ctx);
#endif
		audio_set_param(ctx->audio, ctx->params.sample_rate, ctx->params.word_length);  // ASR_8KHZ, ASR_16KHZ //ASR_48KHZ
		printf("sample rate = %d, %d\r\n", ctx->params.sample_rate, audio_get_samplerate(ctx->params.sample_rate));

#if defined(CONFIG_PLATFORM_8735B)
		for (int i = 0; i < (AUDIO_DMA_PAGE_NUM - 1); i++) {
			uint8_t *ptx_buf_reset = audio_get_tx_page_adr(ctx->audio);
			if (ptx_buf_reset) {
				memset(ptx_buf_reset, 0x0, TX_PAGE_SIZE);
				audio_set_tx_page(ctx->audio, ptx_buf_reset);
			}
			audio_set_rx_page(ctx->audio);
		}

		//Set up hpf for mic
#if defined(SAVE_AUDIO_DATA) && SAVE_AUDIO_DATA
		audio_adc_l_hpf(ctx->audio, 1, ctx->params.hpf_set);
		audio_adc_r_hpf(ctx->audio, 1, ctx->params.hpf_set);
		printf("hpf_set = %d\r\n", ctx->params.hpf_set);
#else
		audio_adc_l_hpf(ctx->audio, 1, 0);
		audio_adc_r_hpf(ctx->audio, 1, 0);
#endif

#if defined(SAVE_AUDIO_DATA) && SAVE_AUDIO_DATA
		if (ctx->params.use_mic_type == USE_AUDIO_LEFT_DMIC || ctx->params.use_mic_type == USE_AUDIO_STEREO_DMIC || ctx->params.use_mic_type == USE_AUDIO_AMIC) {
			printf("left digital mic or analog mic set\r\n");
			for (uint32_t j = 0; j < 5; j++) {
				if (ctx->params.mic_l_eq[j].eq_enable) {
					audio_input_l_eq(ctx->audio, j, 1, ctx->params.mic_l_eq[j].eq_h0, ctx->params.mic_l_eq[j].eq_a1, ctx->params.mic_l_eq[j].eq_a2, ctx->params.mic_l_eq[j].eq_b1,
									 ctx->params.mic_l_eq[j].eq_b2);
					printf("OPEN EQ:0x%lx\r\nh0:0x%lx\r\na1:0x%lx\r\na2:0x%lx\r\nb1:0x%lx\r\nb2:0x%lx\r\n", j, ctx->params.mic_l_eq[j].eq_h0, ctx->params.mic_l_eq[j].eq_a1,
						   ctx->params.mic_l_eq[j].eq_a2, ctx->params.mic_l_eq[j].eq_b1, ctx->params.mic_l_eq[j].eq_b2);
				} else {
					audio_input_l_eq(ctx->audio, j, 0, 0, 0, 0, 0, 0);
					printf("CLOSE EQ:0x%lx\r\n", j);
				}
			}
		}

		if (ctx->params.use_mic_type == USE_AUDIO_RIGHT_DMIC || ctx->params.use_mic_type == USE_AUDIO_STEREO_DMIC) {
			printf("right digital dmic set\r\n");
			for (uint32_t j = 0; j < 5; j++) {
				if (ctx->params.mic_r_eq[j].eq_enable) {
					audio_input_r_eq(ctx->audio, j, 1, ctx->params.mic_r_eq[j].eq_h0, ctx->params.mic_r_eq[j].eq_a1, ctx->params.mic_r_eq[j].eq_a2, ctx->params.mic_r_eq[j].eq_b1,
									 ctx->params.mic_r_eq[j].eq_b2);
					printf("OPEN EQ:0x%lx\r\nh0:0x%lx\r\na1:0x%lx\r\na2:0x%lx\r\nb1:0x%lx\r\nb2:0x%lx\r\n", j, ctx->params.mic_r_eq[j].eq_h0, ctx->params.mic_r_eq[j].eq_a1,
						   ctx->params.mic_r_eq[j].eq_a2, ctx->params.mic_r_eq[j].eq_b1, ctx->params.mic_r_eq[j].eq_b2);
				} else {
					audio_input_r_eq(ctx->audio, j, 0, 0, 0, 0, 0, 0);
					printf("CLOSE EQ:0x%lx\r\n", j);
				}
			}
		}

		printf("speaker set\r\n");
		for (uint32_t j = 0; j < 5; j++) {
			if (ctx->params.spk_l_eq[j].eq_enable) {
				audio_output_l_eq(ctx->audio, j, 1, ctx->params.spk_l_eq[j].eq_h0, ctx->params.spk_l_eq[j].eq_a1, ctx->params.spk_l_eq[j].eq_a2, ctx->params.spk_l_eq[j].eq_b1,
								  ctx->params.spk_l_eq[j].eq_b2);
				printf("OPEN EQ:0x%lx\r\nh0:0x%lx\r\na1:0x%lx\r\na2:0x%lx\r\nb1:0x%lx\r\nb2:0x%lx\r\n", j, ctx->params.spk_l_eq[j].eq_h0, ctx->params.spk_l_eq[j].eq_a1,
					   ctx->params.spk_l_eq[j].eq_a2, ctx->params.spk_l_eq[j].eq_b1, ctx->params.spk_l_eq[j].eq_b2);
			} else {
				audio_output_l_eq(ctx->audio, j, 0, 0, 0, 0, 0, 0);
				printf("CLOSE EQ:0x%lx\r\n", j);
			}
		}
#else
		//Set up Left mic EQ
		if (ctx->params.use_mic_type == USE_AUDIO_LEFT_DMIC || ctx->params.use_mic_type == USE_AUDIO_STEREO_DMIC || ctx->params.use_mic_type == USE_AUDIO_AMIC) {
			if (ctx->params.sample_rate == ASR_8KHZ) {
				audio_input_l_eq(ctx->audio, 0, 1, 0x1ca2925, 0x1c000000, 0x2000000, 0x38ea551, 0x1e6600bf); //Setup first EQ high pass filter with cutoff 200HZ
			} else if (ctx->params.sample_rate == ASR_16KHZ) {
				audio_input_l_eq(ctx->audio, 0, 1, 0x1e45618, 0x1c000000, 0x2000000, 0x3c72d61, 0x1e35d500); //Setup first EQ high pass filter with cutoff 200HZ
			}
			//audio_input_l_eq(ctx->audio, 1, 0, 0x00, 0x00, 0x00, 0x00, 0x00);
			//audio_input_l_eq(ctx->audio, 2, 0, 0x00, 0x00, 0x00, 0x00, 0x00);
			//audio_input_l_eq(ctx->audio, 3, 0, 0x00, 0x00, 0x00, 0x00, 0x00);
			//audio_input_l_eq(ctx->audio, 4, 0, 0x00, 0x00, 0x00, 0x00, 0x00);
		}

		//Set up Right mic EQ
		if (ctx->params.use_mic_type == USE_AUDIO_RIGHT_DMIC || ctx->params.use_mic_type == USE_AUDIO_STEREO_DMIC) {
			if (ctx->params.sample_rate == ASR_8KHZ) {
				audio_input_r_eq(ctx->audio, 0, 1, 0x1ca2925, 0x1c000000, 0x2000000, 0x38ea551, 0x1e6600bf); //Setup first EQ high pass filter with cutoff 200HZ
			} else if (ctx->params.sample_rate == ASR_16KHZ) {
				audio_input_r_eq(ctx->audio, 0, 1, 0x1e45618, 0x1c000000, 0x2000000, 0x3c72d61, 0x1e35d500); //Setup first EQ high pass filter with cutoff 200HZ
			}
			//audio_input_r_eq(ctx->audio, 1, 0, 0x00, 0x00, 0x00, 0x00, 0x00);
			//audio_input_r_eq(ctx->audio, 2, 0, 0x00, 0x00, 0x00, 0x00, 0x00);
			//audio_input_r_eq(ctx->audio, 3, 0, 0x00, 0x00, 0x00, 0x00, 0x00);
			//audio_input_r_eq(ctx->audio, 4, 0, 0x00, 0x00, 0x00, 0x00, 0x00);
		}

		//Set up Left Speaker EQ
		if (ctx->params.sample_rate == ASR_8KHZ) {
			audio_output_l_eq(ctx->audio, 0, 1, 0x1ca2925, 0x1c000000, 0x2000000, 0x38ea551, 0x1e6600bf); //Setup first EQ high pass filter with cutoff 200HZ
		} else if (ctx->params.sample_rate == ASR_16KHZ) {
			audio_output_l_eq(ctx->audio, 0, 1, 0x1e45618, 0x1c000000, 0x2000000, 0x3c72d61, 0x1e35d500); //Setup first EQ high pass filter with cutoff 200HZ
		}
		//audio_output_l_eq(ctx->audio, 1, 0, 0x00, 0x00, 0x00, 0x00, 0x00);
		//audio_output_l_eq(ctx->audio, 2, 0, 0x00, 0x00, 0x00, 0x00, 0x00);
		//audio_output_l_eq(ctx->audio, 3, 0, 0x00, 0x00, 0x00, 0x00, 0x00);
		//audio_output_l_eq(ctx->audio, 4, 0, 0x00, 0x00, 0x00, 0x00, 0x00);
#endif
		if (ctx->params.use_mic_type == USE_AUDIO_AMIC) { // AMIC
#if defined(SAVE_AUDIO_DATA) && SAVE_AUDIO_DATA
			audio_mic_bias_ctrl(ctx->audio, 1, ctx->params.mic_bias);
			audio_mic_analog_gain(ctx->audio, 1, ctx->params.mic_gain);
			printf("set AMIC bias = %d, gain = %d\r\n", ctx->params.mic_bias, ctx->params.mic_gain);
#else
			audio_mic_analog_gain(ctx->audio, 1, ctx->params.mic_gain); // default 0DB
#endif
		} else if (ctx->params.use_mic_type == USE_AUDIO_LEFT_DMIC) { // LEFT DMIC
			audio_mic_analog_gain(ctx->audio, 1, ctx->params.mic_gain);
			audio_l_dmic_gain(ctx->audio, ctx->params.dmic_l_gain);
			printf("set LEFT DMIC gain = %d\r\n", ctx->params.dmic_l_gain);
		} else if (ctx->params.use_mic_type == USE_AUDIO_RIGHT_DMIC) { // RIGHT DMIC
			audio_mic_analog_gain(ctx->audio, 1, ctx->params.mic_gain);
			audio_r_dmic_gain(ctx->audio, ctx->params.dmic_r_gain);
			printf("set RIGHT DMIC gain = %d\r\n", ctx->params.dmic_r_gain);
		} else if (ctx->params.use_mic_type == USE_AUDIO_STEREO_DMIC) { // STEREO DMIC
			audio_mic_analog_gain(ctx->audio, 1, ctx->params.mic_gain);
			audio_l_dmic_gain(ctx->audio, ctx->params.dmic_l_gain);
			audio_r_dmic_gain(ctx->audio, ctx->params.dmic_r_gain);
			printf("set LEFT/RIGHT DMIC gain = %d/%d\r\n", ctx->params.dmic_l_gain, ctx->params.dmic_r_gain);
		}
#else
		audio_mic_analog_gain(ctx->audio, 1, ctx->params.mic_gain); // default 0DB
#endif
		audio_trx_start(ctx->audio);
		break;
#endif
	case CMD_AUDIO_APPLY:
#if defined(CONFIG_PLATFORM_8735B)
		if (ctx->params.use_mic_type == USE_AUDIO_AMIC) { // AMIC
			printf("Init AMIC \r\n");
			//Audio Init
			audio_init(ctx->audio, OUTPUT_SINGLE_EDNED, MIC_SINGLE_EDNED, AUDIO_CODEC_2p8V);
		} else if (ctx->params.use_mic_type == USE_AUDIO_LEFT_DMIC) {
			if (ctx->dmic_pin_set == 0) {
				audio_dmic_pinmux(ctx->audio, DMIC_CLK_PIN, DMIC_DATA_PIN);
				ctx->dmic_pin_set = 1;
			}
			audio_init(ctx->audio, OUTPUT_SINGLE_EDNED, AUDIO_LEFT_DMIC, AUDIO_CODEC_2p8V);
			printf("Init LEFT DMIC \r\n");
		} else if (ctx->params.use_mic_type == USE_AUDIO_RIGHT_DMIC) {
			if (ctx->dmic_pin_set == 0) {
				audio_dmic_pinmux(ctx->audio, DMIC_CLK_PIN, DMIC_DATA_PIN);
				ctx->dmic_pin_set = 1;
			}
			audio_init(ctx->audio, OUTPUT_SINGLE_EDNED, AUDIO_RIGHT_DMIC, AUDIO_CODEC_2p8V);
			printf("Init RIGHT DMIC \r\n");
		} else if (ctx->params.use_mic_type == USE_AUDIO_STEREO_DMIC) {
			if (ctx->dmic_pin_set == 0) {
				audio_dmic_pinmux(ctx->audio, DMIC_CLK_PIN, DMIC_DATA_PIN);
				ctx->dmic_pin_set = 1;
			}
			audio_init(ctx->audio, OUTPUT_SINGLE_EDNED, AUDIO_STEREO_DMIC, AUDIO_CODEC_2p8V);
			printf("Init STEREO DMIC \r\n");
		} else {
			printf("unsupported MIC type \r\n");
			goto audio_control_fail;
		}
		audio_set_dma_buffer(ctx->audio, dma_txdata, dma_rxdata, AUDIO_DMA_PAGE_SIZE, AUDIO_DMA_PAGE_NUM);
#if defined(SAVE_AUDIO_DATA) && SAVE_AUDIO_DATA
		audio_adc_digital_vol(ctx->audio, ctx->params.ADC_gain);
		audio_dac_digital_vol(ctx->audio, ctx->params.DAC_gain);
#else
		audio_adc_digital_vol(ctx->audio, 0x7F);  //Mic DGain 30dB
		audio_dac_digital_vol(ctx->audio, 0xAF);  //Spk DGain 0dB
#endif
		//Init RX dma
		audio_rx_irq_handler(ctx->audio, audio_rx_complete, (uint32_t)ctx);
		//Init TX dma
		audio_tx_irq_handler(ctx->audio, audio_tx_complete, (uint32_t)ctx);
#else
		audio_init(ctx->audio, OUTPUT_SINGLE_EDNED, MIC_SINGLE_EDNED, AUDIO_CODEC_2p8V);
		//audio_mic_analog_gain(ctx->audio, 1, AUDIO_MIC_40DB);
		audio_adc_digital_vol(ctx->audio, 0x7F);
		audio_dac_digital_vol(ctx->audio, 0xAF);

		//audio_headphone_analog_mute(ctx->audio, 1);
		//Init RX dma
		audio_set_rx_dma_buffer(ctx->audio, dma_rxdata, RX_PAGE_SIZE);
		audio_rx_irq_handler(ctx->audio, audio_rx_complete, (uint32_t)ctx);
		//Init TX dma
		audio_set_tx_dma_buffer(ctx->audio, dma_txdata, TX_PAGE_SIZE);
		audio_tx_irq_handler(ctx->audio, audio_tx_complete, (uint32_t)ctx);
#endif
#if ENABLE_ASP==1
		if (ctx->params.enable_aec || (ctx->params.enable_ns & 0x2) || ctx->params.enable_vad || (ctx->params.enable_agc & 0x2)) {
			ctx->aec_rx_done_sema = xSemaphoreCreateBinary();
			if (!ctx->aec_rx_done_sema) {
				goto audio_control_fail;
			}
			audio_rx_buf = xQueueCreate(RX_CACHE_DEPTH, AUDIO_DMA_PAGE_SIZE * sizeof(uint8_t));
			if (!audio_rx_buf) {
				goto audio_control_fail;
			}
#if defined(SAVE_AUDIO_DATA) && SAVE_AUDIO_DATA
			if (xTaskCreate(audio_rx_handle_thread, ((const char *)"audio_rx"), 24 * 1024, (void *)ctx, tskIDLE_PRIORITY + 5, &ctx->aec_rx_task) != pdPASS) {
#else
			if (xTaskCreate(audio_rx_handle_thread, ((const char *)"audio_rx"), 16 * 1024, (void *)ctx, tskIDLE_PRIORITY + 5, &ctx->aec_rx_task) != pdPASS) {
#endif
				rt_printf("\r\n audio_rx_handle_thread: Create Task Error\n");
				goto audio_control_fail;
			}
		}
		if (ctx->params.sample_rate == ASR_8KHZ || ctx->params.sample_rate == ASR_16KHZ) {
			sample_rate = audio_get_samplerate(ctx->params.sample_rate);
			if (ctx->params.enable_ns & 0x1) {
#if defined(SAVE_AUDIO_DATA) && SAVE_AUDIO_DATA
				if (ctx->params.NS_level_SPK != -1) {
					NS_init(sample_rate, ctx->params.NS_level_SPK);
					printf("Inintial NS \r\n");
					ctx->inited_ns |= 0x1;
					ctx->run_ns |= 0x1;
					printf("Set Speaker NS level %d\r\n", ctx->params.NS_level_SPK);
				} else {
					printf("NO Speaker NS SET\r\n");
				}
#else
				NS_init(sample_rate, NS_MODE);
				printf("Inintial NS \r\n");
				ctx->inited_ns |= 0x1;
				ctx->run_ns |= 0x1;
#endif
			}
			if (ctx->params.enable_agc & 0x1) {
				AGC_init(sample_rate, 2, 24, 0);
				printf("Inintial AGC \r\n");
				ctx->inited_agc |= 0x1;
				ctx->run_agc |= 0x1;
			}
		}
#elif ENABLE_SPEEX_AEC==1
		if (ctx->params.enable_aec) {
			ctx->aec_rx_done_sema = xSemaphoreCreateBinary();
			if (!ctx->aec_rx_done_sema) {
				goto audio_control_fail;
			}
			if (xTaskCreate(audio_rx_handle_thread, ((const char *)"audio_rx"), 256, (void *)ctx, tskIDLE_PRIORITY + 1, &ctx->aec_rx_task) != pdPASS) {
				rt_printf("\r\n audio_rx_handle_thread: Create Task Error\n");
				goto audio_control_fail;
			}
		}
#endif
		// mix mode --> LOGIC_INPUT_NUM input, non-mix mode 1 input
		if (ctx->params.mix_mode) {
			logic_input_num = LOGIC_INPUT_NUM;
		}
		pcm_tx_cache = malloc(logic_input_num * sizeof(pcm_tx_cache_t));
		if (!pcm_tx_cache) {
			rt_printf("\r\n pcm tx cache: Create Error\n");
			goto audio_control_fail;
		}
		memset(pcm_tx_cache, 0, logic_input_num * sizeof(pcm_tx_cache_t));
		for (int i = 0; i < logic_input_num; i++) {
			memset(pcm_tx_cache[i].txbuf, 0x80, AUDIO_DMA_PAGE_SIZE);	// set to audio level 0;
			pcm_tx_cache[i].queue = xQueueCreate(AUDIO_TX_PCM_QUEUE_LENGTH, AUDIO_DMA_PAGE_SIZE);
			if (!pcm_tx_cache[i].queue) {
				goto audio_control_fail;
			}
		}

#if defined(CONFIG_PLATFORM_8735B)
		ctx->sample_rate = audio_get_samplerate(ctx->params.sample_rate);

		printf("sample rate = %d, %d\r\n", ctx->params.sample_rate, audio_get_samplerate(ctx->params.sample_rate));
		if (ctx->params.word_length == WL_16BIT) {
			ctx->word_length = 2;
		} else if (ctx->params.word_length == WL_24BIT) {
			ctx->word_length = 3;
		}
		audio_set_param(ctx->audio, ctx->params.sample_rate, ctx->params.word_length);  // ASR_8KHZ, ASR_16KHZ //ASR_48KHZ
		// Use (DMA page count -1) because occur RX interrupt in first //
		for (int i = 0; i < (AUDIO_DMA_PAGE_NUM - 1); i++) {
			uint8_t *ptx_buf = audio_get_tx_page_adr(ctx->audio);
			if (ptx_buf) {
				memset(ptx_buf, 0x0, TX_PAGE_SIZE);
				audio_set_tx_page(ctx->audio, ptx_buf);
			}
			audio_set_rx_page(ctx->audio);
		}

		//Set up hpf for mic
#if defined(SAVE_AUDIO_DATA) && SAVE_AUDIO_DATA
		audio_adc_l_hpf(ctx->audio, 1, ctx->params.hpf_set);
		audio_adc_r_hpf(ctx->audio, 1, ctx->params.hpf_set);
		printf("hpf_set = %d\r\n", ctx->params.hpf_set);
#else
		audio_adc_l_hpf(ctx->audio, 1, 0);
		audio_adc_r_hpf(ctx->audio, 1, 0);
#endif

#if defined(SAVE_AUDIO_DATA) && SAVE_AUDIO_DATA
		if (ctx->params.use_mic_type == USE_AUDIO_LEFT_DMIC || ctx->params.use_mic_type == USE_AUDIO_STEREO_DMIC || ctx->params.use_mic_type == USE_AUDIO_AMIC) {
			printf("left digital mic or analog mic set\r\n");
			for (uint32_t j = 0; j < 5; j++) {
				if (ctx->params.mic_l_eq[j].eq_enable) {
					audio_input_l_eq(ctx->audio, j, 1, ctx->params.mic_l_eq[j].eq_h0, ctx->params.mic_l_eq[j].eq_a1, ctx->params.mic_l_eq[j].eq_a2, ctx->params.mic_l_eq[j].eq_b1,
									 ctx->params.mic_l_eq[j].eq_b2);
					printf("OPEN EQ:0x%lx\r\nh0:0x%lx\r\na1:0x%lx\r\na2:0x%lx\r\nb1:0x%lx\r\nb2:0x%lx\r\n", j, ctx->params.mic_l_eq[j].eq_h0, ctx->params.mic_l_eq[j].eq_a1,
						   ctx->params.mic_l_eq[j].eq_a2, ctx->params.mic_l_eq[j].eq_b1, ctx->params.mic_l_eq[j].eq_b2);
				} else {
					audio_input_l_eq(ctx->audio, j, 0, 0, 0, 0, 0, 0);
					printf("CLOSE EQ:0x%lx\r\n", j);
				}
			}
		}

		if (ctx->params.use_mic_type == USE_AUDIO_RIGHT_DMIC || ctx->params.use_mic_type == USE_AUDIO_STEREO_DMIC) {
			printf("right digital dmic set\r\n");
			for (uint32_t j = 0; j < 5; j++) {
				if (ctx->params.mic_r_eq[j].eq_enable) {
					audio_input_r_eq(ctx->audio, j, 1, ctx->params.mic_r_eq[j].eq_h0, ctx->params.mic_r_eq[j].eq_a1, ctx->params.mic_r_eq[j].eq_a2, ctx->params.mic_r_eq[j].eq_b1,
									 ctx->params.mic_r_eq[j].eq_b2);
					printf("OPEN EQ:0x%lx\r\nh0:0x%lx\r\na1:0x%lx\r\na2:0x%lx\r\nb1:0x%lx\r\nb2:0x%lx\r\n", j, ctx->params.mic_r_eq[j].eq_h0, ctx->params.mic_r_eq[j].eq_a1,
						   ctx->params.mic_r_eq[j].eq_a2, ctx->params.mic_r_eq[j].eq_b1, ctx->params.mic_r_eq[j].eq_b2);
				} else {
					audio_input_r_eq(ctx->audio, j, 0, 0, 0, 0, 0, 0);
					printf("CLOSE EQ:0x%lx\r\n", j);
				}
			}
		}

		printf("speaker set\r\n");
		for (uint32_t j = 0; j < 5; j++) {
			if (ctx->params.spk_l_eq[j].eq_enable) {
				audio_output_l_eq(ctx->audio, j, 1, ctx->params.spk_l_eq[j].eq_h0, ctx->params.spk_l_eq[j].eq_a1, ctx->params.spk_l_eq[j].eq_a2, ctx->params.spk_l_eq[j].eq_b1,
								  ctx->params.spk_l_eq[j].eq_b2);
				printf("OPEN EQ:0x%lx\r\nh0:0x%lx\r\na1:0x%lx\r\na2:0x%lx\r\nb1:0x%lx\r\nb2:0x%lx\r\n", j, ctx->params.spk_l_eq[j].eq_h0, ctx->params.spk_l_eq[j].eq_a1,
					   ctx->params.spk_l_eq[j].eq_a2, ctx->params.spk_l_eq[j].eq_b1, ctx->params.spk_l_eq[j].eq_b2);
			} else {
				audio_output_l_eq(ctx->audio, j, 0, 0, 0, 0, 0, 0);
				printf("CLOSE EQ:0x%lx\r\n", j);
			}
		}
#else
		//Set up Left mic EQ
		if (ctx->params.use_mic_type == USE_AUDIO_LEFT_DMIC || ctx->params.use_mic_type == USE_AUDIO_STEREO_DMIC || ctx->params.use_mic_type == USE_AUDIO_AMIC) {
			if (ctx->params.sample_rate == ASR_8KHZ) {
				audio_input_l_eq(ctx->audio, 0, 1, 0x1ca2925, 0x1c000000, 0x2000000, 0x38ea551, 0x1e6600bf); //Setup first EQ high pass filter with cutoff 200HZ
			} else if (ctx->params.sample_rate == ASR_16KHZ) {
				audio_input_l_eq(ctx->audio, 0, 1, 0x1e45618, 0x1c000000, 0x2000000, 0x3c72d61, 0x1e35d500); //Setup first EQ high pass filter with cutoff 200HZ
			}
			//audio_input_l_eq(ctx->audio, 1, 0, 0x00, 0x00, 0x00, 0x00, 0x00);
			//audio_input_l_eq(ctx->audio, 2, 0, 0x00, 0x00, 0x00, 0x00, 0x00);
			//audio_input_l_eq(ctx->audio, 3, 0, 0x00, 0x00, 0x00, 0x00, 0x00);
			//audio_input_l_eq(ctx->audio, 4, 0, 0x00, 0x00, 0x00, 0x00, 0x00);
		}

		//Set up Right mic EQ
		if (ctx->params.use_mic_type == USE_AUDIO_RIGHT_DMIC || ctx->params.use_mic_type == USE_AUDIO_STEREO_DMIC) {
			if (ctx->params.sample_rate == ASR_8KHZ) {
				audio_input_r_eq(ctx->audio, 0, 1, 0x1ca2925, 0x1c000000, 0x2000000, 0x38ea551, 0x1e6600bf); //Setup first EQ high pass filter with cutoff 200HZ
			} else if (ctx->params.sample_rate == ASR_16KHZ) {
				audio_input_r_eq(ctx->audio, 0, 1, 0x1e45618, 0x1c000000, 0x2000000, 0x3c72d61, 0x1e35d500); //Setup first EQ high pass filter with cutoff 200HZ
			}
			//audio_input_r_eq(ctx->audio, 1, 0, 0x00, 0x00, 0x00, 0x00, 0x00);
			//audio_input_r_eq(ctx->audio, 2, 0, 0x00, 0x00, 0x00, 0x00, 0x00);
			//audio_input_r_eq(ctx->audio, 3, 0, 0x00, 0x00, 0x00, 0x00, 0x00);
			//audio_input_r_eq(ctx->audio, 4, 0, 0x00, 0x00, 0x00, 0x00, 0x00);
		}

		//Set up Left Speaker EQ
		if (ctx->params.sample_rate == ASR_8KHZ) {
			audio_output_l_eq(ctx->audio, 0, 1, 0x1ca2925, 0x1c000000, 0x2000000, 0x38ea551, 0x1e6600bf); //Setup first EQ high pass filter with cutoff 200HZ
		} else if (ctx->params.sample_rate == ASR_16KHZ) {
			audio_output_l_eq(ctx->audio, 0, 1, 0x1e45618, 0x1c000000, 0x2000000, 0x3c72d61, 0x1e35d500); //Setup first EQ high pass filter with cutoff 200HZ
		}
		//audio_output_l_eq(ctx->audio, 1, 0, 0x00, 0x00, 0x00, 0x00, 0x00);
		//audio_output_l_eq(ctx->audio, 2, 0, 0x00, 0x00, 0x00, 0x00, 0x00);
		//audio_output_l_eq(ctx->audio, 3, 0, 0x00, 0x00, 0x00, 0x00, 0x00);
		//audio_output_l_eq(ctx->audio, 4, 0, 0x00, 0x00, 0x00, 0x00, 0x00);
#endif
		if (ctx->params.use_mic_type == USE_AUDIO_AMIC) { // AMIC
#if defined(SAVE_AUDIO_DATA) && SAVE_AUDIO_DATA
			audio_mic_bias_ctrl(ctx->audio, 1, ctx->params.mic_bias);
			audio_mic_analog_gain(ctx->audio, 1, ctx->params.mic_gain);
			printf("set AMIC bias = %d, gain = %d\r\n", ctx->params.mic_bias, ctx->params.mic_gain);
#else
			audio_mic_analog_gain(ctx->audio, 1, ctx->params.mic_gain); // default 0DB
#endif
		} else if (ctx->params.use_mic_type == USE_AUDIO_LEFT_DMIC) { // LEFT DMIC
			audio_mic_analog_gain(ctx->audio, 1, ctx->params.mic_gain);
			audio_l_dmic_gain(ctx->audio, ctx->params.dmic_l_gain);
			printf("set LEFT DMIC gain = %d\r\n", ctx->params.dmic_l_gain);
		} else if (ctx->params.use_mic_type == USE_AUDIO_RIGHT_DMIC) { // RIGHT DMIC
			audio_mic_analog_gain(ctx->audio, 1, ctx->params.mic_gain);
			audio_r_dmic_gain(ctx->audio, ctx->params.dmic_r_gain);
			printf("set RIGHT DMIC gain = %d\r\n", ctx->params.dmic_r_gain);
		} else if (ctx->params.use_mic_type == USE_AUDIO_STEREO_DMIC) { // STEREO DMIC
			audio_mic_analog_gain(ctx->audio, 1, ctx->params.mic_gain);
			audio_l_dmic_gain(ctx->audio, ctx->params.dmic_l_gain);
			audio_r_dmic_gain(ctx->audio, ctx->params.dmic_r_gain);
			printf("set LEFT/RIGHT DMIC gain = %d/%d\r\n", ctx->params.dmic_l_gain, ctx->params.dmic_r_gain);
		}
#else
		if (ctx->params.sample_rate == ASR_8KHZ) {
			ctx->sample_rate = 8000;
		} else if (ctx->params.sample_rate == AUDIO_SR_16KHZ) {
			ctx->sample_rate = 16000;
		} else if (ctx->params.sample_rate == AUDIO_SR_32KHZ) {
			ctx->sample_rate = 32000;
		} else if (ctx->params.sample_rate == AUDIO_SR_44p1KHZ) {
			ctx->sample_rate = 44100;
		} else if (ctx->params.sample_rate == AUDIO_SR_48KHZ) {
			ctx->sample_rate = 48000;
		} else if (ctx->params.sample_rate == AUDIO_SR_88p2KHZ) {
			ctx->sample_rate = 88200;
		} else if (ctx->params.sample_rate == AUDIO_SR_96KHZ) {
			ctx->sample_rate = 96000;
		}


		if (ctx->params.word_length == WL_8BIT) {
			ctx->word_length = 1;
		} else if (ctx->params.word_length == WL_16BIT) {
			ctx->word_length = 2;
		} else if (ctx->params.word_length == WL_24BIT) {
			ctx->word_length = 3;
		}
		audio_set_param(ctx->audio, ctx->params.sample_rate, ctx->params.word_length);  // ASR_8KHZ, ASR_16KHZ //ASR_48KHZ
		audio_mic_analog_gain(ctx->audio, 1, ctx->params.mic_gain); // default 0DB
#endif
		audio_trx_start(ctx->audio);
		break;
	}
	return 0;
audio_control_fail:
	mm_printf("audio_control fail\n\r");
	if (ctx->aec_rx_done_sema) {
		vSemaphoreDelete(ctx->aec_rx_done_sema);
		ctx->aec_rx_done_sema = NULL;
	}
#if ENABLE_ASP==1
	if (audio_rx_buf) {
		vQueueDelete(audio_rx_buf);
		audio_rx_buf = NULL;
	}
#endif
	if (ctx->aec_rx_task) {
		vTaskDelete(ctx->aec_rx_task);
		ctx->aec_rx_task = NULL;
	}
	if (pcm_tx_cache) {
		for (int i = 0; i < logic_input_num; i++)
			if (pcm_tx_cache[i].queue) {
				vQueueDelete(pcm_tx_cache[i].queue);
			}
		free(pcm_tx_cache);
	}
	return -1;
}

int audio_handle(void *p, void *input, void *output)
{
	audio_ctx_t *ctx = (audio_ctx_t *)p;
	mm_queue_item_t *input_item = (mm_queue_item_t *)input;
	uint8_t *input_data = (uint8_t *)input_item->data_addr;
	(void)output;
	int cache_idx = input_item->in_idx > (logic_input_num - 1) ? 0 : input_item->in_idx;
	pcm_tx_cache_t *cache = &pcm_tx_cache[cache_idx];
	for (int i = 0; i < input_item->size; i++) {
		cache->buffer[cache->idx++] = input_data[i];
		if (cache->idx == AUDIO_DMA_PAGE_SIZE) {
			//if (ctx->mic_record_file) {
			//ctx->mic_record_file((int16_t *)(cache->buffer));
			//}
#if ENABLE_ASP==1
			if (ctx->params.sample_rate == ASR_8KHZ || ctx->params.sample_rate == ASR_16KHZ) {
				if ((ctx->inited_ns & 0x1) && (ctx->run_ns & 0x1) && (ATAF_NS_CTRL & 0x1)) {
					NS_process(AUDIO_DMA_PAGE_SIZE / sizeof(int16_t), (int16_t *)cache->buffer);
				}
				if ((ctx->inited_agc & 0x1) && (ctx->run_agc & 0x1) && (ATAF_AGC_CTRL & 0x1)) {
					AGC_process(AUDIO_DMA_PAGE_SIZE / sizeof(int16_t), (int16_t *)cache->buffer);
				}
			}
#endif
			if (xQueueSend(cache->queue, cache->buffer, 1000) != pdTRUE) {
				printf("fail to send tx queue\r\n");
			}
			cache->idx = 0;
		}
	}
	//
	return 0;
}

void *audio_destroy(void *p)
{
	audio_ctx_t *ctx = (audio_ctx_t *)p;

	//stop the audio
	audio_tx_stop(ctx->audio);
	audio_rx_stop(ctx->audio);
	audio_deinit(ctx->audio);
	if (pcm_tx_cache) {
		for (int i = 0; i < logic_input_num; i++)
			if (pcm_tx_cache[i].queue) {
				vQueueDelete(pcm_tx_cache[i].queue);
			}
		free(pcm_tx_cache);
	}
#if ENABLE_ASP==1
	if (audio_rx_buf) {
		vQueueDelete(audio_rx_buf);
		audio_rx_buf = NULL;
	}
#endif
	if (ctx && ctx->aec_rx_done_sema) {
		vSemaphoreDelete(ctx->aec_rx_done_sema);
	}
	if (ctx && ctx->aec_rx_task) {
		vTaskDelete(ctx->aec_rx_task);
	}
	if (ctx && ctx->audio) {
		free(ctx->audio);
	}
#if ENABLE_ASP==1
	if (ctx->params.sample_rate == ASR_8KHZ || ctx->params.sample_rate == ASR_16KHZ) {
		if (ctx->inited_ns & 0x1) {
			NS_destory();
		}
		if (ctx->inited_ns & 0x2) {
			NS2_destory();
		}
		if (ctx->inited_agc & 0x1) {
			AGC_destory();
		}
		if (ctx->inited_agc & 0x2) {
			AGC2_destory();
		}
		if (ctx->inited_aec) {
			AEC_destory();
		}
		if (ctx->inited_vad) {
			VAD_destory();
		}
	}
#endif
	if (ctx) {
		free(ctx);
	}
	return NULL;
}

void *audio_create(void *parent)
{
	audio_ctx_t *ctx = malloc(sizeof(audio_ctx_t));
	if (!ctx) {
		return NULL;
	}

	memset(ctx, 0, sizeof(audio_ctx_t));
	ctx->parent = parent;
	ctx->audio = malloc(sizeof(audio_t));
	if (!ctx->audio) {
		goto audio_create_fail;
	}
	memset(ctx->audio, 0, sizeof(audio_t));
#if ENABLE_ASP==1
	memset(last_tx_buf, 0, AUDIO_DMA_PAGE_SIZE);	//MIC_SINGLE_EDNED
	//ctx->run_aec = 0;
	ctx->run_ns = 0;
	ctx->run_agc = 0;
	ctx->run_vad = 0;
	//ctx->inited_aec = 0;
	ctx->inited_ns = 0;
	ctx->inited_agc = 0;
	ctx->inited_vad = 0;
#endif
#if defined(CONFIG_PLATFORM_8735B)
	ctx->dmic_pin_set = 0;
#endif
	return ctx;
audio_create_fail:
	audio_destroy((void *)ctx);
	return NULL;
}

void *audio_new_item(void *p)
{
	//audio_ctx_t* ctx = (audio_ctx_t*)p;
	// get parameter
	void *ptr = malloc(AUDIO_DMA_PAGE_SIZE);
	memset(ptr, 0x0,  AUDIO_DMA_PAGE_SIZE);
	return ptr;
}

void *audio_del_item(void *p, void *d)
{
	if (d) {
		free(d);
	}
	return NULL;
}

mm_module_t audio_module = {
	.create = audio_create,
	.destroy = audio_destroy,
	.control = audio_control,
	.handle = audio_handle,

	.new_item = audio_new_item,
	.del_item = audio_del_item,

	.output_type = MM_TYPE_ADSP | MM_TYPE_ASINK,   // no output
	.module_type = MM_TYPE_ASRC | MM_TYPE_ASINK,
	.name = "AUDIO"
};