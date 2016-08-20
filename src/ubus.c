/*************************************************************************
  > File Name: ubus.c
  > Author: lunan
  > Mail: 6616@shuncom.com 
  > Created Time: 2015年05月20日 星期三 15时13分52秒
 ************************************************************************/

#include<stdio.h>
#include <libubus.h>
#include <libubox/utils.h>
#include <libubox/blobmsg_json.h>
//lua头文件  
#include "lua.h"
#include <lualib.h> 
#include <lauxlib.h> 

#include "sqlite3.h"

#include "ubus.h"

struct ubus_context *ubus_ctx;
static struct ubus_subscriber ubus_event;
struct blob_buf b;

int u_pipe[2];


const struct blobmsg_policy register_policy[__REGIST_ATTR_MAX] = {
	[REGIST_DEVTYPE] = { "devType",BLOBMSG_TYPE_STRING},
	[REGIST_MID] = { "mid",BLOBMSG_TYPE_STRING},
	[REGIST_DEVNAME] = { "devName",BLOBMSG_TYPE_STRING},
	[REGIST_DEVMODEL] = { "devModel",BLOBMSG_TYPE_STRING},
	[REGIST_MANUFACTURER] = { "manufacturer",BLOBMSG_TYPE_STRING},
	[REGIST_PROTOCOL] = { "protocol",BLOBMSG_TYPE_STRING},
	[REGIST_UBUSOBJECT] = { "ubusObject",BLOBMSG_TYPE_STRING},
	[REGIST_LUASCRIPT] = { "luaScript",BLOBMSG_TYPE_STRING},
	[REGIST_VERSION] = { "version",BLOBMSG_TYPE_STRING},
	[REGIST_DESCRIPTION] = { "description",BLOBMSG_TYPE_STRING},
};


bool registerFunc(struct ubus_context *ctx, struct ubus_object *obj, 
		struct ubus_request_data *req, const char *method, 
		struct blob_attr *msg)
{
		struct blob_attr *tb[__REGIST_ATTR_MAX];
		
		char *devType=NULL,*mid=NULL,*devName=NULL,*devModel=NULL,*description=NULL;
		char *manufacturer=NULL,*protocol=NULL,*ubusobj=NULL,*version=NULL,*luascript=NULL;
		
		blobmsg_parse(register_policy,__REGIST_ATTR_MAX, tb, blob_data(msg), blob_len(msg));
		
		if(tb[REGIST_DEVTYPE])
		{
			devType = blobmsg_get_string(tb[REGIST_DEVTYPE]);
		}else{
			return UBUS_STATUS_NO_DATA;
		}
		
		if(tb[REGIST_MID])
		{
			mid = blobmsg_get_string(tb[REGIST_MID]);
		}else{
			return UBUS_STATUS_NO_DATA;
		}
		if(tb[REGIST_DEVNAME])
		{
			devName = blobmsg_get_string(tb[REGIST_DEVNAME]);
		}
		if(tb[REGIST_DEVMODEL])
		{
			devModel = blobmsg_get_string(tb[REGIST_DEVMODEL]);
		}
		if(tb[REGIST_MANUFACTURER])
		{
			manufacturer = blobmsg_get_string(tb[REGIST_MANUFACTURER]);
		}else{
			return UBUS_STATUS_NO_DATA;
		}
		if(tb[REGIST_PROTOCOL])
		{
			protocol = blobmsg_get_string(tb[REGIST_PROTOCOL]);
		}else{
			return UBUS_STATUS_NO_DATA;
		}
		if(tb[REGIST_UBUSOBJECT])
		{
			ubusobj = blobmsg_get_string(tb[REGIST_UBUSOBJECT]);
		}else{
			return UBUS_STATUS_NO_DATA;
		}
		if(tb[REGIST_LUASCRIPT])
		{
			luascript = blobmsg_get_string(tb[REGIST_LUASCRIPT]);
		}else{
			return UBUS_STATUS_NO_DATA;
		}
		if(tb[REGIST_VERSION])
		{
			version = blobmsg_get_string(tb[REGIST_VERSION]);
		}else{
			return UBUS_STATUS_NO_DATA;
		}
		if(tb[REGIST_DESCRIPTION])
		{
			description = blobmsg_get_string(tb[REGIST_DESCRIPTION]);
		}
		
		//write to db or update or do nothing
		sqlite3 *db;
		char *zErrMsg = 0;
		int rc;
		
		
		rc = sqlite3_open("/etc/config/devInfo.db",&db);
		
		if(rc){
			printf("Can't open database: %s\n", sqlite3_errmsg(db));
			return UBUS_STATUS_UNKNOWN_ERROR;
		}
		
