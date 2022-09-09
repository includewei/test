/**************************************************************************//**
 * @file    rtl8735b_eth.c
 * @brief    This file implements the Ethernet HAL functions.
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
#if 0
#include "cmsis.h"
#include "hal_eth.h"

#if defined(CONFIG_MII_EN) && (CONFIG_MII_EN == 1)

#define SECTION_ETH_TEXT           SECTION(".rom.hal_eth.text")
#define SECTION_ETH_DATA           SECTION(".rom.hal_eth.data")
#define SECTION_ETH_RODATA         SECTION(".rom.hal_eth.rodata")
#define SECTION_ETH_BSS            SECTION(".rom.hal_eth.bss")
#define SECTION_ETH_STUBS          SECTION(".rom.hal_eth.stubs")



extern void *_memset(void *dst0, int Val, SIZE_T length);
extern void *_memcpy(void *s1, const void *s2, size_t n);

void MII_IRQHandler(void);


/**
 * @addtogroup hs_hal_ethernet ETHERNET
 * @{
 */


/**
  * @brief Pin mux table for Ethernet
  */
SECTION_ETH_RODATA
u8 eth_pin_table[] = {
	PIN_B0, PIN_B1, PIN_B2, PIN_B3, PIN_B4, PIN_B5, PIN_C10, PIN_C11, PIN_E0, PIN_E1, PIN_E2, PIN_E3, PIN_E4, PIN_E5, 0xFF,                         // MII, S0
	PIN_E0, PIN_E1, PIN_E2, PIN_E3, PIN_E4, PIN_E5, PIN_E6, PIN_E7, PIN_E8, PIN_E9, PIN_E10, PIN_E11, PIN_E12, PIN_E13, 0xFF,                       // MII, S1
	PIN_B1, PIN_B2, PIN_B3, PIN_B5, PIN_C10, PIN_C11, PIN_E0, PIN_E1, PIN_E2, PIN_E5, PIN_LIST_END, PIN_LIST_END, PIN_LIST_END, PIN_LIST_END, 0xFF, // RMII, S0
	PIN_E0, PIN_E1, PIN_E2, PIN_E5, PIN_E7, PIN_E8, PIN_E9, PIN_E11, PIN_E12, PIN_E13, PIN_LIST_END, PIN_LIST_END, PIN_LIST_END, PIN_LIST_END, 0xFF // RMII, S1
};

/**
  * @brief The global common data structure for Ethernet HAL operations.
  */
SECTION_ETH_BSS hal_eth_adapter_t *peth_adapt;
/**
  * @brief The start address of Tx descriptor ring.
  */
SECTION_ETH_BSS volatile hal_eth_tx_desc_t *peth_tx_desc;
/**
  * @brief The start address of Rx descriptor ring.
  */
SECTION_ETH_BSS volatile hal_eth_rx_desc_t *peth_rx_desc;
/**
  * @brief The start address of Tx packet buffer.
  */
SECTION_ETH_BSS volatile u8 *peth_tx_pkt_buf;
/**
  * @brief The start address of Rx packet buffer.
  */
SECTION_ETH_BSS volatile u8 *peth_rx_pkt_buf;
/**
  * @brief The index of current available Tx descriptor.
  */
SECTION_ETH_BSS volatile u8 eth_tx_desc_wptr = 0;
/**
  * @brief The index of current available Rx descriptor.
  */
SECTION_ETH_BSS volatile u8 eth_rx_desc_rptr = 0;
/**
  * @brief The size of the packet to be transmitted.
  */
SECTION_ETH_BSS volatile u32 curr_data_len = 0;


/// @cond DOXYGEN_ROM_HAL_API

/**
 * @addtogroup hs_hal_ethernet_rom_func ETHERNET HAL ROM APIs.
 * @ingroup hs_hal_ethernet
 * @{
 * @brief The ETHERNET HAL ROM APIs. The user application(in RAM space) should not call these APIs directly.
 *        There is another set of ETHERNET HAL APIs in the RAM space is provided for the user application.
 */


/**
  * @brief The stubs functions table to exports ETHERNET HAL functions in ROM.
  */
SECTION_ETH_STUBS const hal_eth_func_stubs_t hal_eth_stubs = {
	.eth_pin_table = (io_pin_t *) &eth_pin_table[0],
	.hal_eth_pin_ctrl = hal_eth_pin_ctrl_rtl8735b,
	.hal_eth_set_interface = hal_eth_set_interface_rtl8735b,
	.hal_eth_irq_handler = MII_IRQHandler,
	.hal_eth_irq_reg = hal_eth_irq_reg_rtl8735b,
	.hal_eth_irq_unreg = hal_eth_irq_unreg_rtl8735b,
	.hal_eth_set_desc_num = hal_eth_set_desc_num_rtl8735b,
	.hal_eth_set_desc_addr = hal_eth_set_desc_addr_rtl8735b,
	.hal_eth_set_pkt_buf = hal_eth_set_pkt_buf_rtl8735b,
	.hal_eth_set_mac_addr = hal_eth_set_mac_addr_rtl8735b,
	.hal_eth_get_mac_addr = hal_eth_get_mac_addr_rtl8735b,
	.hal_eth_init = hal_eth_init_rtl8735b,
	.hal_eth_deinit = hal_eth_deinit_rtl8735b,
	.hal_eth_write_data = hal_eth_write_data_rtl8735b,
	.hal_eth_send_pkt = hal_eth_send_pkt_rtl8735b,
	.hal_eth_receive_pkt = hal_eth_receive_pkt_rtl8735b,
	.hal_eth_read_data = hal_eth_read_data_rtl8735b,
	.hal_eth_get_link_status = hal_eth_get_link_status_rtl8735b,
	.hal_eth_set_link = hal_eth_set_link_rtl8735b,
	.hal_eth_callback_hook = hal_eth_callback_hook_rtl8735b,
	.hal_eth_task_yield_hook = hal_eth_task_yield_hook_rtl8735b
};



