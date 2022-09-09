#ifndef _EXAMPLE_AUDIO_LOOP_H_
#define _EXAMPLE_AUDIO_LOOP_H_
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USE_DMIC 1
#define USE_AMIC 2

#define CONFIG_MIC_TYPE USE_AMIC

void example_audio_loop();

#endif