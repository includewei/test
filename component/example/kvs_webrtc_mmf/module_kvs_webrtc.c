/******************************************************************************
*
* Copyright(c) 2007 - 2018 Realtek Corporation. All rights reserved.
*
******************************************************************************/

/* Headers for example */
#include "module_kvs_webrtc.h"
#include "AppMain.h"
#include "AppMediaSrc_AmebaPro2.h"
#include "AppCommon.h"

/* webrtc git version */
#include "kvs_webrtc_version.h"

/* Config for Ameba-Pro */
#include "sample_config_webrtc.h"
#define KVS_QUEUE_DEPTH         20
#define WEBRTC_AUDIO_FRAME_SIZE 256

/* Network */
#include <lwip_netconf.h>
#include "wifi_conf.h"
#include <sntp/sntp.h>
#include "mbedtls/config.h"
#include "mbedtls/platform.h"

uint8_t *ameba_get_ip(void)
{
	return LwIP_GetIP(0);
}

/* Virtual file system */
#include "vfs.h"

/* Audio/Video */
#include "avcodec.h"

kvsWebrtcMediaQueue_t *pkvsWebrtcMediaQ;

static void ameba_platform_init(void)
{
#if defined(MBEDTLS_PLATFORM_C)
	mbedtls_platform_set_calloc_free(calloc, free);
#endif

	sntp_init();
	while (getEpochTimestampInHundredsOfNanos(NULL) < 10000000000000000ULL) {
		vTaskDelay(200 / portTICK_PERIOD_MS);
		printf("[KVS WebRTC module]: waiting get epoch timer\r\n");
	}

	// sd virtual file syetem register
	vfs_init(NULL);
	vfs_user_register("sd", VFS_FATFS, VFS_INF_SD);
}


static void kvs_webrtc_main_thread(void *param)
{
	printf("=== KVS Example Start ===\n\r");

	// display the git version
	printf("[KVS WebRTC module]: webrtc branch name = %s\r\n", webrtc_branch_name);
	printf("[KVS WebRTC module]: webrtc commit hash = %s\r\n", webrtc_commit_hash);

	// ameba platform init
	ameba_platform_init();

	// webrtc main
	WebRTCAppMain(&gAppMediaSrc);

	// sd virtual file syetem unregister
	vfs_user_unregister("sd", VFS_FATFS, VFS_INF_SD);
	vfs_deinit(NULL);

	vTaskDelete(NULL);
}


#ifdef ENABLE_AUDIO_SENDRECV
static void kvs_webrtc_audio_thread(void *param)
{
	kvs_webrtc_ctx_t *ctx = (kvs_webrtc_ctx_t *)param;
	webrtc_audio_buf_t audio_rev_buf;

	while (!ctx->mediaStop) {
		if (xQueueReceive(pkvsWebrtcMediaQ->AudioRecvQueue, &audio_rev_buf, 50 / portTICK_PERIOD_MS) != pdTRUE) {
			continue;    // should not happen
		}

		mm_context_t *mctx = (mm_context_t *)ctx->parent;
		mm_queue_item_t *output_item;
		if (xQueueReceive(mctx->output_recycle, &output_item, 0xFFFFFFFF) == pdTRUE) {
			memcpy((void *)output_item->data_addr, (void *)audio_rev_buf.data_buf, audio_rev_buf.size);
			output_item->size = audio_rev_buf.size;
			output_item->type = audio_rev_buf.type;
			output_item->timestamp = audio_rev_buf.timestamp;
			xQueueSend(mctx->output_ready, (void *)&output_item, 0xFFFFFFFF);
			free(audio_rev_buf.data_buf);
		}
	}

	vTaskDelete(NULL);
}
#endif /* ENABLE_AUDIO_SENDRECV */


void kvsWebrtcVidioSendToQueue(webrtc_video_buf_t *pVideoBuf, kvsWebrtcMediaQueue_t *MediaQ)
{
	if (uxQueueSpacesAvailable(MediaQ->VideoSendQueue) == 0) {
		webrtc_video_buf_t tmp_item;
		xQueueReceive(MediaQ->VideoSendQueue, &tmp_item, 0);
		free(tmp_item.output_buffer);
	}
	xQueueSend(MediaQ->VideoSendQueue, pVideoBuf, 0);
}


