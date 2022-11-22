#include "FreeRTOS.h"
#include "task.h"
#include "diag.h"
#include "main.h"
#include "log_service.h"
#include "osdep_service.h"
#include <platform_opts.h>
#include <platform_opts_bt.h>
#include "sys_api.h"

#include "wifi_conf.h"
#include <lwip_netconf.h>
#include <lwip/sockets.h>

#include "power_mode_api.h"

#define STACKSIZE  2048
#define TCP_RESUME 1

extern uint8_t rtl8735b_wowlan_wake_reason(void);
extern uint8_t *rtl8735b_read_wakeup_packet(uint32_t *size, uint8_t wowlan_reason);
extern int rtl8735b_suspend(int mode);
extern void rtl8735b_set_lps_pg(void);

static uint8_t wowlan_wake_reason = 0;
static uint8_t wlan_resume = 0;
static uint8_t tcp_resume = 0;
static uint32_t tcp_resume_seqno = 0;
static uint32_t tcp_resume_ackno = 0;
static char server_ip[16] = "192.168.13.163";
static uint16_t server_port = 5566;
static uint16_t local_port = 1000;
static uint8_t goto_sleep = 0;

#if CONFIG_WLAN
#include <wifi_fast_connect.h>
extern void wlan_network(void);
#endif

extern void console_init(void);