SECTION_ETH_TEXT
void hal_eth_en_ctrl_rtl8735b(BOOL en)
{
	syson_hs_mii_ctrl_t func_ctrl;
	u8 func_on;


	func_on = (en) ? 1 : 0;
	func_ctrl.w = SYSON->hs_mii_ctrl;

	func_ctrl.b.hs_mii_en = func_on;
	func_ctrl.b.hs_mii_clk_en = func_on;
	func_ctrl.b.hs_mii_sclk_gen = func_on;
	SYSON->hs_mii_ctrl = func_ctrl.w;
}


SECTION_ETH_TEXT
u16 hal_eth_rw_phy_reg_rtl8735b(hal_eth_adapter_t *peth_adapter, u8 operation, u8 address, u16 data)
{
	ETHERNET_Type *peth;
	u32 i = 0;


	if ((peth_adapter == NULL) || (address > ETH_PHY_REG31_ADDR)) {
		DBG_ETH_ERR("Invalid parameter !!\r\n");
		return HAL_ERR_PARA;
	}

	hal_misc_delay_us_rtl8735b(1);
	peth = peth_adapter->base_addr;
	peth->miia = (operation << 31) | (ETH_PHY_ADDR << 26) | (address << 16) | data;

	if (operation) {
		while (peth->miia_b.flag) {
			hal_misc_delay_us_rtl8735b(1);
			i++;
			if (i > ETH_TIMEOUT_CNT_MAX) {
				DBG_ETH_ERR("hal_eth_rw_phy_reg_rtl8735b(): Wait write operation flag timeout !!\r\n");
				break;
			}
		}

		return 0;
	} else {
		while (peth->miia_b.flag == 0) {
			hal_misc_delay_us_rtl8735b(1);
			i++;
			if (i > ETH_TIMEOUT_CNT_MAX) {
				DBG_ETH_ERR("hal_eth_rw_phy_reg_rtl8735b(): Wait read operation flag timeout !!\r\n");
				break;
			}
		}

		return (u16)((peth->miia) & 0xFFFF);
	}
}


SECTION_ETH_TEXT
hal_status_t hal_eth_pin_ctrl_rtl8735b(eth_pin_sel_t pin_sel, BOOL en)
{
	syson_hs_mii_ctrl_t func_ctrl;
	u8 func_on;


	func_on = (en) ? 1 : 0;
	func_ctrl.w = SYSON->hs_mii_ctrl;

	func_ctrl.b.hs_mii_pin_en = func_on;
	func_ctrl.b.hs_mii_mux_sel = pin_sel;
	SYSON->hs_mii_ctrl = func_ctrl.w;


	return HAL_OK;
}


SECTION_ETH_TEXT
void hal_eth_set_interface_rtl8735b(eth_if_sel_t if_sel)
{
	SYSON->hs_mii_ctrl_b.hs_mii_mod_sel = if_sel;
}


SECTION_ETH_TEXT
void MII_IRQHandler(void)
{
	ethernet_isr_imr_t isr_imr;


	hal_irq_clear_pending_rtl8735b(ETHERNET_IRQn);

	isr_imr.w = peth_adapt->base_addr->isr_imr;
	if ((isr_imr.b.isr_rok) && ((peth_adapt->int_mask) & BIT16)) {
		peth_adapt->int_mask &= (~BIT16);
//        DBG_ETH_INFO("(R)\r\n");
		if ((peth_adapt->callback) != NULL) {
			peth_adapt->callback(EthRxDone, 0);
		}
	}

	if ((isr_imr.b.isr_rer_ovf) && ((peth_adapt->int_mask) & BIT20)) {
		peth_adapt->int_mask &= (~BIT4);
//        DBG_ETH_INFO("Rx FIFO overflow\r\n");
	}

	if ((isr_imr.b.isr_tok) && ((peth_adapt->int_mask) & BIT22)) {
//        DBG_ETH_INFO("(T)\r\n");
		peth_adapt->int_mask &= (~BIT22);
		if ((peth_adapt->callback) != NULL) {
			peth_adapt->callback(EthTxDone, 0);
		}
	}

	if ((isr_imr.b.isr_linkchg) && ((peth_adapt->int_mask) & BIT24)) {
		if ((peth_adapt->callback) != NULL) {
			if (!(peth_adapt->base_addr->ms_b.linkb)) {
				peth_adapt->callback(EthLinkUp, 0);
			} else {
				peth_adapt->callback(EthLinkDown, 0);
			}
		}
	}

	peth_adapt->base_addr->isr_imr = (peth_adapt->int_mask) | 0xFFFF;
	__DSB();
}


SECTION_ETH_TEXT
void hal_eth_irq_reg_rtl8735b(irq_handler_t irq_handler)
{
	// IRQ vector may has been registered, disable and re-register it
	hal_irq_disable_rtl8735b(ETHERNET_IRQn);
	__ISB();
	hal_irq_set_vector_rtl8735b(ETHERNET_IRQn, (uint32_t)irq_handler);
	hal_irq_enable_rtl8735b(ETHERNET_IRQn);
}


