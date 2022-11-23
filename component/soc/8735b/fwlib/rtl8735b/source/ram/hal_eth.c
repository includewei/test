/**************************************************************************//**
 * @file    hal_eth.c
 * @brief   The Ethernet HAL API functions
 * @version V1.00
 * @date    2017-07-20
 *
 * @note
 *
 ******************************************************************************
 *
 * Copyright(c) 2007 - 2017 Realtek Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************/
#include "hal_eth.h"
#include "hal_pinmux.h"
#include "hal_cache.h"
#include "hal_sys_ctrl.h"



#if defined(CONFIG_MII_EN) && (CONFIG_MII_EN == 1)
/**
 * @addtogroup hs_hal_ethernet ETHERNET
 * @{
 */

extern hal_eth_func_stubs_t hal_eth_stubs;

/**
  * @brief The global Ethernet HAL adapter.
  */
hal_eth_adapter_t eth_adapter;



void hal_eth_irq_unreg(void)
{
	hal_eth_stubs.hal_eth_irq_unreg();
}


/**
 *  @brief To set the ethernet MAC address.
 *
 *  @param[in]  addr The specified MAC address.
 *
 *  @returns    void.
 */
void hal_eth_set_mac_addr(u8 *addr)
{
	hal_eth_stubs.hal_eth_set_mac_addr(&eth_adapter, addr);
}


/**
 *  @brief To get the ethernet MAC address.
 *
 *  @param[in]  addr The buffer of MAC address.
 *
 *  @returns    void.
 */
void hal_eth_get_mac_addr(u8 *addr)
{
	hal_eth_stubs.hal_eth_get_mac_addr(&eth_adapter, addr);
}


/**
 *  @brief To set the Tx/Rx descriptor number.
 *
 *  @param[in]  tx_desc_no The specified Tx descriptor number.
 *  @param[in]  rx_desc_no The specified Rx descriptor number.
 *
 *  @returns    void.
 */
void hal_eth_set_desc_num(u8 tx_desc_no, u8 rx_desc_no)
{
	hal_eth_stubs.hal_eth_set_desc_num(&eth_adapter, tx_desc_no, rx_desc_no);
}


/**
 *  @brief To set the start address of Tx/Rx descriptor ring.
 *
 *  @param[in]  tx_desc The start address of Tx descriptor ring.
 *  @param[in]  rx_desc The start address of Rx descriptor ring.
 *
 *  @returns    void.
 */
void hal_eth_set_desc_addr(u8 *tx_desc, u8 *rx_desc)
{
	hal_eth_stubs.hal_eth_set_desc_addr(&eth_adapter, tx_desc, rx_desc);
}


/**
 *  @brief To set the start address of Tx/Rx packet buffer.
 *
 *  @param[in]  tx_pkt_buf The start address of Tx packet buffer.
 *  @param[in]  rx_pkt_buf The start address of Rx packet buffer.
 *
 *  @returns    void.
 */
void hal_eth_set_pkt_buf(u8 *tx_pkt_buf, u8 *rx_pkt_buf)
{
	hal_eth_stubs.hal_eth_set_pkt_buf(&eth_adapter, tx_pkt_buf, rx_pkt_buf);
}


/**
 *  @brief To initialize the Ethernet controller.
 *
 *  @param[in]  N/A
 *
 *  @returns    The result.
 */
hal_status_t hal_eth_init(void)
{
	eth_adapter.dcache_invalidate_by_addr = hal_cache_stubs.dcache_invalidate_by_addr;
	eth_adapter.dcache_clean_by_addr = hal_cache_stubs.dcache_clean_by_addr;

	return hal_eth_stubs.hal_eth_init(&eth_adapter);
}

#if 0
/**
 *  @brief To initialize the Ethernet controller with force speed mode (100Mbps/Full duplex).
 *
 *  @param[in]  if_sel The interface between the MAC and PHY.
 *  @param[in]  pin_sel The pinmux selection.
 *
 *  @returns    The result.
 */
hal_status_t hal_eth_init_force_spd(eth_if_sel_t if_sel, eth_pin_sel_t pin_sel)
{
	hal_status_t ret;
	io_pin_t *pin_list;


	hal_dbg_port_disable();

#if ((CHIP_VER <= CHIP_C_CUT) && (defined(CONFIG_BUILD_RAM)))
	pin_list = (io_pin_t *)&eth_pin_table_patch[(if_sel * 30) + (pin_sel * 15)];
#else
	pin_list = (io_pin_t *)&hal_eth_stubs.eth_pin_table[(if_sel * 30) + (pin_sel * 15)];
#endif
	ret = hal_pinmux_register(pin_list, PID_ETH);
	if (ret == HAL_OK) {
		eth_adapter.dcache_invalidate_by_addr = hal_cache_stubs.dcache_invalidate_by_addr;
		eth_adapter.dcache_clean_by_addr = hal_cache_stubs.dcache_clean_by_addr;
		return hal_eth_init_force_spd_rtl8735b(&eth_adapter, if_sel, pin_sel);
	} else {
		return ret;
	}
}
#endif

