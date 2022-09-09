/****************************************************************************
*
*    The MIT License (MIT)
*
*    Copyright (c) 2017 - 2021 Vivante Corporation
*
*    Permission is hereby granted, free of charge, to any person obtaining a
*    copy of this software and associated documentation files (the "Software"),
*    to deal in the Software without restriction, including without limitation
*    the rights to use, copy, modify, merge, publish, distribute, sublicense,
*    and/or sell copies of the Software, and to permit persons to whom the
*    Software is furnished to do so, subject to the following conditions:
*
*    The above copyright notice and this permission notice shall be included in
*    all copies or substantial portions of the Software.
*
*    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
*    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
*    DEALINGS IN THE SOFTWARE.
*
*****************************************************************************/

#ifndef _GC_VIP_COMMON_H
#define _GC_VIP_COMMON_H

#include <vip_lite_common.h>

/* Utility macro. */
typedef vip_uint8_t         *vip_uint8_t_ptr;
typedef vip_float_t         *vip_float_t_ptr;
typedef vip_uint32_t        *vip_uint32_t_ptr;
typedef vip_int32_t         *vip_int32_t_ptr;
typedef vip_char_t          *vip_char_t_ptr;
typedef vip_int8_t          *vip_int8_t_ptr;
typedef vip_int16_t         *vip_int16_t_ptr;
typedef vip_int64_t         *vip_int64_t_ptr;
typedef vip_address_t        phy_address_t;


#define gcOnError(func)  \
    status = func; \
    if (status != VIP_SUCCESS) { \
        goto onError; \
    }

#define gcGoOnError(func)  \
    status = func; \
    goto onError;

#define gcIsNULL(object)  \
    if (VIP_NULL == object) { \
        status = VIP_ERROR_INVALID_ARGUMENTS; \
        goto onError; \
    }

#define GCVIP_ALIGN(n, align) \
( \
    ((n) + ((align) - 1)) & ~((align) - 1) \
)


/*****************features define*********************/

/*
defined for printing 64-bits values. default print long date type.
you should change this define after changing vip_uint64_t and vip_address_t date type.

example, if VIP works in 32-bits system only need support 32-bits address.you can change as follows:
#change vip_uint64_t and vip_address_t defines in vip_lite_common.h
typedef unsigned long    vip_uint64_t;
typedef signed long      vip_int64_t;
typedef unsigned long    vip_address_t;
#change defines PRIx64
#define PRIx64                  "lx"
#define PRId64                  "ld"
*/
#define PRIx64                  "llx"
#define PRId64                  "lld"

/*
   when set to 1, recovery kernel when hardware hang occurs.
*/
#ifndef vpmdENABLE_RECOVERY
#define vpmdENABLE_RECOVERY                0
#endif

/*
   This define specified the number of milliseconds the system will wait
   before it broadcasts the VIP is stuck.  In other words, it will define
   the timeout of any operation that needs to wait for the VIP.
   on FPGA, 1800s as hang by default.
   on silicon, 20s as hang by default
*/
#ifndef vpmdHARDWARE_TIMEOUT
#if defined(FPGA_BUILD)
#define vpmdHARDWARE_TIMEOUT               1800000
#else
#define vpmdHARDWARE_TIMEOUT               20000
#endif
#endif

/*
   Dump register and command buffer when HW hangs.
   set to 1. hang states, commands, module status information.
   set to 2. dump all information, states, commands, module status, AHB register.
*/
#ifndef vpmdENABLE_HANG_DUMP
#define vpmdENABLE_HANG_DUMP                1
#endif

/*
  Enable viplite driver API to wait command buffer compelte which submit by agent driver.
  Agent driver is used for heterogeneous processer. (such as CPU + DSP heterogeneous trigger VIP)
  set to 1, if you use agent driver API(vip_agent_trigger) to trigger hardware and use
            viplite driver API(vip_wait_network) to wait interrupt
*/
#ifndef vpmdENABLE_USE_AGENT_TRIGGER
#define vpmdENABLE_USE_AGENT_TRIGGER        0
#endif