SECTION_ETH_TEXT
void hal_eth_irq_unreg_rtl8735b(void)
{
	hal_irq_disable_rtl8735b(ETHERNET_IRQn);
	__ISB();
	hal_irq_set_vector_rtl8735b(ETHERNET_IRQn, (uint32_t)_default_handler_rtl8735b);
}


/**
 *  @brief To set the Tx/Rx descriptor number.
 *
 *  @param[in]  peth_adapter The ETHERNET adapter.
 *  @param[in]  tx_desc_no The specified Tx descriptor number.
 *  @param[in]  rx_desc_no The specified Rx descriptor number.
 *
 *  @returns    void.
 */
SECTION_ETH_TEXT
void hal_eth_set_desc_num_rtl8735b(hal_eth_adapter_t *peth_adapter, u8 tx_desc_no, u8 rx_desc_no)
{
	if ((peth_adapter == NULL) || (tx_desc_no == 0) || (rx_desc_no == 0)) {
		DBG_ETH_ERR("Invalid parameter !!\r\n");
		return;
	}
	if (((tx_desc_no * sizeof(hal_eth_tx_desc_t)) % 32) || ((rx_desc_no * sizeof(hal_eth_rx_desc_t)) % 32)) {
		DBG_ETH_ERR("The size of Tx/Rx descriptor ring must be 32-Byte alignment !!\r\n");
		return;
	}

	peth_adapter->tx_desc_num = tx_desc_no;
	peth_adapter->rx_desc_num = rx_desc_no;
}


/**
 *  @brief To set the start address of Tx/Rx descriptor ring.
 *
 *  @param[in]  peth_adapter The ETHERNET adapter.
 *  @param[in]  tx_desc The start address of Tx descriptor ring.
 *  @param[in]  rx_desc The start address of Rx descriptor ring.
 *
 *  @returns    void.
 */
SECTION_ETH_TEXT
void hal_eth_set_desc_addr_rtl8735b(hal_eth_adapter_t *peth_adapter, u8 *tx_desc, u8 *rx_desc)
{
	if ((peth_adapter == NULL) || (tx_desc == NULL) || (rx_desc == NULL)) {
		DBG_ETH_ERR("Invalid parameter !!\r\n");
		return;
	}
	if ((((u32)tx_desc) & 0x1F) || (((u32)rx_desc) & 0x1F)) {
		DBG_ETH_ERR("The descriptor address must be 32-Byte alignment !!\r\n");
		return;
	}

	peth_adapter->tx_desc_addr = tx_desc;
	peth_adapter->rx_desc_addr = rx_desc;
}


/**
 *  @brief To set the start address of Tx/Rx packet buffer.
 *
 *  @param[in]  peth_adapter The ETHERNET adapter.
 *  @param[in]  tx_pkt_buf The start address of Tx packet buffer.
 *  @param[in]  rx_pkt_buf The start address of Rx packet buffer.
 *
 *  @returns    void.
 */
SECTION_ETH_TEXT
void hal_eth_set_pkt_buf_rtl8735b(hal_eth_adapter_t *peth_adapter, u8 *tx_pkt_buf, u8 *rx_pkt_buf)
{
	if ((peth_adapter == NULL) || (tx_pkt_buf == NULL) || (rx_pkt_buf == NULL)) {
		DBG_ETH_ERR("Invalid parameter !!\r\n");
		return;
	}
	if ((((u32)tx_pkt_buf) & 0x1F) || (((u32)rx_pkt_buf) & 0x1F)) {
		DBG_ETH_ERR("The packet buffer address must be 32-Byte alignment !!\r\n");
		return;
	}

	peth_adapter->tx_pkt_buf = tx_pkt_buf;
	peth_adapter->rx_pkt_buf = rx_pkt_buf;
}


/**
 *  @brief To set the ethernet MAC address.
 *
 *  @param[in]  peth_adapter The ETHERNET adapter.
 *  @param[in]  addr The specified MAC address.
 *
 *  @returns    void.
 */
SECTION_ETH_TEXT
void hal_eth_set_mac_addr_rtl8735b(hal_eth_adapter_t *peth_adapter, u8 *addr)
{
	if ((peth_adapter == NULL) || (addr == NULL)) {
		DBG_ETH_ERR("Invalid parameter !!\r\n");
		return;
	}

	_memcpy((void *)(peth_adapter->mac_id), addr, ETH_MAC_ADDR_LEN);
}


/**
 *  @brief To get the ethernet MAC address.
 *
 *  @param[in]  peth_adapter The ETHERNET adapter.
 *  @param[in]  addr The buffer of MAC address.
 *
 *  @returns    void.
 */
SECTION_ETH_TEXT
void hal_eth_get_mac_addr_rtl8735b(hal_eth_adapter_t *peth_adapter, u8 *addr)
{
	if ((peth_adapter == NULL) || (addr == NULL)) {
		DBG_ETH_ERR("Invalid parameter !!\r\n");
		return;
	}

	_memcpy((void *)addr, peth_adapter->mac_id, ETH_MAC_ADDR_LEN);
}


/**
 *  @brief To initialize the Ethernet MAC controller.
 *
 *  @param[in]  peth_adapter The ETHERNET adapter.
 *  @param[in]  if_sel The interface between the MAC and PHY.
 *  @param[in]  pin_sel The pinmux selection.
 *
 *  @returns    The result.
 */
