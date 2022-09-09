/******************************************************************************
*
* Copyright(c) 2007 - 2018 Realtek Corporation. All rights reserved.
*
******************************************************************************/
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "mmf2_module.h"
#include "module_opusd.h"

#include "opus.h"
#include <FreeRTOS.h>
#include <semphr.h>

//extern xSemaphoreHandle  opus_progress_sema;

//------------------------------------------------------------------------------

void opusd_bypass_parser(void *p, void *input, int len)
{
	opusd_ctx_t *ctx = (opusd_ctx_t *)p;
	int newlen;
	if (len > 0) {
		newlen = opus_packet_unpad(input, len);
		//printf("olen: %d newlen:%d\r\n",len, newlen);
		if (newlen >= 0) {
			if (newlen == 1) {
				memcpy(ctx->data_cache + ctx->data_cache_len, input, newlen);
				ctx->data_cache_len += 1;
				ctx->last_frame_idx ++;
				ctx->frame_len_buf[ctx->last_frame_idx] = 0;
			} else {
				memcpy(ctx->data_cache + ctx->data_cache_len, input, newlen);
				ctx->data_cache_len += newlen;
				ctx->last_frame_idx ++;
				ctx->frame_len_buf[ctx->last_frame_idx] = newlen;
			}
		}
	}
}

int opusd_handle(void *p, void *input, void *output)
{
	opusd_ctx_t *ctx = (opusd_ctx_t *)p;
	mm_queue_item_t *input_item = (mm_queue_item_t *)input;
	mm_queue_item_t *output_item = (mm_queue_item_t *)output;

	//static uint32_t pretimestamp;
	//printf("pre %u, now %u\r\n",pretimestamp,input_item->timestamp);
	/*
	if(pretimestamp == input_item->timestamp)// || input_item->timestamp == 0)
	{
	    pretimestamp = input_item->timestamp;
	    return -1;
	}else{
	    pretimestamp = input_item->timestamp;
	}
	*/

	if (input_item->size == 0)	{
		return -1;
	}

	if (ctx->data_cache_len + input_item->size >= ctx->data_cache_size || ctx->last_frame_idx > ctx->max_frame_in_chache) {
		// This should never happened
		mm_printf("opus data cache overflow %d %d\r\n", ctx->data_cache_len, input_item->size);
		while (1);
	}

	int numofsamplesperframe = 0;
	//int numofframes = 0;
	int numofsamples = 0;
	int outputframesize = 0;
	int remain_len = 0;
	int ret;


	ctx->parser((void *)ctx, (void *)input_item->data_addr, input_item->size);
	remain_len = ctx->data_cache_len;
	/*
	if(ctx->params.with_opus_enc){
	    xSemaphoreTake(opus_progress_sema, portMAX_DELAY);
	}
	*/
	while (1) {

		if (ctx->last_frame_idx < 0 || remain_len <= 0) {
			ctx->last_frame_idx = -1;
			remain_len = 0;
			break;
		}
		numofsamplesperframe = opus_packet_get_samples_per_frame((const unsigned char *)ctx->data_cache, ctx->params.sample_rate);
		numofsamples = opus_packet_get_nb_samples((const unsigned char *)ctx->data_cache, ctx->frame_len_buf[ctx->last_frame_idx], ctx->params.sample_rate);
		//numofframes = opus_packet_get_nb_frames((const unsigned char *)ctx->data_cache, ctx->params.sample_rate);
		if (outputframesize + numofsamples * ctx->params.bit_length / 8  > 1024) {
			break;
		}
		if (numofsamplesperframe > 0) {
			memset(ctx->decode_buf, 0, 1024 * sizeof(int16_t));


			ret = opus_decode(ctx->opus_dec, (const unsigned char *)ctx->data_cache, ctx->frame_len_buf[ctx->last_frame_idx], (opus_int16 *)ctx->decode_buf,
							  numofsamplesperframe, 0);

			if (ret >= 0) {
				memcpy((void *)(output_item->data_addr + outputframesize), (void *)ctx->decode_buf, ret * ctx->params.bit_length / 8);
				memmove(ctx->data_cache, ctx->data_cache + ctx->data_cache_len - ctx->frame_len_buf[ctx->last_frame_idx], ctx->frame_len_buf[ctx->last_frame_idx]);
				if (ctx->frame_len_buf[ctx->last_frame_idx] != 0) {
					remain_len -= ctx->frame_len_buf[ctx->last_frame_idx];
				}
				outputframesize += ret * ctx->params.bit_length / 8;
				memmove(ctx->frame_len_buf, ctx->frame_len_buf + sizeof(uint32_t), ctx->last_frame_idx * sizeof(uint32_t));
				ctx->last_frame_idx --;
			} else if (ret < 0) {
				//printf("ret < 0\r\n");
				memmove(ctx->data_cache, ctx->data_cache + ctx->data_cache_len - ctx->frame_len_buf[ctx->last_frame_idx], ctx->frame_len_buf[ctx->last_frame_idx]);
				if (ctx->frame_len_buf[ctx->last_frame_idx] != 0) {
					remain_len -= ctx->frame_len_buf[ctx->last_frame_idx];
				}
				memmove(ctx->frame_len_buf, ctx->frame_len_buf + sizeof(uint32_t), ctx->last_frame_idx * sizeof(uint32_t));
				ctx->last_frame_idx --;
				break;
			}
		}
		if (ret == 0) {
			break;
		}
	}
	/*
	if(ctx->params.with_opus_enc){
	    xSemaphoreGive(opus_progress_sema);
	}
	*/
	ctx->data_cache_len = remain_len;

	output_item->size = outputframesize;
	output_item->timestamp = input_item->timestamp;
	output_item->index = 0;
	output_item->type = 0;

	return output_item->size;
}

