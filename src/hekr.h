#ifndef _HEKR_H_
#define _HEKR_H_

#define SUCCESS			1
#define	FAILURE			0

#define __DEBUG__

#ifdef __DEBUG__
	#define DEBUG(format,...) printf("File: "__FILE__", Line: %05d: "format"\n", __LINE__, ##__VA_ARGS__)
#else
	#define DEBUG(format,...)
#endif

struct cloud_s{
	unsigned char *recv_buf;
	int recv_len;
	int fd;
	int heart_count;
}__attribute__((aligned(1))); 
typedef struct cloud_s cloud_info_s;

#define SERVER_PORT			83
#define SERVER_IP			"192.168.1.100"

#define MAX_RECV_SIZE		1024
#define MAX_SEND_SIZE		1024

uint8_t MAC[6]={0};

#define DEVTID		"GATEWAY_002016071408"
//#define DEVTID_LENGTH_MAX (sizeof(DEVTID))


#define PIPE_SIZE	512    //inpipe buffer size

#define JSON_PROTOCOL_ACTION_DEVSEND	"devSend"
#define JSON_PROTOCOL_ACTION_APPSEND	"appSend"
#define JSON_PROTOCOL_ACTION_DEVSYNC	"devSync"
#define JSON_PROTOCOL_ACTION_DEVUPGRADE	"devUpgrade"

#define JSON_PROTOCOL_FIELD_MSGID		"msgId"
#define JSON_PROTOCOL_FIELD_ACTION		"action"
#define JSON_PROTOCOL_FIELD_PARAMS		"params"
#define JSON_PROTOCOL_FIELD_CODE		"code"
#define JSON_PROTOCOL_FIELD_DESC		"desc"

#define ACTION_CODE_SUCCESS					200
#define ACTION_CODE_IDLE					201
#define ACTION_CODE_ERROR_PARSE_ERROR		2000102
#define ACTION_CODE_ERROR_ACTION_UNKNOWN	2000103
#define ACTION_CODE_ERROR_UART_TIMEOUT		2000105
#define ACTION_CODE_ERROR_FORMAT			2000106
#define ACTION_CODE_ERROR_DEVTID			2000107
#define ACTION_CODE_ERROR_CTRLKEY			2000109
#define ACTION_CODE_ERROR_EXECUTION 		2000114
#define ACTION_CODE_ERROR_BIN_TYPE			2000115
#define ACTION_CODE_ERROR_UPDATE_STARTED 	2000116
#define ACTION_CODE_ERROR_FRAME_INVALID 	2000117
#define ACTION_CODE_ERROR_INTERNAL_ERROR 	2000118
#define ACTION_CODE_ERROR_USER_NOT_EXIST 	2000119

#define TABLE_FINAL_MASK	0
#define ACTION_CODE_ITEM_FINAL {TABLE_FINAL_MASK, ""}

#define CODE_DESC_LENGTH_MAX	16
struct action_code_item
{
	uint32_t code;
	char desc[CODE_DESC_LENGTH_MAX];
};

struct action_code_item g_action_code_table[] = {
	{ ACTION_CODE_SUCCESS ,					"succuss"},
	{ ACTION_CODE_ERROR_PARSE_ERROR ,		"parse error" },
	{ ACTION_CODE_ERROR_ACTION_UNKNOWN ,	"action unknown" },
	{ ACTION_CODE_ERROR_UART_TIMEOUT ,		"uart timeout" },
	{ ACTION_CODE_ERROR_FORMAT ,			"format error" },
	{ ACTION_CODE_ERROR_DEVTID ,			"devTid error" },
	{ ACTION_CODE_ERROR_CTRLKEY ,			"ctrlKey error" },
	{ ACTION_CODE_ERROR_EXECUTION ,			"execute error" },
	{ ACTION_CODE_ERROR_BIN_TYPE ,			"bin type error" },
	{ ACTION_CODE_ERROR_UPDATE_STARTED ,	"update started" },
	{ ACTION_CODE_ERROR_FRAME_INVALID ,		"frame invalid" },
	{ ACTION_CODE_ERROR_INTERNAL_ERROR ,	"internal error" },
	{ ACTION_CODE_ERROR_USER_NOT_EXIST ,	"user not exist" },
	ACTION_CODE_ITEM_FINAL
};

#endif