SECTION_ETH_TEXT
hal_status_t hal_eth_init_rtl8735b(hal_eth_adapter_t *peth_adapter, eth_if_sel_t if_sel, eth_pin_sel_t pin_sel)
{
	ETHERNET_Type *peth;
	u32 i, start_us;
	u16 tmp;
#define TIMEOUT_US  100000000


	if (peth_adapter == NULL) {
		DBG_ETH_ERR("Invalid parameter !!\r\n");
		return HAL_ERR_PARA;
	}
	hal_eth_irq_reg_rtl8735b((irq_handler_t)MII_IRQHandler);
	hal_irq_set_priority_rtl8735b(ETHERNET_IRQn, MII_IRQPri);

	/* enable LX bus & LX bus clock */
	HAL_WRITE32(TM9_APB_BASE, 0x220, HAL_READ32(TM9_APB_BASE, 0x220) | BIT17 | BIT16);
	hal_eth_en_ctrl_rtl8735b(ON);
	hal_eth_pin_ctrl_rtl8735b(pin_sel, ON);
	hal_eth_set_interface_rtl8735b(if_sel);
	/* enable external interrupt */
	HAL_WRITE32(0xE000E13C, 0x0, HAL_READ32(0xE000E13C, 0x0) | BIT1);

	peth_adapt = peth_adapter;
	peth_adapt->base_addr = ETHERNET;
	peth = peth_adapt->base_addr;
	peth_adapter->if_sel = if_sel;
	peth_adapter->pin_sel = pin_sel;

	/* reset PHY & disable MAC's auto-polling */
	i = 0;
	peth->miia = 0x84408000;
	while (peth->miia_b.flag) {
		hal_misc_delay_us_rtl8735b(1);
		i++;
		if (i > ETH_TIMEOUT_CNT_MAX) {
			DBG_ETH_ERR("Wait write operation flag timeout !!\r\n");
			break;
		}
	}
	hal_delay_ms(4000);  // wait PHY's N-way is completed

	/* reset MAC */
	peth->com_b.rst = 1;
	do {
	} while (peth->com_b.rst);

	/* Tx settings */
	if (if_sel == EthMiiMode) {
		peth->ms_b.reg_rmii2mii_en = 0;
		peth->ms_b.refclk_on = 0;  // REFCLK off
	} else {
		peth->ms_b.reg_rmii2mii_en = 1;  // RMII interface
		peth->ms_b.refclk_on = 1;  // REFCLK on
	}
	peth->tc_b.ifg = eth_ifg_time_3;
	peth->tc_b.lbk = eth_normal_op;

	/* Rx settings */
	peth->idr0 = ((peth_adapter->mac_id[0]) << 24) | ((peth_adapter->mac_id[1]) << 16) | ((peth_adapter->mac_id[2]) << 8) | (peth_adapter->mac_id[3]);
	peth->idr4 = ((peth_adapter->mac_id[4]) << 24) | ((peth_adapter->mac_id[5]) << 16);
	peth->rc = 0x3F;  // Accept error/runt/broadcast/multicast packets, etc.
	peth->com_b.rxjumbo = 1;  // Support jumbo packet
	peth->etnrxcpu1 = 0x01010100;

	/* I/O command */
	peth->io_cmd1 = 0x31000000;  // Extra desc. format = 011, support 1GB addressing
	peth->io_cmd = 0x40081000;  // short desc. format = 1, Tx & Rx FIFO threshold = 256 bytes

	peth_tx_desc = (hal_eth_tx_desc_t *)(peth_adapter->tx_desc_addr);
	peth_rx_desc = (hal_eth_rx_desc_t *)(peth_adapter->rx_desc_addr);
	peth->txfdp1 = (u32)peth_tx_desc;
	peth->rxfdp1 = (u32)peth_rx_desc;

	peth_tx_pkt_buf = peth_adapter->tx_pkt_buf;
	peth_rx_pkt_buf = peth_adapter->rx_pkt_buf;

	/* initialize Tx descriptors */
	for (i = 0; i < (peth_adapter->tx_desc_num); i++) {
		if (i == ((peth_adapter->tx_desc_num) - 1)) {
			peth_tx_desc[i].dw1 = BIT30;
		}
		peth_tx_desc[i].addr = (u32)(peth_tx_pkt_buf + (i * ETH_PKT_BUFF_SZ));
		peth_tx_desc[i].dw2 = (eth_vlan_hdr_remove << 25) | (ETH_C_VLAN_HDR & 0xFFFF);
		peth_tx_desc[i].dw3 = 0;
		peth_tx_desc[i].dw4 = 0;
	}

	/* initialize Rx descriptors */
	for (i = 0; i < (peth_adapter->rx_desc_num); i++) {
		if (i == ((peth_adapter->rx_desc_num) - 1)) {
			peth_rx_desc[i].dw1 = BIT31 | BIT30 | ETH_PKT_BUFF_SZ;
		} else {
			peth_rx_desc[i].dw1 = BIT31 | ETH_PKT_BUFF_SZ;
		}
		peth_rx_desc[i].addr = (u32)(peth_rx_pkt_buf + (i * ETH_PKT_BUFF_SZ));
		peth_rx_desc[i].dw2 = 0;
		peth_rx_desc[i].dw3 = 0;
	}
	if (peth_adapter->dcache_clean_by_addr != NULL) {
		peth_adapter->dcache_clean_by_addr((uint32_t *)peth_tx_desc, (int32_t)((peth_adapter->tx_desc_num) * sizeof(hal_eth_tx_desc_t)));
		peth_adapter->dcache_clean_by_addr((uint32_t *)peth_rx_desc, (int32_t)((peth_adapter->rx_desc_num) * sizeof(hal_eth_rx_desc_t)));
	}

	/* enable Tx & Rx */
	peth->io_cmd_b.te = 1;
	peth->io_cmd_b.re = 1;

	/* isr & imr */
	peth_adapt->int_mask = BIT24 | BIT22 | BIT20 | BIT16 | 0xFFFF;
	peth->isr_imr = peth_adapt->int_mask;

	/* enable auto-polling */
	peth->miia_b.disable_auto_polling = 0;

	/* Wait PHY's link is up */
	start_us = hal_misc_read_curtime_rtl8735b();
	do {
		/* 1st read */
		tmp = hal_eth_rw_phy_reg_rtl8735b(peth_adapter, eth_phy_reg_read, ETH_PHY_REG1_ADDR, ETH_PHY_REG_DEFAULT_VALUE);
		/* 2nd read */
		tmp = hal_eth_rw_phy_reg_rtl8735b(peth_adapter, eth_phy_reg_read, ETH_PHY_REG1_ADDR, ETH_PHY_REG_DEFAULT_VALUE);

		if ((TIMEOUT_US == 0) || (tmp & ETH_PHY_LINK_STATUS)) {
			break;
		} else {
			if ((TIMEOUT_US != HAL_WAIT_FOREVER) && hal_misc_is_timeout(start_us, TIMEOUT_US)) {
				DBG_ETH_ERR("Wait PHY's link up timeout !!\r\n");
				return HAL_TIMEOUT;
			} else {
				if (peth_adapter->task_yield != NULL) {
					(peth_adapter->task_yield)();
				}
			}
		}
	} while (1);

	/* Get PHY's link info. */
	tmp = hal_eth_rw_phy_reg_rtl8735b(peth_adapter, eth_phy_reg_read, ETH_PHY_REG0_ADDR, ETH_PHY_REG_DEFAULT_VALUE);
	if ((!(tmp & ETH_PHY_SPEED_MSB)) && (tmp & ETH_PHY_SPEED_LSB)) {
		if (tmp & ETH_PHY_DUPLEX_MODE) {
			DBG_ETH_INFO("PHY's link info.: 100 Mb/s, Full duplex\r\n");
		} else {
			DBG_ETH_INFO("PHY's link info.: 100 Mb/s, Half duplex\r\n");
		}
	} else if ((!(tmp & ETH_PHY_SPEED_MSB)) && (!(tmp & ETH_PHY_SPEED_LSB))) {
		if (tmp & ETH_PHY_DUPLEX_MODE) {
			DBG_ETH_INFO("PHY's link info.: 10 Mb/s, Full duplex\r\n");
		} else {
			DBG_ETH_INFO("PHY's link info.: 10 Mb/s, Half duplex\r\n");
		}
	}

	/* Wait MAC's link is up */
	start_us = hal_misc_read_curtime_rtl8735b();
	do {
		if ((TIMEOUT_US == 0) || ((peth->ms_b.linkb) == eth_link_up)) {
			break;
		} else {
			if ((TIMEOUT_US != HAL_WAIT_FOREVER) && hal_misc_is_timeout(start_us, TIMEOUT_US)) {
				DBG_ETH_ERR("Wait MAC's link up timeout !!\r\n");
				return HAL_TIMEOUT;
			} else {
				if (peth_adapter->task_yield != NULL) {
					(peth_adapter->task_yield)();
				}
			}
		}
	} while (1);

	/* Get MAC's link info. */
	switch (((peth->ms) & 0x18000000) >> 27) {
	case eth_speed_100:
		if ((peth->ms_b.fulldupreg) == eth_full_duplex) {
			DBG_ETH_INFO("MAC's link info.: 100 Mb/s, Full duplex\r\n");
		} else {
			DBG_ETH_INFO("MAC's link info.: 100 Mb/s, Half duplex\r\n");
		}
		break;
	case eth_speed_10:
		if ((peth->ms_b.fulldupreg) == eth_full_duplex) {
			DBG_ETH_INFO("MAC's link info.: 10 Mb/s, Full duplex\r\n");
		} else {
			DBG_ETH_INFO("MAC's link info.: 10 Mb/s, Half duplex\r\n");
		}
		break;
	default:
		DBG_ETH_INFO("MAC's link speed: %d\r\n", ((peth->ms) & 0x1800000) >> 2);
	}

	/* enable Rx ring1 */
	peth->io_cmd1_b.rxring1 = 1;


	return HAL_OK;
}


