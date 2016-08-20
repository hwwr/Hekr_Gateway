#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/epoll.h> 
#include <sys/wait.h> 
#include "cJSON.h"
#include <netinet/tcp.h>
//lua头文件  
#include "lua.h"
#include <lualib.h> 
#include <lauxlib.h>     

#include "sqlite3.h"

#include "ubus.h"
#include "hekr.h"


static pthread_t cloud_thread_t;
static pthread_cond_t cloud_cond;
static pthread_mutex_t cloud_mutex;

cloud_info_s cloud_info;


static uint8_t msgId = 0;


static pthread_t pipe_thread_t;


int cloud_send_data(char *data,int data_len)
{
	extern cloud_info_s cloud_info;
	int ret = 0;
	
	if((!data)||(data_len <= 0)||(cloud_info.fd <= 0))
	{
		DEBUG("para error");
		return FAILURE;
	}
	
	ret = send(cloud_info.fd,data,data_len,0);

	return ret;
}



int cloud_is_ip_add(char *add,int add_len)
{
	int i = 0;

	for(i = 0;i < add_len;i++)
	{
		if(((add[i] > '9')||(add[i] < '0'))&&(add[i] != '.'))
			return FAILURE;
	}
	return SUCCESS;
}

struct in_addr cloud_get_addr(char *add,int add_len)
{
	struct hostent *host;
	struct in_addr sin_add;
	sin_add.s_addr = 0xffffffff;
	
	if(cloud_is_ip_add(add,add_len))
	{
		if(FAILURE == inet_aton(add,&sin_add))
			DEBUG("ip addr format error");
		else
			;
	}
	else
	{
		if((host=gethostbyname(add))==NULL)
			DEBUG("gethostbyname error");
		else
		{
			if(host->h_addrtype == AF_INET)
			{
				sin_add = *((struct in_addr *)host->h_addr); 
				DEBUG("get ip by hostname[%s]",inet_ntoa(sin_add));
			}
			else
				DEBUG("don`t get ipv4");
		}
	}

	return sin_add;
}

