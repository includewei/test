/**
 ******************************************************************************
 *This file contains sensor configurations for AmebaPro platform
 ******************************************************************************
*/


#ifndef __SENSOR_H
#define __SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif



/**
@code
 The table below gives an overview of the hardware resources supported by each
 AmebaPro EVAL board.
     - Sensor board:

 =================================================================================================================+
   AmebaPro EVAL  | Sensor board |
 =================================================================================================================+
   AmebaPro2-EVAL   |  SC2053      |
 -----------------------------------------------------------------------------------------------------------------+
   AmebaPro2-EVAL   |  SC2336      |
 -----------------------------------------------------------------------------------------------------------------+
   AmebaPro2-EVAL   |  PS5258      |
 -----------------------------------------------------------------------------------------------------------------+
 =================================================================================================================+
@endcode
*/
#define SENSOR_DUMMY        0x00 //For dummy sensor, no support fast camera start
#define SENSOR_SC2336       0x01
#define SENSOR_GC2053       0x02
#define SENSOR_GC4653 		0x03
#define SENSOR_MIS2008      0x04
#define SENSOR_PS5258       0x05 //It don't support the multi sensor for PS5258 now.If you want to use the sensor,please remove it.

#define MULTI_DISABLE       0x00
#define MULTI_ENABLE        0x01

#define MULTI_SENSOR  		MULTI_DISABLE
#define USE_SENSOR      	SENSOR_GC2053

#ifdef __cplusplus
}
#endif


#endif /* __AMEBAPRO_SENSOR_EVAL_H */