void kvsWebrtcAudioSendToQueue(webrtc_audio_buf_t *pAudioBuf, kvsWebrtcMediaQueue_t *MediaQ)
{
	if (uxQueueSpacesAvailable(MediaQ->AudioSendQueue) == 0) {
		webrtc_audio_buf_t tmp_item;
		xQueueReceive(MediaQ->AudioSendQueue, &tmp_item, 0);
		free(tmp_item.data_buf);
	}
	xQueueSend(MediaQ->AudioSendQueue, pAudioBuf, 0);
}


int kvs_webrtc_handle(void *p, void *input, void *output)
{
	kvs_webrtc_ctx_t *ctx = (kvs_webrtc_ctx_t *)p;

	mm_queue_item_t *input_item = (mm_queue_item_t *)input;
	if (!ctx->mediaStop) {
		if (input_item->type == AV_CODEC_ID_H264) {
			webrtc_video_buf_t video_buf;

			video_buf.output_size = input_item->size;
			video_buf.output_buffer_size = video_buf.output_size;
			video_buf.output_buffer = malloc(video_buf.output_size);
			if (!video_buf.output_buffer) {
				printf("fail to allocate memory for webrtc video frame\r\n");
				return 0;
			}
			memcpy(video_buf.output_buffer, (uint8_t *)input_item->data_addr, video_buf.output_size);

			video_buf.timestamp = input_item->timestamp;

			kvsWebrtcVidioSendToQueue(&video_buf, pkvsWebrtcMediaQ);

		} else if ((input_item->type == AV_CODEC_ID_PCMU) || (input_item->type == AV_CODEC_ID_PCMA) || (input_item->type == AV_CODEC_ID_OPUS)) {
			webrtc_audio_buf_t audio_buf;

			audio_buf.size = input_item->size;
			audio_buf.data_buf =  malloc(audio_buf.size);
			if (!audio_buf.data_buf) {
				printf("fail to allocate memory for webrtc audio frame\r\n");
				return 0;
			}
			memcpy(audio_buf.data_buf, (uint8_t *)input_item->data_addr, audio_buf.size);

			audio_buf.timestamp = input_item->timestamp;

			kvsWebrtcAudioSendToQueue(&audio_buf, pkvsWebrtcMediaQ);
		} else {
			printf("[KVS WebRTC module]: input type cannot be handled:%ld\r\n", input_item->type);
		}
	}

	return 0;
}


static void kvsWebrtcMediaQueueInit(kvsWebrtcMediaQueue_t *MediaQ)
{
	MediaQ->VideoSendQueue = xQueueCreate(KVS_QUEUE_DEPTH, sizeof(webrtc_video_buf_t));
	xQueueReset(MediaQ->VideoSendQueue);

	MediaQ->AudioSendQueue = xQueueCreate(KVS_QUEUE_DEPTH * 6, sizeof(webrtc_audio_buf_t));
	xQueueReset(MediaQ->AudioSendQueue);

#if defined(ENABLE_AUDIO_SENDRECV)
	//Create a queue to receive the G711 or Opus audio frame from viewer
	MediaQ->AudioRecvQueue = xQueueCreate(KVS_QUEUE_DEPTH * 6, sizeof(webrtc_audio_buf_t));
	xQueueReset(MediaQ->AudioRecvQueue);
#endif
	printf("[KVS WebRTC module]: media queue inited.\r\n");
}


static void kvsWebrtcMediaQueueFlushDelete(kvsWebrtcMediaQueue_t *MediaQ)
{
	webrtc_video_buf_t video_send_tmp_item;
	while (xQueueReceive(MediaQ->VideoSendQueue, &video_send_tmp_item, 0) == pdTRUE) {
		if (video_send_tmp_item.output_buffer) {
			free(video_send_tmp_item.output_buffer);
		}
	}
	vQueueDelete(MediaQ->VideoSendQueue);

	webrtc_audio_buf_t audio_send_tmp_item;
	while (xQueueReceive(MediaQ->AudioSendQueue, &audio_send_tmp_item, 0) == pdTRUE) {
		if (audio_send_tmp_item.data_buf) {
			free(audio_send_tmp_item.data_buf);
		}
	}
	vQueueDelete(MediaQ->AudioSendQueue);

#if defined(ENABLE_AUDIO_SENDRECV)
	webrtc_audio_buf_t audio_recv_tmp_item;
	while (xQueueReceive(MediaQ->AudioRecvQueue, &audio_recv_tmp_item, 0) == pdTRUE) {
		if (audio_recv_tmp_item.data_buf) {
			free(audio_recv_tmp_item.data_buf);
		}
	}
	vQueueDelete(MediaQ->AudioRecvQueue);
#endif

	printf("[KVS WebRTC module]: media queue deleted.\r\n");
}


