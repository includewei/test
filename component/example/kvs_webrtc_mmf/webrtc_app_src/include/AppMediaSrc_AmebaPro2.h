/*
 * Copyright 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
#ifndef __AWS_KVS_WEBRTC_APP_MEDIA_AMEBAPRO2_SRC_INCLUDE__
#define __AWS_KVS_WEBRTC_APP_MEDIA_AMEBAPRO2_SRC_INCLUDE__

#ifdef __cplusplus
extern "C" {
#endif

#include <kvs/webrtc_client.h>
#include "AppConfig.h"
#include "AppError.h"
#include "AppMediaSrc.h"
#include "AppMediaSink.h"

typedef struct {
	uint8_t *output_buffer;
	uint32_t output_buffer_size;
	uint32_t output_size;
	uint32_t timestamp;
} webrtc_video_buf_t;

typedef struct {
	uint8_t *data_buf;
	uint32_t size;
	uint32_t timestamp;
	uint32_t type;
} webrtc_audio_buf_t;


uint64_t getEpochTimestampInHundredsOfNanos(void *pTick);

extern AppMediaSrc gAppMediaSrc;
#ifdef __cplusplus
}
#endif
#endif /* __AWS_KVS_WEBRTC_APP_MEDIA_AMEBAPRO2_SRC_INCLUDE__ */
