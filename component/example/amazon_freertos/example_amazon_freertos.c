
#include <platform_opts.h>

#include "FreeRTOS.h"
#include "task.h"
#include "diag.h"
#include "stdlib.h"

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if !defined(configSUPPORT_STATIC_ALLOCATION) || (configSUPPORT_STATIC_ALLOCATION != 1)
/* #define configSUPPORT_STATIC_ALLOCATION 1 */
#error configSUPPORT_STATIC_ALLOCATION must be defined in FreeRTOSConfig.h as 1.
#endif

#if !defined(CONFIG_USE_MBEDTLS) || (CONFIG_USE_MBEDTLS != 1)
#error CONFIG_USE_MBEDTLS must be defined in platform_opts.h as 1.
#endif

extern int aws_main(void);

static void example_amazon_freertos_thread(void *pvParameters)
{
#if defined(MBEDTLS_PLATFORM_C)
	mbedtls_platform_set_calloc_free(calloc, free);
#endif

	aws_main();
#if !defined(configUSE_DAEMON_TASK_STARTUP_HOOK) || (configUSE_DAEMON_TASK_STARTUP_HOOK == 0)
	vApplicationDaemonTaskStartupHook();
#endif
	vTaskDelete(NULL);
	return;
}

void example_amazon_freertos(void)
{
	if (xTaskCreate(example_amazon_freertos_thread, ((const char *)"example_amazon_freertos_thread"), 2048, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
		printf("\n\r%s xTaskCreate(example_amazon_freertos_thread) failed", __FUNCTION__);
	}
}