/**
 *  @brief To de-initialize the Ethernet MAC controller.
 *
 *  @param[in]  peth_adapter The ETHERNET adapter.
 *
 *  @returns    void.
 */
SECTION_ETH_TEXT
void hal_eth_deinit_rtl8735b(hal_eth_adapter_t *peth_adapter)
{
	if (peth_adapter == NULL) {
		DBG_ETH_ERR("Invalid parameter !!\r\n");
		return;
	}

	/* disable interrupt & clear all pending interrupts */
	peth_adapter->base_addr->isr_imr = 0xFFFF;
	hal_eth_irq_unreg_rtl8735b();
	hal_eth_set_interface_rtl8735b(EthMiiMode);
	hal_eth_pin_ctrl_rtl8735b(peth_adapter->pin_sel, OFF);
	hal_eth_en_ctrl_rtl8735b(OFF);
}


/**
 *  @brief To write "size" bytes of data from "data" to the Tx packet buffer.
 *
 *  @param[in]  peth_adapter The ETHERNET adapter.
 *  @param[in]  data The buffer of packet data.
 *  @param[in]  size The size of the packet data.
 *
 *  @returns    The number of bytes written, or (-1) if errors.
 */
SECTION_ETH_TEXT
s32 hal_eth_write_data_rtl8735b(hal_eth_adapter_t *peth_adapter, u8 *data, u32 size)
{
	ETHERNET_Type *peth;
	u8 tx_serach_idx = eth_tx_desc_wptr;


	if ((peth_adapter == NULL) || (data == NULL) || (size == 0)) {
		DBG_ETH_ERR("Invalid parameter !!\r\n");
		return -1;
	}
	peth = peth_adapter->base_addr;

	// D-Cache sync (Invalidate)
	if (peth_adapter->dcache_invalidate_by_addr != NULL) {
		peth_adapter->dcache_invalidate_by_addr((uint32_t *)peth_tx_desc, (int32_t)((peth_adapter->tx_desc_num) * sizeof(hal_eth_tx_desc_t)));
	}

	/* check if current Tx descriptor is available */
	if ((((volatile u32)(peth_tx_desc[tx_serach_idx].dw1)) & BIT31) == 0) {
		_memcpy((void *)(peth_tx_desc[tx_serach_idx].addr), data, size);
		if (peth_adapter->dcache_clean_by_addr != NULL) {
			peth_adapter->dcache_clean_by_addr((uint32_t *)(peth_tx_desc[tx_serach_idx].addr), (int32_t)ETH_PKT_BUFF_SZ);
		}

		peth_tx_desc[tx_serach_idx].dw1 &= BIT30;
		peth_tx_desc[tx_serach_idx].dw1 |= (BIT31 | BIT29 | BIT28 | BIT23 | size);
		peth_tx_desc[tx_serach_idx].dw2 = (eth_vlan_hdr_remove << 25) | (ETH_C_VLAN_HDR & 0xFFFF);
		if (peth_adapter->dcache_clean_by_addr != NULL) {
			peth_adapter->dcache_clean_by_addr((uint32_t *)peth_tx_desc, (int32_t)((peth_adapter->tx_desc_num) * sizeof(hal_eth_tx_desc_t)));
		}

		curr_data_len += size;
	} else {
		if (peth_adapter->dcache_clean_by_addr != NULL) {
			peth_adapter->dcache_clean_by_addr((uint32_t *)peth_tx_pkt_buf, (int32_t)((peth_adapter->tx_desc_num) * ETH_PKT_BUFF_SZ));
			peth_adapter->dcache_clean_by_addr((uint32_t *)peth_tx_desc, (int32_t)((peth_adapter->tx_desc_num) * sizeof(hal_eth_tx_desc_t)));
		}

		peth_adapt->int_mask |= BIT22;
		peth->isr_imr = peth_adapt->int_mask;
		/* enable Tx ring1 */
		peth->io_cmd_b.txfn1st = 1;

		DBG_ETH_WARN("Tx descriptor ring is full !!\r\n");

		return 0;
	}

	if (tx_serach_idx == ((peth_adapter->tx_desc_num) - 1)) {
		eth_tx_desc_wptr = 0;
	} else {
		eth_tx_desc_wptr++;
	}

	return size;
}