void set_tcp_connected_pattern(wowlan_pattern_t *pattern)
{
	// This pattern make STA can be wake from a connected TCP socket
	memset(pattern, 0, sizeof(wowlan_pattern_t));

	char buf[32];
	char mac[6];
	char ip_protocol[2] = {0x08, 0x00}; // IP {08,00} ARP {08,06}
	char ip_ver[1] = {0x45};
	char tcp_protocol[1] = {0x06}; // 0x06 for tcp
	char tcp_port[2] = {(server_port >> 8) & 0xFF, server_port & 0xFF};
	char flag2[1] = {0x18}; // PSH + ACK
	uint8_t *ip = LwIP_GetIP(0);
	const uint8_t data_mask[6] = {0x3f, 0x70, 0x80, 0xc0, 0x0F, 0x80};
	const uint8_t *mac_temp = LwIP_GetMAC(0);

	//wifi_get_mac_address(buf);
	//sscanf(buf, "%02x:%02x:%02x:%02x:%02x:%02x", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
	memcpy(mac, mac_temp, 6);
	printf("mac = 0x%2X,0x%2X,0x%2X,0x%2X,0x%2X,0x%2X \r\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	memcpy(pattern->eth_da, mac, 6);
	memcpy(pattern->eth_proto_type, ip_protocol, 2);
	memcpy(pattern->header_len, ip_ver, 1);
	memcpy(pattern->ip_proto, tcp_protocol, 1);
	memcpy(pattern->ip_da, ip, 4);
	memcpy(pattern->src_port, tcp_port, 2);
	memcpy(pattern->flag2, flag2, 1);
	memcpy(pattern->mask, data_mask, 6);

	//payload
	// uint8_t data[10] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
	// uint8_t payload_mask[9] = {0xc0, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
	// memcpy(pattern->payload, data, 10);
	// memcpy(pattern->payload_mask, payload_mask, 9);

}

void tcp_app_task(void *param)
{
	int sock_fd = -1;

	// wait for IP address
	while (!((wifi_get_join_status() == RTW_JOINSTATUS_SUCCESS) && (*(u32 *)LwIP_GetIP(0) != IP_ADDR_INVALID))) {
		vTaskDelay(10);
	}

	// socket
	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	printf("\n\r socket(%d) \n\r", sock_fd);
	int reuse = 1;
	printf("set reuse %s \n\r", setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse) == 0 ? "OK" : "FAIL");
	struct sockaddr_in local_addr;
	local_addr.sin_family = AF_INET;
	local_addr.sin_addr.s_addr = INADDR_ANY;
	local_addr.sin_port = htons(local_port);
	printf("bind port:%d %s \n\r", local_port, bind(sock_fd, (struct sockaddr *) &local_addr, sizeof(local_addr)) == 0 ? "OK" : "FAIL");

	if (tcp_resume) {
#if TCP_RESUME
		// resume tcp pcb
		extern int lwip_resumetcp(int s, uint32_t seqno, uint32_t ackno);
		printf("resume TCP pcb & seqno & ackno %s \n\r", lwip_resumetcp(sock_fd, tcp_resume_seqno, tcp_resume_ackno) == 0 ? "OK" : "FAIL");
#endif
	} else {
		// connect
		struct sockaddr_in server_addr;
		server_addr.sin_family = AF_INET;
		server_addr.sin_addr.s_addr = inet_addr(server_ip);
		server_addr.sin_port = htons(server_port);

		if (connect(sock_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == 0) {
			printf("connect to %s:%d OK \n\r", server_ip, server_port);
		} else {
			printf("connect to %s:%d FAIL \n\r", server_ip, server_port);
			close(sock_fd);
			goto exit;
		}
	}

	while (!goto_sleep) {
		int ret = write(sock_fd, "_APP", strlen("_APP"));
		printf("write application data %d \n\r", ret);

		if (ret < 0) {
			// close
			printf("\n\r close(%d) \n\r", sock_fd);
			close(sock_fd);
			goto exit;
		}

		vTaskDelay(5000);
	}

#if TCP_RESUME
	// retain tcp pcb
	extern int lwip_retaintcp(int s);
	printf("retain TCP pcb %s \n\r", lwip_retaintcp(sock_fd) == 0 ? "OK" : "FAIL");
#endif
	// set keepalive
	uint8_t keepalive_content[] = {'_', 'A', 'l', 'i', 'v', 'e'};
	uint32_t interval_ms = 30000;
	uint32_t resend_ms = 10000;
	wifi_set_tcp_keep_alive_offload(sock_fd, keepalive_content, sizeof(keepalive_content), interval_ms, resend_ms, 1);

	// set wakeup pattern
	wowlan_pattern_t data_pattern;
	set_tcp_connected_pattern(&data_pattern);
	wifi_wowlan_set_pattern(data_pattern);

	// for wlan resume
	extern int rtw_hal_wlan_resume_backup(void);
	rtw_hal_wlan_resume_backup();

	// sleep
	rtl8735b_set_lps_pg();
	rtw_enter_critical(NULL, NULL);
	if (rtl8735b_suspend(0) == 0) { // should stop wifi application before doing rtl8735b_suspend(
		printf("rtl8735b_suspend\r\n");

		uint32_t bcnearly = 0x1000;
		HAL_WRITE32(0x40080000, 0x88, bcnearly);

		// standby
		HAL_WRITE32(0x40009000, 0x18, 0xB5E36001);//4MHz power on
		Standby(DSTBY_WLAN | SLP_GTIMER, 0, 0, 1);
	} else {
		printf("rtl8735b_suspend fail\r\n");
		sys_reset();
	}
	rtw_exit_critical(NULL, NULL);

	while (1) {
		vTaskDelay(2000);
		printf("sleep fail!!!\r\n");
	}

exit:
	vTaskDelete(NULL);
}

int wlan_do_resume(void)
{
	extern int rtw_hal_wlan_resume_restore(void);
	rtw_hal_wlan_resume_restore();

	wifi_fast_connect_enable(1);
	wifi_fast_connect_load_fast_dhcp();
	LwIP_DHCP(0, DHCP_START);

	return 0;
}

void fPS(void *arg)
{
	int argc;
	char *argv[MAX_ARGC] = {0};

	argc = parse_param(arg, argv);

	if (strcmp(argv[1], "wowlan") == 0) {
		goto_sleep = 1;
	}
}

log_item_t at_power_save_items[ ] = {
	{"PS", fPS,},
};

void main(void)
{
	/* *************************************
		RX_DISASSOC = 0x04,
		RX_DEAUTH = 0x08,
		FW_DECISION_DISCONNECT = 0x10,
		RX_PATTERN_PKT = 0x23,
		TX_TCP_SEND_LIMIT = 0x69,
		RX_DHCP_NAK = 0x6A,
		DHCP_RETRY_LIMIT = 0x6B,
		RX_MQTT_PATTERN_MATCH = 0x6C,
		RX_MQTT_PUBLISH_WAKE = 0x6D,
		RX_MQTT_MTU_LIMIT_PACKET = 0x6E,
	*************************************** */

	wowlan_wake_reason = rtl8735b_wowlan_wake_reason();
	if (wowlan_wake_reason != 0) {
		printf("\r\nwake fom wlan: 0x%02X\r\n", wowlan_wake_reason);

		if (wowlan_wake_reason == 0x23) {
			uint32_t packet_len = 0;
			uint8_t *wakeup_packet = rtl8735b_read_wakeup_packet(&packet_len, wowlan_wake_reason);

			// parse wakeup packet
			uint8_t *ip_header = NULL;
			uint8_t *tcp_header = NULL;
			uint8_t tcp_header_first4[4];
			tcp_header_first4[0] = (server_port & 0xff00) >> 8;
			tcp_header_first4[1] = (server_port & 0x00ff);
			tcp_header_first4[2] = (local_port & 0xff00) >> 8;
			tcp_header_first4[3] = (local_port & 0x00ff);

			for (int i = 0; i < packet_len - 4; i ++) {
				if ((memcmp(wakeup_packet + i, tcp_header_first4, 4) == 0) && (*(wakeup_packet + i - 20) == 0x45)) {
					ip_header = wakeup_packet + i - 20;
					tcp_header = wakeup_packet + i;
					break;
				}
			}

			if (ip_header && tcp_header) {
				if (tcp_header[13] == 0x18) {
					printf("PUSH + ACK\n\r");
					wlan_resume = 1;
#if TCP_RESUME
					tcp_resume = 1;

					uint16_t ip_len = (((uint16_t) ip_header[2]) << 8) | ((uint16_t) ip_header[3]);
					uint16_t tcp_payload_len = ip_len - 20 - 20;
					uint32_t wakeup_seqno = (((uint32_t) tcp_header[4]) << 24) | (((uint32_t) tcp_header[5]) << 16) |
											(((uint32_t) tcp_header[6]) << 8) | ((uint32_t) tcp_header[7]);
					uint32_t wakeup_ackno = (((uint32_t) tcp_header[8]) << 24) | (((uint32_t) tcp_header[9]) << 16) |
											(((uint32_t) tcp_header[10]) << 8) | ((uint32_t) tcp_header[11]);
					printf("tcp_payload_len=%d, wakeup_seqno=%u, wakeup_ackno=%u \n\r", tcp_payload_len, wakeup_seqno, wakeup_ackno);

					tcp_resume_seqno = wakeup_ackno;
					tcp_resume_ackno = wakeup_seqno + tcp_payload_len;
					printf("tcp_resume_seqno=%u, tcp_resume_ackno=%u \n\r", tcp_resume_seqno, tcp_resume_ackno);
#endif
				} else if (tcp_header[13] == 0x11) {
					printf("FIN + ACK\n\r");

					// not resume because TCP connection is closed
				}
			}

			free(wakeup_packet);
			// Get remote ctrl info from resvd page filled by FW
			extern int read_remotectrl_info_from_txfifo(void);
			read_remotectrl_info_from_txfifo();
		}
	}

	console_init();
	log_service_add_table(at_power_save_items, sizeof(at_power_save_items) / sizeof(at_power_save_items[0]));

	if (wlan_resume) {
		p_wifi_do_fast_connect = wlan_do_resume;
		p_store_fast_connect_info = NULL;
	} else {
		wifi_fast_connect_enable(1);
	}

	wlan_network();

	if (xTaskCreate(tcp_app_task, ((const char *)"tcp_app_task"), STACKSIZE, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
		printf("\n\r%s xTaskCreate(tcp_app_task) failed\n", __FUNCTION__);
	}

	vTaskStartScheduler();
	while (1);
}