		char sql[256];
		memset(sql, '\0', 256);
		sprintf(sql,"REPLACE into protocol(prototype,ubusObject,luaScript,version) VALUES('%s','%s','%s','%s')",protocol,ubusobj,luascript,version);
		
		rc = sqlite3_exec(db, sql, NULL, NULL, &zErrMsg);
		
		if(rc != SQLITE_OK){
      		printf("SQL error: %s\n", zErrMsg);
      		sqlite3_free(zErrMsg);
      		
      		sqlite3_close(db);
      		return UBUS_STATUS_UNKNOWN_ERROR;
		}
		
		
		memset(sql, '\0', 256);
		sprintf(sql,"REPLACE into mid(mid,devType,devName,devModel,manufacturer,description,prototype) VALUES('%s','%s','%s','%s','%s','%s','%s')",mid,devType,devName,devModel,manufacturer,description,protocol);
		
		rc = sqlite3_exec(db, sql, NULL, NULL, &zErrMsg);
		
		if(rc != SQLITE_OK){
      		printf("SQL error: %s\n", zErrMsg);
      		sqlite3_free(zErrMsg);
      		
      		sqlite3_close(db);
      		return UBUS_STATUS_UNKNOWN_ERROR;
		}		
		
		
		
		sqlite3_close(db);
		return UBUS_STATUS_OK;

}

static const struct ubus_method hgms_methods[] = {
//	UBUS_METHOD_NOARG("list", zha_list),
	
	UBUS_METHOD("register",registerFunc, register_policy),

};


struct ubus_object_type hgms_object_type = UBUS_OBJECT_TYPE("hgms", hgms_methods);

struct ubus_object hgms_object = {
	.name = "hgms",
	.type = &hgms_object_type,
	.methods = hgms_methods,
	.n_methods = ARRAY_SIZE(hgms_methods),
};


int ubus_envent_notify(struct ubus_context *ctx, struct ubus_object *obj,  
              struct ubus_request_data *req,  
             const char *method, struct blob_attr *msg)  
{  
    //DEBUG("\nnotify handler********************%s",method);
    
    char* root = blobmsg_format_json(blob_data(msg),0);
    if(NULL == root)return 0;
    
    //add '{' '}' for msg head and end
    int len = strlen(root) + 2;
    char* buff = (char*)malloc(len);
    
    if(NULL == buff)return 0;
    
    buff[0] = '{';
    
    memcpy(&buff[1],root,strlen(root));
    
    buff[len-1] = '}';
    buff[len] = '\0';
    
    //DEBUG("%s\n",buff);
    
    free(root);
    
    /**
     *
     * now ivoke lua script "deal_notify" function
     *
     */
    
    char* path = (char*)malloc(64);
    sprintf(path,"/etc/config/%s.lua",method);
    
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	if(luaL_loadfile(L, path) || lua_pcall(L,0,0,0))
		 error(L, "cannot run configuration file: %s", lua_tostring(L, -1));
    
    free(path);
    
  	lua_getglobal(L, "deal_notify");
	lua_pushstring(L,buff);
	
	if(lua_pcall(L,1,1,0) != 0)
		error(L, "error running function 'f': %s", lua_tostring(L, -1));
	
	//printf("lua msg:%s\n",lua_tostring(L,-1));
	char *ret_str = lua_tostring(L,-1);
	
	if(ret_str != NULL){
	
		lockf(u_pipe[1],1,0);
		write(u_pipe[1],ret_str,strlen(ret_str));
		lockf(u_pipe[1],0,0);
		
	}
	
	lua_close(L);
    free(buff);
    return 0;
    
}

static void ubus_handle_remove(struct ubus_context *ctx,  
                      struct ubus_subscriber *obj, uint32_t id)  
{  
    printf("remove handler...\n");
}


void ubus_run()
{
	int ret;
	uint32_t id;
	const char *ubus_socket = NULL;

	uloop_init();

	ubus_ctx = ubus_connect(ubus_socket);
	if(!ubus_ctx)
	{
		printf("Failed to connect to ubus\n");
	}

	ret = ubus_add_object(ubus_ctx, &hgms_object);
	if(ret)
		printf("Failed to add object: %s\n", ubus_strerror(ret));

	ubus_add_uloop(ubus_ctx);
	
	////subscribe zha notify
	ubus_lookup_id(ubus_ctx, "zha", &id);
	ubus_event.cb = ubus_envent_notify;
	ubus_event.remove_cb = ubus_handle_remove;
	if(ubus_register_subscriber(ubus_ctx, &ubus_event) == UBUS_STATUS_OK){
		ubus_subscribe(ubus_ctx, &ubus_event, id);
	}
	
	///subscribe other(rf.z-wave et.al.) notify
	
	
	//while wait
	uloop_run();
	//can not exec here
	ubus_free(ubus_ctx);
	uloop_done();
}