/*
   Dump for hw capture.
   when set to 1, only capture states and command buffer.
   when set to 2, a capture of all states and command buffer,input, output.
*/
#ifndef vpmdENABLE_CAPTURE
#define vpmdENABLE_CAPTURE                  0
#endif

/*
   Dump for hw capture in kernel space.
   when set to 1, capture AHB register write/read in kernel space.
*/
#ifndef vpmdENABLE_CAPTURE_IN_KERNEL
#define vpmdENABLE_CAPTURE_IN_KERNEL        0
#endif

/*
   Dump all states and command.
   when set to 1, dump command buffer in every time when run network.
*/
#ifndef vpmdDUMP_COMMANDS_EVERY_RUN
#define vpmdDUMP_COMMANDS_EVERY_RUN         0
#endif

/*
   This is for DDR-less project. Load NBG from flash.
   when set to 1, you should call vip_create_network() and
        specify type VIP_CREATE_NETWORK_FROM_FLASH to create network.
*/
#ifndef vpmdCREATE_NETWORK_FROM_FLASH
#define vpmdCREATE_NETWORK_FROM_FLASH       0
#endif

/*
   Enable wait-link loop to trigger hardware
   when set to 1, wait-link loop scheme waiting for user command submission.
   when set to 0, write CMD_BUF_XTRL_AHB register to tigger hardware execution user commands.
*/
#ifndef vpmdENABLE_WAIT_LINK_LOOP
#define vpmdENABLE_WAIT_LINK_LOOP           0
#endif

/*
   Enable flush CPU cache for video memory.
   Please set to 1, if video memory with cache.
   Flush video memory heap CPU cache in gckVIP_OS_FlushCache() function at gc_vip_kernel_os.c
*/
#ifndef vpmdENABLE_FLUSH_CPU_CACHE
#define vpmdENABLE_FLUSH_CPU_CACHE          1
#endif

/*
   dump outputs of every layer.
   when set to 1, please make sure you have enabled nn layer dump when generate binary graph.
   export NN_LAYER_DUMP=1 before generating NBG in unified driver.
*/
#ifndef vpmdENABLE_LAYER_DUMP
#define vpmdENABLE_LAYER_DUMP               0
#endif

/*
   Enable layer performance profiling
   when set to 1, the execution time statistics for each operation.
*/
#ifndef vpmdENABLE_CNN_PROFILING
#define vpmdENABLE_CNN_PROFILING            0
#endif

/*
   Enable bandwidth profiling
   when set to 1, output bandwidth information after each submission command buffer.
*/
#ifndef vpmdENABLE_BW_PROFILING
#define vpmdENABLE_BW_PROFILING             0
#endif

/*
   system memory and video memory usage profiling.
   when set to 1, output current and peak memory usage for this NBG.
*/
#ifndef vpmdENABLE_MEMORY_PROFILING
#define vpmdENABLE_MEMORY_PROFILING         0
#endif

/*
   log print out
   when set to 0, disable all logs print out.
   when set to 1, only enable error logs print out.
   when set to 2, enable warning and error log.
   when set to 3, enable info, warning and error logs print out.
   when set to 4, enable debug, info, warning and error logs print out.
*/
#ifndef vpmdENABLE_DEBUG_LOG
#define vpmdENABLE_DEBUG_LOG                1
#endif

/*
   viplite support layers runs on CPU
   when set to 1, enable viplite supports CPU layers.
   when set to 0, disable viplite supports CPU layers.
*/
#ifndef vpmdENABLE_SUPPORT_CPU_LAYER
#define vpmdENABLE_SUPPORT_CPU_LAYER        1
#endif

/*
   viplite use system memory heap to manage memory which used by CPU to store variable and struction.
   when set to 1, enable system memory heap.
   when set to 0, disable system memory heap.
*/
#ifndef vpmdENABLE_SYS_MEMORY_HEAP
#define vpmdENABLE_SYS_MEMORY_HEAP          1
#endif

/*
   viplite use video memory heap to manage memory which used by NPU to store commands and coeff.
   when set to 1, enable video memory heap.
   when set to 0, disable video memory heap. Maybe the VIP's MMU should be enable when video memory heap is disabled.
                  Because VIP needs to allocate memory dynamic from system.
*/