int kvs_webrtc_control(void *p, int cmd, int arg)
{
	kvs_webrtc_ctx_t *ctx = (kvs_webrtc_ctx_t *)p;

	switch (cmd) {

	case CMD_KVS_WEBRTC_SET_APPLY:
		if (xTaskCreate(kvs_webrtc_main_thread, ((const char *)"kvs_webrtc_main_thread"), 2048, NULL, tskIDLE_PRIORITY + 1,
						&ctx->kvs_webrtc_module_main_task) != pdPASS) {
			printf("[KVS WebRTC module]: %s xTaskCreate(kvs_webrtc_main_thread) failed\n\r", __FUNCTION__);
		}
#ifdef ENABLE_AUDIO_SENDRECV
		if (xTaskCreate(kvs_webrtc_audio_thread, ((const char *)"kvs_webrtc_audio_thread"), 512, (void *)ctx, tskIDLE_PRIORITY + 1,
						&ctx->kvs_webrtc_module_audio_recv_task) != pdPASS) {
			printf("[KVS WebRTC module]: %s xTaskCreate(kvs_webrtc_audio_thread) failed\n\r", __FUNCTION__);
		}
#endif
		break;
	case CMD_KVS_WEBRTC_STOP:
		quitApp(); //kvs_webrtc_main_thread will be deleted
		ctx->mediaStop = 1; //stop media source and kvs_webrtc_audio_thread will be deleted
		kvsWebrtcMediaQueueFlushDelete(pkvsWebrtcMediaQ); //flush item in media queue
		break;
	}
	return 0;
}


void *kvs_webrtc_destroy(void *p)
{
	kvs_webrtc_ctx_t *ctx = (kvs_webrtc_ctx_t *)p;
	if (ctx) {
		free(ctx);
	}
	if (pkvsWebrtcMediaQ != NULL) {
		free(pkvsWebrtcMediaQ);
		pkvsWebrtcMediaQ = NULL;
	}
	return NULL;
}


void *kvs_webrtc_create(void *parent)
{
	kvs_webrtc_ctx_t *ctx = malloc(sizeof(kvs_webrtc_ctx_t));
	if (!ctx) {
		return NULL;
	}
	memset(ctx, 0, sizeof(kvs_webrtc_ctx_t));
	ctx->parent = parent;

	pkvsWebrtcMediaQ = (kvsWebrtcMediaQueue_t *)malloc(sizeof(kvsWebrtcMediaQueue_t));
	if (!pkvsWebrtcMediaQ) {
		return NULL;
	}
	kvsWebrtcMediaQueueInit(pkvsWebrtcMediaQ);
	printf("[KVS WebRTC module]: module created.\r\n");

	return ctx;
}


void *kvs_webrtc_new_item(void *p)
{
	kvs_webrtc_ctx_t *ctx = (kvs_webrtc_ctx_t *)p;
	(void)ctx;

	return (void *)malloc(WEBRTC_AUDIO_FRAME_SIZE * 2);
}


void *kvs_webrtc_del_item(void *p, void *d)
{
	(void)p;
	if (d) {
		free(d);
	}
	return NULL;
}


mm_module_t kvs_webrtc_module = {
	.create = kvs_webrtc_create,
	.destroy = kvs_webrtc_destroy,
	.control = kvs_webrtc_control,
	.handle = kvs_webrtc_handle,

	.new_item = kvs_webrtc_new_item,
	.del_item = kvs_webrtc_del_item,

	.output_type = MM_TYPE_ASINK,       // output for audio sink
	.module_type = MM_TYPE_AVSINK,      // module type is video algorithm
	.name = "KVS_WebRTC"
};
