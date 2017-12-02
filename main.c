#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>

#include "lua_lib.h"
#include "epoll.h"
#include "main.h"

static void init_lua(char *filename)
{
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);

	int err = 0;
	lua_debug_init(L);
	err = luaL_loadfile(L, filename);
	if (err) 
		log_die("luaL_loadfile err %s",lua_tostring(L,-1));
	
	err = lua_pcall(L,0,0,0);
	if (err) 
		log_die("lua_pcall err %s",lua_tostring(L,-1));

}

/* 加载指定目录中的所有Lua脚本文件 */
static void init_luas(char *dir)
{
	DIR *dp;
	struct dirent *ep;
	dp=opendir(dir);
	char fpath[128] = {0};
	strcpy(fpath, dir);
	strcat(fpath, "/");
	int len = strlen(fpath);

	/* 创建一个哈希表用于存放所有注册的任务，程序在运行过程中任务始终存在，所以没有释放函数 */
	task_hash = xhash_create(NULL, NULL);

	if(dp == NULL)
		log_die("open lua dir err");
	
	while((ep = readdir(dp)) != NULL) {
		if((strcmp(ep->d_name,".") == 0) || (strcmp(ep->d_name,"..") == 0))
			continue;

		memset(fpath + len, 0, sizeof(fpath) - len);
		strcat(fpath, ep->d_name);
		init_lua(fpath);
	}
}

int main(int argc,char **argv)
{	
	if (argc != 5) {
		printf("Usage: ./main 192.168.152.129 80 lua debug\n");
		exit(0);
	}
	
	log_init("./error.log", argv[4]);
	update_log_time();
	
	init_luas(argv[3]);
	epoll_loop(argv[1], argv[2]);
	
	log_exit();
	return 0;
}
