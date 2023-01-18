#ifndef EXAMPLE_MEDIA_UVCD_H
#define EXAMPLE_MEDIA_UVCD_H

#include "sensor.h"
//#include "video_common_api.h"

#define UVCD_TUNNING_MODE  1// 1 :Contain four formats to support tunning tool (YUV2 NV12 H264 JPEG)
// 0 :Contain two formats to support h264 and jpeg format
#define CUSTOMER_AMEBAPRO_ADVANCE 0 //1: Based on the defined "UVCD_TUNNING_MODE", this option "1" is for customer release, "0" is for internal use.
void example_media_uvcd(void);

#if USE_SENSOR == SENSOR_GC4023
#define MAX_W 2560
#define MAX_H 1440
#elif USE_SENSOR == SENSOR_PS5420
#define MAX_W 1936
#define MAX_H 1936
#elif USE_SENSOR == SENSOR_PS5270
#define MAX_W 1536
#define MAX_H 1536
#else
#define MAX_W 1920
#define MAX_H 1080
#endif


#define UVCD_YUY2 1
#define UVCD_NV12 1
#define UVCD_MJPG 1
#define UVCD_H264 1
#define UVCD_H265 1

#define FORMAT_TYPE_YUY2        0
#define FORMAT_TYPE_NV12        1
#define FORMAT_TYPE_MJPEG       2
#define FORMAT_TYPE_H264        3
#define FORMAT_TYPE_H265        4

void example_tuning_rtsp_video_v2_init(void);

#endif /* MMF2_EXAMPLE_H */