/**
 *  @brief To send the packet from Tx packet buffer.
 *
 *  @param[in]  peth_adapter The ETHERNET adapter.
 *
 *  @returns    The packet size.
 */
SECTION_ETH_TEXT
u32 hal_eth_send_pkt_rtl8735b(hal_eth_adapter_t *peth_adapter)
{
	ETHERNET_Type *peth = peth_adapter->base_addr;
	u32 size = curr_data_len;


	peth_adapt->int_mask |= BIT22;
	peth->isr_imr = peth_adapt->int_mask;
	peth->io_cmd_b.txfn1st = 1;
	curr_data_len = 0;


	return size;
}


/**
 *  @brief To receive a packet into the Rx packet buffer.
 *
 *  @param[in]  peth_adapter The ETHERNET adapter.
 *
 *  @returns    The packet size, or 0 if no packet received.
 */
SECTION_ETH_TEXT
u32 hal_eth_receive_pkt_rtl8735b(hal_eth_adapter_t *peth_adapter)
{
	ETHERNET_Type *peth = peth_adapter->base_addr;
	u32 rx_len = 0;
	u8 rx_serach_idx = eth_rx_desc_rptr;


	// D-Cache sync (Invalidate)
	if (peth_adapter->dcache_invalidate_by_addr != NULL) {
		peth_adapter->dcache_invalidate_by_addr((uint32_t *)peth_rx_desc, (int32_t)((peth_adapter->rx_desc_num) * sizeof(hal_eth_rx_desc_t)));
	}

	/* check if current Rx descriptor is available */
	if ((((volatile u32)(peth_rx_desc[rx_serach_idx].dw1)) & BIT31) == 0) {
		rx_len = (peth_rx_desc[rx_serach_idx].dw1) & 0xFFF;
	} else {
		peth_adapt->int_mask |= (BIT16 | BIT4);
		peth->isr_imr = peth_adapt->int_mask;

		return 0;
	}


	return rx_len;
}