int opusd_control(void *p, int cmd, int arg)
{
	opusd_ctx_t *ctx = (opusd_ctx_t *)p;

	switch (cmd) {
	case CMD_OPUSD_SET_PARAMS:
		memcpy(&ctx->params, ((opusd_params_t *)arg), sizeof(opusd_params_t));
		//ctx->params.samples_input = ((ctx->params.frame_size_in_msec) * (ctx->params.sample_rate))/1000;
		break;
	case CMD_OPUSD_GET_PARAMS:
		memcpy(((opusd_params_t *)arg), &ctx->params, sizeof(opusd_params_t));
		break;
	case CMD_OPUSD_SAMPLERATE:
		ctx->params.sample_rate = arg;
		break;
	case CMD_OPUSD_CHANNEL:
		ctx->params.channel = arg;
		break;
	case CMD_OPUSD_STREAM_TYPE:
		ctx->params.opus_application = arg;
		break;
	case CMD_OPUSD_RESET:
		//AACDeInitDecoder(ctx->opusd);
		//ctx->opusd = AACInitDecoder();
		ctx->data_cache_len = 0;
	case CMD_OPUSD_APPLY:
		opus_decoder_init(ctx->opus_dec, ctx->params.sample_rate, ctx->params.channel);
		ctx->parser = opusd_bypass_parser;
		opus_decoder_ctl(ctx->opus_dec, OPUS_SET_SIGNAL(ctx->params.opus_application));
		opus_decoder_ctl(ctx->opus_dec, OPUS_SET_GAIN(0));


		//opus_decoder_ctl(ctx->opus_dec, OPUS_SET_GAIN(10));

		/*
		if(ctx->params.type == TYPE_RTP_RAW){
			ctx->parser = opusd_rtp_raw_parser;
		}else if(ctx->params.type == TYPE_TS)
			ctx->parser = opusd_ts_parser;
		else
			ctx->parser = opusd_bypass_parser;
		*/

		break;
	}

	return 0;
}

void *opusd_destroy(void *p)
{
	opusd_ctx_t *ctx = (opusd_ctx_t *)p;

	if (ctx && ctx->opus_dec) {
		opus_decoder_destroy(ctx->opus_dec);
	}

	if (ctx && ctx->decode_buf) {
		free(ctx->decode_buf);
	}
	if (ctx && ctx->data_cache)	{
		free(ctx->data_cache);
	}
	if (ctx) {
		free(ctx);
	}

	return NULL;
}

void *opusd_create(void *parent)
{
	int error_code;
	opusd_ctx_t *ctx = (opusd_ctx_t *)malloc(sizeof(opusd_ctx_t));
	if (!ctx) {
		return NULL;
	}
	ctx->parent = parent;

	// no need check return value because of AACInitDecoder implement
	ctx->opus_dec = opus_decoder_create(48000, 1, &error_code);

	if (error_code != 0) { //OPUS_OK
		goto opusd_create_fail;
	}

	ctx->data_cache_size = 1024;
	ctx->data_cache = malloc(ctx->data_cache_size);
	if (!ctx->data_cache) {
		goto opusd_create_fail;
	}

	ctx->max_frame_in_chache = 20;
	ctx->last_frame_idx = -1; //-1 means no frame
	ctx->frame_len_buf = malloc(ctx->max_frame_in_chache * sizeof(uint32_t));

	// AAC_MAX_NCHANS (2) AAC_MAX_NSAMPS (1024) defined in aacdec.h
	ctx->decode_buf = malloc(1 * 1024 * sizeof(int16_t));
	if (!ctx->decode_buf) {
		goto opusd_create_fail;
	}

	ctx->data_cache_len = 0;
	return ctx;

opusd_create_fail:
	opusd_destroy((void *)ctx);
	return NULL;
}

void *opusd_new_item(void *p)
{
	//opusd_ctx_t *ctx = (opusd_ctx_t *)p;

	return malloc(1 * 1024 * sizeof(int16_t));
}

void *opusd_del_item(void *p, void *d)
{
	if (d) {
		free(d);
	}
	return NULL;
}


mm_module_t opusd_module = {
	.create = opusd_create,
	.destroy = opusd_destroy,
	.control = opusd_control,
	.handle = opusd_handle,

	.new_item = opusd_new_item,
	.del_item = opusd_del_item,

	.output_type = MM_TYPE_ASINK | MM_TYPE_ADSP,
	.module_type = MM_TYPE_ADSP,
	.name = "OPUSD"
};