#ifndef vpmdENABLE_VIDEO_MEMORY_HEAP
#define vpmdENABLE_VIDEO_MEMORY_HEAP        1
#endif

/*
   viplite driver supports MMU
   when set to 1, enable VIP's MMU
   when set to 0, disable VIP's MMU
*/
#ifndef vpmdENABLE_MMU
#define vpmdENABLE_MMU                      0 //1 , rtk, no mmu on amebapro2
#endif

/*
   viplite driver supports using broker driver to trigger hardware.
   The broker driver is not included in VIPLite. It is an add on package for VIPLite.

   use case e.g.
        ___________    ___________
       |           |  |           |
       |  TFLite   |  |  Android  |
       |___________|  |___________|
       ___________________________    ______________
      |                           |  |              |
      |          NNRT             |  |   NBG file   |
      |___________________________|  |______________|
       ___________________________    ______________                      __________________________
      |                           |  |              |                    |                          |
      |       NN Compiler         |  |    NBG APP   |                    |                          |
      |___________________________|  |______________|                    |                          |
       _______________________________________________                   |     VIPLite Driver       |
      |                                               |       NBG        |                          |
      |                 VIPLite Broker                |   ------------>  |                          |
      |_______________________________________________|       RPC        |__________________________|

    The VIPLite broker is a library running with NN compiler in an environment.
    It can receive NBG generated by NN compiler(NO-kernel unified driver) and pass NBG buffer to VIPLite driver.
    VIPLite dirver interpreter NBG to do inference.
    This architecture is recommanded use to VIP Secure Solution, Asymmetrical Multiple Processor(CPU AMP) and so on.

   vpmdENABLE_SECURE is enabled VIP secure solution option.
   when set to 1, enable secure solution. using broker driver to trigger hardware.
   and the VIP's interrupts is handled in the Borker driver.
   when set to 0, disable secure solution.
   <NOTE: vpmdENABLE_SECURE is changed from the vpmdENABLE_BROKER >
*/
#ifndef vpmdENABLE_SECURE
#define vpmdENABLE_SECURE                    0
#endif

/*
   viplite driver support mutliple thread(multi-task).

   e.g.1. supports Asynchronous process multi-graph(multi-task) with vip_trigger_network() and vip_wait_network().

   vip_trigger_network(Network_A)
   vip_trigger_network(Network_B)
   CPU do to other tasks.
   vip_wait_network(Network_A)
   vip_wait_network(Netowrk_B)

   e.g.2 support multi-task running in different thread(process).

            Thread A                |            Thread B                   |           Process C
                                    |                                       |
   vip_trigger_network(Network_A)   |     vip_run_network(Network_B)        |      vip_run_network(Network_C)
   cpu to do other tasks.           |                                       |
   vip_wait_network(Network_A)      |                                       |
                                    |                                       |

   when set vpmdENABLE_MULTIPLE_TASK to 1, enable supports multiple thread
   when set vpmdENABLE_MULTIPLE_TASK to 0, disable supports multiple thread

   SUPPORT_MAX_TASK_NUM is the maximum number of task supported.  10 is default value.
*/

#ifndef vpmdENABLE_MULTIPLE_TASK
#define vpmdENABLE_MULTIPLE_TASK             1
#define SUPPORT_MAX_TASK_NUM   10
#endif

/*
    Enable power management. VIPLite power management only support without using wait-link loop to trigger hardware.
    When set to non-zero, VIP will power off automatically from idle state,
          and vpmdPOWER_OFF_TIMEOUT is also the default timeout in milliseconds.
    When set to 0, disable VIP power off feature.
    Default value is 10s power off timer timeout.
*/
#ifndef vpmdPOWER_OFF_TIMEOUT
#define vpmdPOWER_OFF_TIMEOUT              0 //10 * 1000
#endif

/*
    Enable power on/off VIP core when the system suspend and resume.
    The suspend/resume machanism is often used in Linux/Android and other systems.
    When set to 1, enable suspend and resume VIP core feature.
    When set to 0, disable this feature.
    You can disable this feature when VIPLite driver works in RTOS, DSP or bare metal.
*/
#ifndef vpmdENABLE_SUSPEND_RESUME
#define vpmdENABLE_SUSPEND_RESUME           1
#endif