/**
 *  @brief To read packet data from Rx packet buffer to the "data" buffer.
 *
 *  @param[in]  peth_adapter The ETHERNET adapter.
 *  @param[in]  data A buffer for the packet data.
 *  @param[in]  size The specified length (in bytes) to be read.
 *
 *  @returns    The actual size (in bytes) of data read.
 */
SECTION_ETH_TEXT
u32 hal_eth_read_data_rtl8735b(hal_eth_adapter_t *peth_adapter, u8 *data, u32 size)
{
	ETHERNET_Type *peth;
	u8 read_idx = eth_rx_desc_rptr;


	if ((peth_adapter == NULL) || (data == NULL) || (size == 0)) {
		DBG_ETH_ERR("Invalid parameter !!\r\n");
		return 0;
	}
	peth = peth_adapter->base_addr;

	// D-Cache sync (Invalidate)
	if (peth_adapter->dcache_invalidate_by_addr != NULL) {
		peth_adapter->dcache_invalidate_by_addr((uint32_t *)(peth_rx_desc[read_idx].addr), (int32_t)ETH_PKT_BUFF_SZ);
	}
	_memcpy((void *)data, (void *)((peth_rx_desc[read_idx].addr) + 2), size);

	peth_rx_desc[read_idx].dw1 &= BIT30;
	peth_rx_desc[read_idx].dw1 |= (BIT31 | ETH_PKT_BUFF_SZ);
	peth_rx_desc[read_idx].dw2 = 0;
	peth_rx_desc[read_idx].dw3 = 0;
	if (peth_adapter->dcache_clean_by_addr != NULL) {
		peth_adapter->dcache_clean_by_addr((uint32_t *)peth_rx_desc, (int32_t)((peth_adapter->rx_desc_num) * sizeof(hal_eth_rx_desc_t)));
	}

	if (read_idx == ((peth_adapter->rx_desc_num) - 1)) {
		eth_rx_desc_rptr = 0;
	} else {
		eth_rx_desc_rptr++;
	}

	peth_adapt->int_mask |= BIT16;
	peth->isr_imr = peth_adapt->int_mask;
	peth->io_cmd1_b.rxring1 = 1;  // enable Rx ring1


	return size;
}


/**
 *  @brief To get the link status.
 *
 *  @param[in]  peth_adapter The ETHERNET adapter.
 *
 *  @returns    1 for link up, 0 for link down.
 */
SECTION_ETH_TEXT
u32 hal_eth_get_link_status_rtl8735b(hal_eth_adapter_t *peth_adapter)
{
	ETHERNET_Type *peth = peth_adapter->base_addr;


	if ((peth->ms_b.linkb) == eth_link_up) {
		return 1;
	} else {
		return 0;
	}
}


/**
 *  @brief To set the link speed and duplex mode.
 *
 *  @param[in]  peth_adapter The ETHERNET adapter.
 *  @param[in]  speed The specified link speed.
 *  @param[in]  duplex The specifed duplex mode.
 *
 *  @returns    void.
 */
