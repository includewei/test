#ifndef __ATCMD_BT_H__
#define __ATCMD_BT_H__

#define CENTRAL_BIT         0x80
#define PERIPHERAL_BIT      0x40
#define SCATTERNET_BIT      0x20
#define BEACON_BIT          0x10
#define CONFIG_BIT          0x08
#define AIRSYNC_CONFIG_BIT  0x04
#define MESH_BIT            0x02
#define STACK_BIT           0x01

typedef enum {
	BT_ATCMD_SCAN,
	BT_ATCMD_CONNECT,
	BT_ATCMD_DISCONNECT,
	BT_ATCMD_AUTH,
	BT_ATCMD_GET,
	BT_ATCMD_GET_COON_INFO,
	BT_ATCMD_SEND_USERCONF,
	BT_ATCMD_UPDATE_CONN_REQUEST,
	BT_ATCMD_BOND_INFORMATION,
	BT_ATCMD_READ,
	BT_ATCMD_WRITE,
	BT_ATCMD_MODIFY_WHITELIST,
	BT_ATCMD_SET_SCAN_PARAM,
	BT_ATCMD_SET_PHY,
	BT_ATCMD_SET_TEST_SUITE,
	BT_ATCMD_SEND_INDI_NOTI,
	BT_ATCMD_CHANGE_TO_PAIR_MODE,
	BT_ATCMD_MAX
} BT_AT_CMD_TYPE;

typedef enum {
	BT_COMMAND_CENTRAL,
	BT_COMMAND_PERIPHERAL,
	BT_COMMAND_SCATTERNET,
	BT_COMMAND_BEACON,
	BT_COMMAND_CONFIG,
	BT_COMMAND_AIRSYNC_CONFIG,
	BT_COMMAND_MESH,
	BT_COMMAND_STACK,
	BT_COMMAND_SCAN,
	BT_COMMAND_CONNECT,
	BT_COMMAND_DISCONNECT,
	BT_COMMAND_AUTH,
	BT_COMMAND_GET,
	BT_COMMAND_GET_CONN_INFO,
	BT_COMMAND_SEND_USERCONF,
	BT_COMMAND_UPDATE_CONN_REQUEST,
	BT_COMMAND_BOND_INFORMATION,
	BT_COMMAND_READ,
	BT_COMMAND_WRITE,
	BT_COMMAND_MODIFY_WHITELIST,
	BT_COMMAND_SET_SCAN_PARAM,
	BT_COMMAND_SET_PHY,
	BT_COMMAND_MODIFY_ADV_INTERVAL,
	BT_COMMAND_RECONFIG,
	BT_COMMAND_SEND_INDI_NOTI,
	BT_COMMAND_MESH_RECONFIG,
	BT_COMMAND_CHANGE_TO_PAIR_MODE
} BT_COMMAND_TYPE;

#endif  /* __ATCMD_BT_H__ */