/*
   supports Linux debugfs.
   when set to 1, enable viplite supports debugfs.
   when set to 0, disable viplite supports debugfs.
*/
#ifndef vpmdENABLE_DEBUGFS
#if defined(__linux__)
#define vpmdENABLE_DEBUGFS                  1
#else
#define vpmdENABLE_DEBUGFS                  0
#endif
#endif

/*
    Enable user applicaton can control VIP power. such as application can set VIP frequency,
    stop and re-start VIP, power on and off VIP.
    please refer vip_power_management() API to get more info.
    When set to 1, enable user application control power.
    When set to 0, disable this feature.
*/
#ifndef vpmdENABLE_USER_CONTROL_POWER
#define vpmdENABLE_USER_CONTROL_POWER       1
#endif

/*
    Enable node module. Only scaler engine(hardware scaling) support node module.
*/
#ifndef vpmdENABLE_NODE
#define vpmdENABLE_NODE                     0
#endif

/*
    Enable VIPLite group mode. The group mode can support two application scenarios now.
    a. runs multiple NBG(network) only one interrupt at last of network in group mode.
    b. support batch mode for network in group mode.
    When set to 1, enable group mode.
    When set to 0, disable group mode.
*/
#ifndef vpmdENABLE_GROUP_MODE
#define vpmdENABLE_GROUP_MODE               1
#endif

/*
    Enable VIPLite supports change PPU node parameter(PPU kernel param and PPU param).
    When set to 1, enable change PPU param feature.
    When set to 0, disable change PPU param feature.
*/
#ifndef vpmdENABLE_CHANGE_PPU_PARAM
#define vpmdENABLE_CHANGE_PPU_PARAM         1
#endif


/*////////////////Correct definations, plese don't modify this section start/////////////////*/
/*
disable multi-task when CNN profiling is enabled.
*/
#if vpmdENABLE_CNN_PROFILING
#undef  vpmdENABLE_MULTIPLE_TASK
#define vpmdENABLE_MULTIPLE_TASK            0
#endif

/*
enable flush CPU cache when VIP's MMU is enabled.
If you CPU alwayes has no cache, you can still disable it even if VIP's MMU is enabled.
*/
#if vpmdENABLE_MMU
#undef  vpmdENABLE_FLUSH_CPU_CACHE
#define vpmdENABLE_FLUSH_CPU_CACHE          1
#endif

/*
should disable some features when secure is enabled.
*/
#if vpmdENABLE_SECURE
#undef vpmdENABLE_WAIT_LINK_LOOP
#define vpmdENABLE_WAIT_LINK_LOOP           0

#undef vpmdENABLE_USE_AGENT_TRIGGER
#define vpmdENABLE_USE_AGENT_TRIGGER        0

/* handle multi-task in Broker driver */
#undef vpmdENABLE_MULTIPLE_TASK
#define vpmdENABLE_MULTIPLE_TASK            0

#undef vpmdPOWER_OFF_TIMEOUT
#define vpmdPOWER_OFF_TIMEOUT               0
#endif

/*
disable wait-link trigger hardware when agent driver is enabled.
*/
#if vpmdENABLE_USE_AGENT_TRIGGER
#undef vpmdENABLE_WAIT_LINK_LOOP
#define vpmdENABLE_WAIT_LINK_LOOP           0
#endif

/* thread number is 1 when multiple task is disabled or it must not be less than 1 */
#if !vpmdENABLE_MULTIPLE_TASK
#undef SUPPORT_MAX_TASK_NUM
#define SUPPORT_MAX_TASK_NUM                1
#endif

/*
disable user application control vip power if driver power management is enabled.
*/
#if vpmdPOWER_OFF_TIMEOUT
#undef  vpmdENABLE_USER_CONTROL_POWER
#define vpmdENABLE_USER_CONTROL_POWER       0
#endif
/*///////////////Correct definations, plese don't modify this section end//////////////////*/


#endif