SECTION_ETH_TEXT
void hal_eth_set_link_rtl8735b(hal_eth_adapter_t *peth_adapter, s32 speed, s32 duplex)
{
	ETHERNET_Type *peth;
	u32 start_us;
	u16 tmp;
#define TIMEOUT_US  100000000


	if (peth_adapter == NULL) {
		DBG_ETH_ERR("Invalid parameter !!\r\n");
		return;
	}
	peth = peth_adapter->base_addr;

#if ETH_PHY_8201F_USE
	/* Switch to page 0 */
	tmp = hal_eth_rw_phy_reg_rtl8735b(peth_adapter, eth_phy_reg_write, ETH_PHY_REG31_ADDR, ETH_PHY_REG0_ADDR);
#endif

	if ((speed == (-1)) && (duplex == (-1))) {
		tmp = hal_eth_rw_phy_reg_rtl8735b(peth_adapter, eth_phy_reg_read, ETH_PHY_REG0_ADDR, ETH_PHY_REG_DEFAULT_VALUE);
		hal_eth_rw_phy_reg_rtl8735b(peth_adapter, eth_phy_reg_write, ETH_PHY_REG0_ADDR, tmp | ETH_PHY_RESTART_NWAY);
		hal_delay_ms(4000);  // wait N-way is completed
	} else {
		/* Disable Auto-negotiation & Manual settings */
		hal_eth_rw_phy_reg_rtl8735b(peth_adapter, eth_phy_reg_write, ETH_PHY_REG0_ADDR, ((speed << 13) | (duplex << 8)));
		hal_delay_ms(100);
	}

	/* Wait PHY's link is up */
	start_us = hal_misc_read_curtime_rtl8735b();
	do {
		/* 1st read */
		tmp = hal_eth_rw_phy_reg_rtl8735b(peth_adapter, eth_phy_reg_read, ETH_PHY_REG1_ADDR, ETH_PHY_REG_DEFAULT_VALUE);
		/* 2nd read */
		tmp = hal_eth_rw_phy_reg_rtl8735b(peth_adapter, eth_phy_reg_read, ETH_PHY_REG1_ADDR, ETH_PHY_REG_DEFAULT_VALUE);

		if ((TIMEOUT_US == 0) || (tmp & ETH_PHY_LINK_STATUS)) {
			break;
		} else {
			if ((TIMEOUT_US != HAL_WAIT_FOREVER) && hal_misc_is_timeout(start_us, TIMEOUT_US)) {
				DBG_ETH_ERR("hal_eth_set_link_rtl8735b(): Wait link up timeout !!\r\n");
				return;
			} else {
				if (peth_adapter->task_yield != NULL) {
					(peth_adapter->task_yield)();
				}
			}
		}
	} while (1);

	/* Get PHY's link info. */
	tmp = hal_eth_rw_phy_reg_rtl8735b(peth_adapter, eth_phy_reg_read, ETH_PHY_REG0_ADDR, ETH_PHY_REG_DEFAULT_VALUE);
	if ((!(tmp & ETH_PHY_SPEED_MSB)) && (tmp & ETH_PHY_SPEED_LSB)) {
		if (tmp & ETH_PHY_DUPLEX_MODE) {
			DBG_ETH_INFO("PHY's link info.: 100 Mb/s, Full duplex\r\n");
		} else {
			DBG_ETH_INFO("PHY's link info.: 100 Mb/s, Half duplex\r\n");
		}
	} else if ((!(tmp & ETH_PHY_SPEED_MSB)) && (!(tmp & ETH_PHY_SPEED_LSB))) {
		if (tmp & ETH_PHY_DUPLEX_MODE) {
			DBG_ETH_INFO("PHY's link info.: 10 Mb/s, Full duplex\r\n");
		} else {
			DBG_ETH_INFO("PHY's link info.: 10 Mb/s, Half duplex\r\n");
		}
	}

	/* Wait MAC's link is up */
	start_us = hal_misc_read_curtime_rtl8735b();
	if ((speed == (-1)) && (duplex == (-1))) {
		do {
			if ((TIMEOUT_US == 0) || (((peth->ms_b.nwcomplete) == eth_nway_completed) && ((peth->ms_b.linkb) == eth_link_up))) {
				break;
			} else {
				if ((TIMEOUT_US != HAL_WAIT_FOREVER) && hal_misc_is_timeout(start_us, TIMEOUT_US)) {
					DBG_ETH_ERR("hal_eth_set_link_rtl8735b(): Wait GMAC n-way completion/link up timeout !!\r\n");
					return;
				} else {
					if (peth_adapter->task_yield != NULL) {
						(peth_adapter->task_yield)();
					}
				}
			}
		} while (1);
	} else {
		do {
			if ((TIMEOUT_US == 0) || ((peth->ms_b.linkb) == eth_link_up)) {
				break;
			} else {
				if ((TIMEOUT_US != HAL_WAIT_FOREVER) && hal_misc_is_timeout(start_us, TIMEOUT_US)) {
					DBG_ETH_ERR("hal_eth_set_link_rtl8735b(): Wait GMAC link up timeout !!\r\n");
					return;
				} else {
					if (peth_adapter->task_yield != NULL) {
						(peth_adapter->task_yield)();
					}
				}
			}
		} while (1);
	}

	/* Get MAC's link info. */
	switch (((peth->ms) & 0x18000000) >> 27) {
	case eth_speed_100:
		if ((peth->ms_b.fulldupreg) == eth_full_duplex) {
			DBG_ETH_INFO("MAC's link info.: 100 Mb/s, Full duplex\r\n");
		} else {
			DBG_ETH_INFO("MAC's link info.: 100 Mb/s, Half duplex\r\n");
		}
		break;
	case eth_speed_10:
		if ((peth->ms_b.fulldupreg) == eth_full_duplex) {
			DBG_ETH_INFO("MAC's link info.: 10 Mb/s, Full duplex\r\n");
		} else {
			DBG_ETH_INFO("MAC's link info.: 10 Mb/s, Half duplex\r\n");
		}
		break;
	default:
		DBG_ETH_INFO("MAC's link speed: %d\r\n", ((peth->ms) & 0x1800000) >> 2);
	}
}


/**
 *  @brief To hook a callback function for Ethernet MAC controller interrupt.
 *
 *  @param[in]  peth_adapter The ETHERNET adapter.
 *  @param[in]  pcallback The callback function.
 *
 *  @returns    void
 */
SECTION_ETH_TEXT
void hal_eth_callback_hook_rtl8735b(hal_eth_adapter_t *peth_adapter, eth_callback_t pcallback)
{
	if ((peth_adapter == NULL) || (pcallback == NULL)) {
		DBG_ETH_ERR("Invalid parameter !!\r\n");
		return;
	}

	peth_adapter->callback = pcallback;
}


/**
 *  @brief To hook a callback function to make OS do a context-switch while waiting.
 *
 *  @param[in]  peth_adapter The ETHERNET adapter.
 *  @param[in]  task_yield The callback function.
 *
 *  @returns    void.
 */
SECTION_ETH_TEXT
void hal_eth_task_yield_hook_rtl8735b(hal_eth_adapter_t *peth_adapter, eth_task_yield task_yield)
{
	if ((peth_adapter == NULL) || (task_yield == NULL)) {
		DBG_ETH_ERR("Invalid parameter !!\r\n");
		return;
	}

	peth_adapter->task_yield = task_yield;
}

/** @} */ /* End of group hs_hal_ethernet_rom_func */
/// @endcond /* End of condition DOXYGEN_ROM_HAL_API */

/** @} */ /* End of group hs_hal_ethernet */

#endif  // end of "#if defined(CONFIG_MII_EN) && (CONFIG_MII_EN == 1)"
#endif


