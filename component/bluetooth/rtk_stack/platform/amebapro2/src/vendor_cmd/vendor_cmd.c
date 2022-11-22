#include <string.h>
#include <trace_app.h>
#include "vendor_cmd.h"
#include "gap_vendor.h"

P_FUN_GAP_APP_CB ext_app_cb = NULL;

#if BT_VENDOR_CMD_ONE_SHOT_SUPPORT
T_GAP_CAUSE le_vendor_one_shot_adv(void)
{
	uint8_t len = 1;
	uint8_t param[1];
	param[0] = HCI_EXT_SUB_ONE_SHOT_ADV;

	if (gap_vendor_cmd_req(HCI_LE_VENDOR_EXTENSION_FEATURE2, len, param) == GAP_CAUSE_SUCCESS) {
		return GAP_CAUSE_SUCCESS;
	}
	return GAP_CAUSE_SEND_REQ_FAILED;
}
#endif

/**
 * @brief Callback for gap common module to notify app
 * @param[in] cb_type callback msy type @ref GAP_COMMON_MSG_TYPE.
 * @param[in] p_cb_data point to callback data @ref T_GAP_CB_DATA.
 * @retval void
 */
void app_gap_vendor_callback(uint8_t cb_type, void *p_cb_data)
{
	T_GAP_VENDOR_CB_DATA cb_data;
	memcpy(&cb_data, p_cb_data, sizeof(T_GAP_VENDOR_CB_DATA));
	switch (cb_type) {
		case GAP_MSG_VENDOR_CMD_RSP:
			APP_PRINT_INFO4("GAP_MSG_VENDOR_CMD_RSP: command 0x%x, cause 0x%x, is_cmpl_evt %d, param_len %d",
							cb_data.p_gap_vendor_cmd_rsp->command,
							cb_data.p_gap_vendor_cmd_rsp->cause,
							cb_data.p_gap_vendor_cmd_rsp->is_cmpl_evt,
							cb_data.p_gap_vendor_cmd_rsp->param_len);
			break;

		default:
			break;
	}
	if (ext_app_cb) {
		ext_app_cb(cb_type, p_cb_data);
	}
	return;
}

void vendor_cmd_init(P_FUN_GAP_APP_CB app_cb)
{
	if (app_cb != NULL) {
		ext_app_cb = app_cb;
	}
	gap_register_vendor_cb(app_gap_vendor_callback);
}
