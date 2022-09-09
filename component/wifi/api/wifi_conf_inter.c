//----------------------------------------------------------------------------//
//#include <flash/stm32_flash.h>
#if !defined(CONFIG_MBED_ENABLED) && !defined(CONFIG_PLATFOMR_CUSTOMER_RTOS)
#include "main.h"
#if CONFIG_LWIP_LAYER
#include <lwip_netconf.h>
#include <dhcp/dhcps.h>
#endif
#endif
#include <platform_stdlib.h>
#include <wifi_conf.h>
#include <wifi_ind.h>
#include <osdep_service.h>
#include <device_lock.h>

#if (defined(CONFIG_EXAMPLE_UART_ATCMD) && CONFIG_EXAMPLE_UART_ATCMD) || (defined(CONFIG_EXAMPLE_SPI_ATCMD) && CONFIG_EXAMPLE_SPI_ATCMD)
#include "at_cmd/atcmd_wifi.h"
#endif
#if defined(CONFIG_PLATFORM_8721D) || defined(CONFIG_PLATFORM_8710C) || defined(CONFIG_PLATFORM_AMEBAD2) || defined(CONFIG_PLATFORM_8735B) || defined(CONFIG_PLATFORM_AMEBALITE)
#include "platform_opts_bt.h"
#endif
#if defined(CONFIG_ENABLE_WPS_AP) && CONFIG_ENABLE_WPS_AP
#include <wifi_wps_config.h>
#endif




/******************************************************
 *                    Constants
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/
/******************************************************
 *               Variables Declarations
 ******************************************************/

/******************************************************
 *               Variables Definitions
 ******************************************************/


/******************************************************
 *               Function Definitions
 ******************************************************/
void wifi_psk_info_set(struct psk_info *psk_data)
{
	rltk_psk_info_set(psk_data);
}

void wifi_psk_info_get(struct psk_info *psk_data)
{
	rltk_psk_info_get(psk_data);
}

u8 wifi_driver_is_mp(void)
{
	return rltk_wlan_is_mp();
}

int wifi_set_pmk_cache_enable(unsigned char value)
{
	return rtw_wx_set_pmk_cache_enable(WLAN0_IDX, value);
}

//----------------------------------------------------------------------------//
int wifi_get_sw_statistic(unsigned char idx, rtw_sw_statistics_t *sw_statistics)
{
	return rltk_wlan_statistic(idx, sw_statistics);
}

int wifi_set_wps_phase(unsigned char is_trigger_wps)
{
	return rltk_wlan_set_wps_phase(is_trigger_wps);
}

int wifi_set_gen_ie(unsigned char wlan_idx, char *buf, __u16 buf_len, __u16 flags)
{
#ifdef CONFIG_WPS
	return rtw_wx_set_gen_ie(wlan_idx, buf, buf_len, flags);
#else
	return -1;
#endif
}

int wifi_set_eap_phase(unsigned char is_trigger_eap)
{
#ifdef CONFIG_EAP
	return rltk_wlan_set_eap_phase(is_trigger_eap);
#else
	return -1;
#endif
}

unsigned char wifi_get_eap_phase(void)
{
#ifdef CONFIG_EAP
	return rltk_wlan_get_eap_phase();
#else
	return 0;
#endif
}

int wifi_set_eap_method(unsigned char eap_method)
{
#ifdef CONFIG_EAP
	return rltk_wlan_set_eap_method(eap_method);
#else
	return -1;
#endif
}

int wifi_send_eapol(const char *ifname, char *buf, __u16 buf_len, __u16 flags)
{
	return rtw_wx_send_eapol(ifname, buf, buf_len, flags);
}

/*
 * @brief get WIFI band type
 *@retval  the support band type.
 * 	WL_BAND_2_4G: only support 2.4G
 *	WL_BAND_5G: only support 5G
 *      WL_BAND_2_4G_5G_BOTH: support both 2.4G and 5G
 */
WL_BAND_TYPE wifi_get_band_type(void)
{
	u8 ret;

	ret = rltk_get_band_type();

	if (ret == 0) {
		return WL_BAND_2_4G;
	} else if (ret == 1) {
		return WL_BAND_5G;
	} else {
		return WL_BAND_2_4G_5G_BOTH;
	}
}

int wifi_get_auto_chl(unsigned char wlan_idx, unsigned char *channel_set, unsigned char channel_num)
{
	return rltk_get_auto_chl(wlan_idx, channel_set, channel_num);
}

int wifi_del_station(unsigned char wlan_idx, unsigned char *hwaddr)
{
	return rltk_del_station(wlan_idx, hwaddr);
}
