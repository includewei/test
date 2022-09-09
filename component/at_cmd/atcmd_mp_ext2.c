#include <platform_stdlib.h>
#include <platform_opts.h>
#if defined(CONFIG_PLATFORM_8721D) || defined(CONFIG_PLATFORM_8710C) || defined(CONFIG_PLATFORM_AMEBAD2) || defined(CONFIG_PLATFORM_8735B) || defined(CONFIG_PLATFORM_AMEBALITE)
#include <platform_opts_bt.h>
#endif
#include <gpio_api.h>
#include "log_service.h"
#include "osdep_service.h"
#include "atcmd_mp.h"
#ifdef CONFIG_PLATFORM_8710C
#include "rtl8710c_pin_name.h"
#endif
#include "bt_intf.h"

#define MP_EXT2_PREFIX		"[ATM2]: "
#define MP_EXT2_PRINTF(...) \
		do{ \
			_AT_PRINTK(MP_EXT2_PREFIX); \
			_AT_PRINTK(__VA_ARGS__); \
		}while(0)

#define UART_BRIDGE_USAGE	"ATM2=bridge\n"
#define BT_POWER_USAGE		"ATM2=bt_power,ACT <ACT: on/off>\n"
#define GNT_BT_USAGE		"ATM2=gnt_bt,TARGET <TARGET: wifi/bt>\n"

#if defined(CONFIG_PLATFORM_8710C) || defined(CONFIG_PLATFORM_8735B)
extern void console_reinit_uart(void);
#endif

#if defined(CONFIG_BT) && CONFIG_BT
extern void bt_uart_bridge_close(void);
extern void bt_uart_bridge_open(void);
extern bool bte_init(void);
extern void bte_deinit(void);
extern bool bt_trace_init(void);
extern bool bt_trace_deinit(void);

static bool open_flag = 0;
#endif
static int mp_ext2_uart_bridge(void **argv, int argc)
{
#if defined(CONFIG_BT) && CONFIG_BT
	MP_EXT2_PRINTF("_AT_MP_BRIDGE_\n");
	if (argc == 1) {
		if (strncmp(argv[0], "close", 5) == 0) {
			MP_EXT2_PRINTF("close UART bridge.\n");
			if (open_flag == 0) {
				return 0;
			}

			bt_uart_bridge_close();
#if defined(CONFIG_PLATFORM_8710C) || defined(CONFIG_PLATFORM_8735B)
			console_reinit_uart();
#endif
			open_flag = 0;
			return 0;
		}
	}
	MP_EXT2_PRINTF("open UART bridge.\n");
	bt_uart_bridge_open();
	open_flag = 1;
#else
	(void)argv;
	(void)argc;
#endif
	return 0;
}

static int mp_ext2_bt_power(void **argv, int argc)
{
	(void)argv;
	(void)argc;
#if defined(CONFIG_BT) && CONFIG_BT
	if (strcmp(argv[0], "on") == 0) {
		MP_EXT2_PRINTF("BT power on.\n\r");
		rtlk_bt_set_gnt_bt_with_clk_source(PTA_BT);
		bt_trace_init();
		bte_init();
	} else if (strcmp(argv[0], "off") == 0) {
		MP_EXT2_PRINTF("BT power off.\n\r");
		bte_deinit();
		bt_trace_deinit();
		rtlk_bt_set_gnt_bt_with_clk_source(PTA_WIFI);
	}
#endif
	return 0;
}

static int mp_ext2_gnt_bt(void **argv, int argc)
{
	(void)argc;
	if (strcmp(argv[0], "wifi") == 0) {
		MP_EXT2_PRINTF("Switch GNT_BT to WIFI.\n\r");
		rtlk_bt_set_gnt_bt_with_clk_source(PTA_WIFI);

	} else if (strcmp(argv[0], "bt") == 0) {
		MP_EXT2_PRINTF("Switch GNT_BT to BT.\n\r");
		rtlk_bt_set_gnt_bt_with_clk_source(PTA_BT);
	}
	return 0;
}

at_mp_ext_item_t at_mp_ext2_items[] = {
	{"bridge",		mp_ext2_uart_bridge,		UART_BRIDGE_USAGE},
	{"bt_power",	mp_ext2_bt_power,			BT_POWER_USAGE},
	{"gnt_bt",		mp_ext2_gnt_bt,				GNT_BT_USAGE},
};

void fATM2(void *arg)
{
	int argc = 0, idx, cmd_cnt;
	char *argv[MAX_ARGC] = {0};

	cmd_cnt = sizeof(at_mp_ext2_items) / sizeof(at_mp_ext2_items[0]);
	argc = parse_param(arg, argv);
	if (argc == 1) {
		_AT_PRINTK("\n");
		MP_EXT2_PRINTF("Command usage :\n");
		for (idx = 0; idx < cmd_cnt; idx++) {
			_AT_PRINTK("%s", at_mp_ext2_items[idx].mp_ext_usage);
		}
	} else {
		for (idx = 0; idx < cmd_cnt; idx++) {
			if (strcmp(argv[1], at_mp_ext2_items[idx].mp_ext_cmd) == 0) {
				int (*fun)(void **argv, int argc) = at_mp_ext2_items[idx].mp_ext_fun;
				fun((void **)&argv[2], argc - 2);
				return;
			}
		}
		MP_EXT2_PRINTF("unknown command %s.\n", argv[1]);
	}
}