/**
 *  @brief To de-initialize the Ethernet controller.
 *
 *  @param None
 *
 *  @returns    void.
 */
void hal_eth_deinit(void)
{
	eth_adapter.dcache_invalidate_by_addr = NULL;
	eth_adapter.dcache_clean_by_addr = NULL;
	hal_eth_stubs.hal_eth_deinit(&eth_adapter);
}


/**
 *  @brief To write "size" bytes of data from "data" to the Tx packet buffer.
 *
 *  @param[in]  data The buffer of packet data.
 *  @param[in]  size The size of the packet data.
 *
 *  @returns    The number of bytes written, or (-1) if errors.
 */
s32 hal_eth_write_data(u8 *data, u32 size)
{
	return hal_eth_stubs.hal_eth_write_data(&eth_adapter, data, size);
}


/**
 *  @brief To send the packet from Tx packet buffer.
 *
 *  @param None
 *
 *  @returns    The packet size.
 */
u32 hal_eth_send_pkt(void)
{
	return hal_eth_stubs.hal_eth_send_pkt(&eth_adapter);
}


/**
 *  @brief To receive a packet into the Rx packet buffer.
 *
 *  @param None
 *
 *  @returns    The packet size, or 0 if no packet received.
 */
u32 hal_eth_receive_pkt(void)
{
	return hal_eth_stubs.hal_eth_receive_pkt(&eth_adapter);
}


/**
 *  @brief To read packet data from Rx packet buffer to the "data" buffer.
 *
 *  @param[in]  data A buffer for the packet data.
 *  @param[in]  size The specified length (in bytes) to be read.
 *
 *  @returns    The actual size (in bytes) of data read.
 */
u32 hal_eth_read_data(u8 *data, u32 size)
{
	return hal_eth_stubs.hal_eth_read_data(&eth_adapter, data, size);
}


/**
 *  @brief To get the link status.
 *
 *  @param None
 *
 *  @returns    1 for link up, 0 for link down.
 */
u32 hal_eth_get_link_status(void)
{
	return hal_eth_stubs.hal_eth_get_link_status(&eth_adapter);
}


/**
 *  @brief To set the link speed and duplex mode.
 *
 *  @param[in]  speed The specified link speed.
 *  @param[in]  duplex The specifed duplex mode.
 *
 *  @returns    void.
 */
void hal_eth_set_link(s32 speed, s32 duplex)
{
	hal_eth_stubs.hal_eth_set_link(&eth_adapter, speed, duplex);
}


/**
 *  @brief To hook a callback function for Ethernet MAC controller interrupt.
 *
 *  @param[in]  pcallback The callback function.
 *
 *  @returns    void
 */
void hal_eth_callback_hook(eth_callback_t pcallback)
{
	hal_eth_stubs.hal_eth_callback_hook(&eth_adapter, pcallback);
}


/**
 *  @brief To hook a callback function to make OS do a context-switch while waiting.
 *
 *  @param[in]  task_yield The callback function.
 *
 *  @returns    void.
 */
void hal_eth_task_yield_hook(eth_task_yield task_yield)
{
	hal_eth_stubs.hal_eth_task_yield_hook(&eth_adapter, task_yield);
}


/**
 *  @brief To read the specified FEPHY register
 *
 *  @param[in]  page The specified page number.
 *  @param[in]  reg_addr The specified register address.
 *
 *  @returns    The register value.
 */
u16 hal_eth_rd_phy_reg(u16 page, u16 reg_addr)
{
	return hal_eth_stubs.hal_eth_rd_phy_reg(&eth_adapter, page, reg_addr);
}


/**
 *  @brief To write "data" value to the specified FEPHY register
 *
 *  @param[in]  page The specified page number.
 *  @param[in]  reg_addr The specified register address.
 *  @param[in]  data The specified data value.
 *
 *  @returns    0.
 */
u32 hal_eth_wr_phy_reg(u16 page, u16 reg_addr, u16 data)
{
	return hal_eth_stubs.hal_eth_wr_phy_reg(&eth_adapter, page, reg_addr, data);
}


/**
 *  @brief To check the ethernet PHY's state.
 *
 *  @param[in]  void
 *
 *  @returns    void
 */
void hal_eth_detect_phy_state(void)
{
	hal_eth_stubs.hal_eth_detect_phy_state(&eth_adapter);
}


/**
 *  @brief To enable/disable the EEE functionality of FEPHY
 *
 *  @param[in] en  Enable control: 0: disable, 1: enable.
 *
 *  @returns    void
 */
void hal_eth_phy_eee_ctrl(u8 en)
{
	hal_eth_stubs.hal_eth_phy_eee_ctrl(&eth_adapter, en);
}

/** @} */ /* End of group hs_hal_ethernet */


#endif  // end of "#if defined(CONFIG_MII_EN) && (CONFIG_MII_EN == 1)"


