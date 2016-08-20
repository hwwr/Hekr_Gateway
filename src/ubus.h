/*************************************************************************

 ************************************************************************/

#ifndef _UBUS_H_
#define _UBUS_H_

#include <libubus.h>

enum {

	REGIST_DEVTYPE,
	REGIST_MID,
	REGIST_DEVNAME,
	REGIST_DEVMODEL,
	REGIST_MANUFACTURER,
	REGIST_PROTOCOL,
	REGIST_UBUSOBJECT,
	REGIST_LUASCRIPT,
	REGIST_VERSION,
	REGIST_DESCRIPTION,
	
	__REGIST_ATTR_MAX
};

extern struct ubus_context *ubus_ctx;

extern int u_pipe[2];


void ubus_run();

#endif
