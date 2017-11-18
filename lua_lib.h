#ifndef lua_common_h
#define lua_common_h

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "hash.h"
#include "list.h"
#include "http.h"

#define MAX_ID_LEN 128
#define MAX_VALUE_LEN 512

#define TASK_ID "task_id" 
#define PARAM_ID "param_id"


struct task_st {
	struct list_head list;
	lua_State 		 *L;			/* lua state */
    int 			 ref;		    /* callback ref */
    int              priority;              /* 优先级 */
    char             id[MAX_ID_LEN];        /* 任务ID */
    struct http_request *request;         /* 请求数据 */
};

struct task_udata {
    struct task_st *task;
};

struct param_udata{
    hash_st *param;
};

extern hash_st *task_hash;

int task_run_executor(const char *id, struct http_request *request);
void lua_debug_init(lua_State *L);
const char *get_lua_file_line(lua_State *L, int *lineno);

#endif