int cloud_init_socket(cloud_info_s *p,char *add,int add_len)
{
	const unsigned int chOpt = 1;
	struct sockaddr_in server_addr;
	struct in_addr sin_add;
	int sockfd = -1;

	sin_add = cloud_get_addr(add,add_len);
	if(sin_add.s_addr == 0xffffffff)
	{
		DEBUG("get sin_add error");
		return FAILURE;
	}

	
	bzero(&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	server_addr.sin_addr = sin_add;

	
	if((sockfd=socket(AF_INET,SOCK_STREAM,0)) < 0) // AF_INET:Internet;SOCK_STREAM:TCP
	{
		DEBUG("Socket create Error");
		return FAILURE;
	}
	
	//get eth1 mac
	struct ifreq req;
	strcpy(req.ifr_name,"eth1");
	if(ioctl(sockfd,SIOCGIFHWADDR,&req)<0){
		DEBUG("Get MAC Error");
		return FAILURE;
	}
	uint8_t i;
	for(i=0;i<6;i++)
		MAC[i] = req.ifr_hwaddr.sa_data[i];
	

	if(setsockopt(sockfd,IPPROTO_TCP,TCP_NODELAY,&chOpt,sizeof(int)) == -1)
	{
		close(sockfd);
		DEBUG("set socket opt error");
		return FAILURE;
	}
	
	if(connect(sockfd,(struct sockaddr *)(&server_addr),sizeof(struct sockaddr)) < 0)
	{
		close(sockfd);
		return FAILURE;
	}
	else
	{
		p->fd = sockfd;
		return SUCCESS;
	}
}




//you must free desc
char *get_code_desc(uint32_t code)
{
	const struct action_code_item *item_cur = g_action_code_table;
	struct action_code_item item;
	char *desc = NULL;

	do
	{
		memcpy(&item, item_cur, sizeof(struct action_code_item));
		if (item.code == TABLE_FINAL_MASK) { return NULL; }
		if (item.code == code)
		{
			uint32_t desc_len = strlen(item.desc);
			desc = (char *)malloc(desc_len + 1);
			if (desc == NULL) { return NULL; }
			memcpy(desc, item.desc, desc_len);
			desc[desc_len] = '\0';
			return desc;
		}
		item_cur++;
	} while (1);
	
	return NULL;
}


void action_resp_handle(cJSON *msgid, cJSON *action, cJSON *params, uint32_t code)
{
	char *ret_str = NULL, *desc = NULL;
	cJSON *ret = NULL;
	char action_resp[20] = { 0 };
	
	ret = cJSON_CreateObject();
	
	sprintf(action_resp, "%sResp", action->valuestring);
	desc = get_code_desc(code);
	cJSON_AddNumberToObject(ret, JSON_PROTOCOL_FIELD_MSGID, msgid->valueint);
	cJSON_AddStringToObject(ret, JSON_PROTOCOL_FIELD_ACTION, action_resp);
	cJSON_AddNumberToObject(ret, JSON_PROTOCOL_FIELD_CODE, code);
	cJSON_AddStringToObject(ret, JSON_PROTOCOL_FIELD_DESC, desc == NULL ? "" : desc);
	
	if (params)
	{
		cJSON_AddItemToObject(ret, JSON_PROTOCOL_FIELD_PARAMS, params);
		ret_str = cJSON_PrintUnformatted(ret);
		cJSON_DetachItemFromObject(ret, JSON_PROTOCOL_FIELD_PARAMS);
	}
	else
	{
		ret_str = cJSON_PrintUnformatted(ret);
	}
	
	
	if(ret_str != NULL)
		cloud_send_data(ret_str,strlen(ret_str));
	
	if (ret) { cJSON_Delete(ret); }
	if (ret_str) { free(ret_str); }
	if (desc) { free(desc); }
	
	return;
}


void *cloud_thread(void *arg)
{
	cloud_info_s *p_info = (cloud_info_s *)arg;
	int epfd = -1;
	int nfds = -1;
	struct epoll_event ev, events[20];
	int i = 0;
	
	
	while (1) {	
RESTART:		
		sleep(1);
		if(epfd < 0)
		{
			if((epfd = epoll_create(3)) < 0)
			{
				DEBUG("epoll_create error");
				continue;
			}
		}
		if(p_info->fd < 0)
		{
			
			if(FAILURE == cloud_init_socket(p_info,SERVER_IP,strlen(SERVER_IP)))
				continue;
		}
		
		ev.data.fd = p_info->fd;
		ev.events= EPOLLIN|EPOLLHUP|EPOLLERR|EPOLLONESHOT|EPOLLET;		
		
	    if(0 != epoll_ctl(epfd,EPOLL_CTL_ADD,p_info->fd,&ev))
	    {
	    	DEBUG("epoll_ctl error");
			close(epfd);
			epfd = -1;
			continue;
	    }
	    
	    while(1)
		{
			nfds=epoll_wait(epfd,events,20,2000);
			
			for(i = 0;i < nfds;i++)
			{
				if(events[i].data.fd == p_info->fd)
				{
					if(events[i].events&EPOLLIN)
					{
						p_info->recv_len = recv(p_info->fd,p_info->recv_buf,MAX_RECV_SIZE, 0);
						//DEBUG("->>recv len[%d]",p_info->recv_len);
						if(p_info->recv_len< 0)
						{
							DEBUG("recv data error");
							close(p_info->fd);
							p_info->fd = -1;
							goto RESTART;
						}
						else if(p_info->recv_len == 0)
						{
							DEBUG("server close tcp");
							close(p_info->fd);
							p_info->fd = -1;
							goto RESTART;
						}
						else
						{
							//DEBUG("->>recv data:%s",p_info->recv_buf);
							uint32_t code = ACTION_CODE_SUCCESS;
							
							cJSON* root = cJSON_Parse(p_info->recv_buf);
							
							cJSON *msgid = cJSON_GetObjectItem(root, JSON_PROTOCOL_FIELD_MSGID);
							cJSON *action = cJSON_GetObjectItem(root,JSON_PROTOCOL_FIELD_ACTION);
							cJSON *params = cJSON_GetObjectItem(root, JSON_PROTOCOL_FIELD_PARAMS);
							
							if(strcmp(action->valuestring,JSON_PROTOCOL_ACTION_APPSEND) == 0){
								cJSON *devtid = cJSON_GetObjectItem(params, "devTid");
								cJSON *ctrlkey = cJSON_GetObjectItem(params, "ctrlKey");
								/*match tid*/
								if(strcmp(DEVTID, devtid->valuestring) == 0){
									
									/*match ctrlkey*/
									////////
									
									lua_State* L = luaL_newstate();
									luaL_openlibs(L);
									if(luaL_loadfile(L, "/etc/config/zha.lua") || lua_pcall(L,0,0,0)){
										 error(L, "cannot run configuration file: %s", lua_tostring(L, -1));
										 code = ACTION_CODE_ERROR_INTERNAL_ERROR;
										 lua_close(L);
									}else{
								
									  	lua_getglobal(L, "deal_appsend");

										lua_pushstring(L,p_info->recv_buf);
									
										if(lua_pcall(L,1,1,0) != 0){
											error(L, "error running function 'f': %s", lua_tostring(L, -1));
											code = ACTION_CODE_ERROR_INTERNAL_ERROR;
										
										}else{//success
											code = lua_tointeger(L,-1);
											lua_close(L);
										}
									}
								
								}else{
								
									code = ACTION_CODE_ERROR_DEVTID;
								}
								
							}else if(strcmp(action->valuestring,JSON_PROTOCOL_ACTION_DEVSYNC) == 0){
							
							
							
							}else if(strcmp(action->valuestring,JSON_PROTOCOL_ACTION_DEVUPGRADE) == 0){
							
							
							}
							
							//sendback Resp
							action_resp_handle(msgid,action,params,code);
							
							cJSON_Delete(root);
							root = NULL;
							
							memset(p_info->recv_buf,0,MAX_RECV_SIZE);
							p_info->recv_len = 0;
							ev.data.fd = p_info->fd;
							ev.events= EPOLLIN|EPOLLHUP|EPOLLERR|EPOLLONESHOT|EPOLLET;
						    if(0 != epoll_ctl(epfd,EPOLL_CTL_MOD,p_info->fd,&ev))
						    {
						    	DEBUG("epoll_ctl error");
								close(epfd);
								epfd = -1;
								goto RESTART;
						    }
						}
					}
					if(events[i].events&EPOLLHUP)
					{
						DEBUG("tcp has been hang up");
						close(p_info->fd);
						p_info->fd = -1;
						goto RESTART;
					}
					if(events[i].events&EPOLLERR)
					{
						DEBUG("tcp error");
						close(p_info->fd);
						p_info->fd = -1;
						goto RESTART;
					}
				}
			}
		}
	}
}


void *pipe_thread(void *arg)
{
	char inpipe[PIPE_SIZE];
	while(1)
	{
		
		memset(inpipe,0,PIPE_SIZE);
		
		read(u_pipe[0],inpipe,PIPE_SIZE);
		
		cJSON* lua_msg = cJSON_Parse(inpipe);
		if(!lua_msg){
			DEBUG("cJSON_Parse error");
		}else{
		
			cJSON *msgid = cJSON_CreateNumber(++msgId);
			cJSON_ReplaceItemInObject(lua_msg, JSON_PROTOCOL_FIELD_MSGID, msgid);

			cJSON *params = cJSON_GetObjectItem(lua_msg, JSON_PROTOCOL_FIELD_PARAMS);
			
			cJSON *devtid = cJSON_CreateString(DEVTID);
			cJSON_ReplaceItemInObject(params, "devTid", devtid);
			
			cJSON *action = cJSON_GetObjectItem(lua_msg,JSON_PROTOCOL_FIELD_ACTION);
			if(strcmp(action->valuestring,JSON_PROTOCOL_ACTION_DEVSEND) == 0){
				cJSON *apptid = cJSON_CreateArray();
				cJSON_ReplaceItemInObject(params, "appTid", apptid);
		    }
		    
		    char *ret_str = cJSON_PrintUnformatted(lua_msg);
			//DEBUG("ret msg:%s",ret_str);
			if(ret_str != NULL)
				cloud_send_data(ret_str,strlen(ret_str));
			
			cJSON_Delete(lua_msg);
			lua_msg = NULL;
			
			free(ret_str);
			
		}
	}
}


int main(void)
{
	
	/**
	 * build pipe
	 */
	if(pipe(u_pipe) == -1){
		
		DEBUG("pipe create error");
		exit(1);
	}
	
	pid_t child = fork();
	
	if(child == -1){
	
		DEBUG("child pid create error");
		exit(1);
		
	}else if(child == 0){
	
		ubus_run();
		
		exit(0);
	
	}else{
		
		pthread_mutex_init(&cloud_mutex, NULL);
		pthread_cond_init(&cloud_cond, NULL);
		
		cloud_info.fd = -1;
		cloud_info.recv_len = 0;
		cloud_info.heart_count = 0;
		cloud_info.recv_buf = (unsigned char *)malloc(MAX_RECV_SIZE);
		
		if(!cloud_info.recv_buf) DEBUG("malloc recv buff error");
		
		//thread for receiving msg from cloud
		if(pthread_create(&cloud_thread_t,NULL,cloud_thread,(void *)&cloud_info) != 0)
		{
			DEBUG("cloud_thread create error:%s",strerror(errno)); 
			exit(1);
		}
		//thread for receiving msg from PIPE
		if(pthread_create(&pipe_thread_t,NULL,pipe_thread,NULL) != 0)
		{
			DEBUG("pipe_thread create error:%s",strerror(errno)); 
			exit(1);
		}
		
		
		pthread_join(cloud_thread_t, NULL);
		pthread_join(pipe_thread_t, NULL);
		
	}
	
	
	return 0;
}





