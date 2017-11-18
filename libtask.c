#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lua_lib.h"
#include "main.h"

hash_st *task_hash = NULL;

static int task_reg_executor(lua_State *L)
{
    struct task_st *task = (struct task_st *)zero_alloc(sizeof(struct task_st));

    const char *id = luaL_checkstring(L, 1);
    strncpy(task->id, id, MAX_ID_LEN);
	task->priority = luaL_checkint(L, 2);
    
    task->L = L; 
    lua_pushvalue(L, 3);
    task->ref = luaL_ref(L, LUA_REGISTRYINDEX);

    struct list_head *head = NULL;
    struct task_st *node = NULL;
    int flag = 0;
    if (xhash_search(task_hash, task->id, strlen(task->id) + 1, (void **)&head) == 0) {
        list_for_each_entry(node, head, list) { /* 安照priority优先级排序，priority越小优先级越高 */
            if (node->priority >= task->priority) { 
                flag = 1;
                list_add(&task->list, node->list.prev);
                break;
            }     
        }
        if (!flag) 
            list_add_tail(&task->list, head);

    } else {
        struct list_head *list_head =  (struct list_head *)zero_alloc(sizeof(struct list_head));
        INIT_LIST_HEAD(list_head);
        list_add(&task->list,list_head);
        xhash_insert(task_hash, task->id, strlen(task->id) + 1, (void *)list_head);
    }

	return 1;
}

static void pushtask(lua_State *L, struct task_st *task, struct http_request *request)
{
    struct task_udata *tsku = lua_newuserdata(L, sizeof(struct task_udata));
    tsku->task = task;
    tsku->task->request = request;

	luaL_getmetatable(L, TASK_ID);
	lua_setmetatable(L, -2);
}

int task_run_executor(const char *id, struct http_request *request)
{
    struct list_head *head = NULL;
    struct task_st *node = NULL;

    if (xhash_search(task_hash, id, strlen(id) + 1, (void **)&head) == 0) {
        list_for_each_entry(node, head, list) {
            
            lua_rawgeti(node->L, LUA_REGISTRYINDEX, node->ref);
            pushtask(node->L, node, request);

            if (lua_pcall(node->L, 1, 0, 0)) {
                log_error("error executing task, error: %s\n", lua_tostring(node->L, -1));
                lua_pop(node->L, 1);
                return -1;
            }
        }
    } else {
        log_error("Error! No such task: %s\n", id);
        return -1;
    }

    return 1;
}



static int task_run_executor_l(lua_State *L)
{
    const char *id = luaL_checkstring(L, 1);
	return task_run_executor(id, NULL);
}

static int task_get_param(lua_State *L)
{
    struct task_udata *task = (struct task_udata *)luaL_checkudata(L, 1, TASK_ID);
    hash_st *hash = task->task->request->param;

    if (hash) {
        struct param_udata *param = (struct param_udata *)lua_newuserdata(L, sizeof(struct param_udata));
        param->param = hash;
        luaL_getmetatable(L, PARAM_ID);
        lua_setmetatable(L, -2);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

#if 0
static int task_set_param(lua_State *L)
{
    struct task_udata *task = (struct task_udata *)luaL_checkudata(L, 1, TASK_ID);
	struct param_udata *param  = (struct param_udata *)luaL_checkudata(L, 2, PARAM_ID);
    
    if(param) {
        task->task->param = param->param;
    }
	return 0;
}
#endif

static int task_replay(lua_State *L)
{
    struct task_udata *task = (struct task_udata *)luaL_checkudata(L, 1, TASK_ID);

    if(lua_istable(L, 2)) {
        lua_getfield(L, 2, "code");
        lua_getfield(L, 2, "content");
        lua_getfield(L, 2, "location");
        lua_getfield(L, 2, "body");
        
        const char *body = luaL_checkstring(L, -1);
        const char *location = luaL_checkstring(L, -2);
        const char *type = luaL_checkstring(L, -3);
        const char *code = luaL_checkstring(L, -4);

        combie_http_replay(task->task->request, code, type, location, body);
        lua_settop(L, -5);
    } else {
        log_error("task replay should pass a table");
    }
    return 0;
}

static luaL_Reg task_f[] = {
    {"regExecutor", task_reg_executor},
    {"runExecutor", task_run_executor_l},
    { NULL, NULL }
};

static luaL_Reg task_m[] = {
    {"getParam", task_get_param},
    //{"setParam", task_set_param},
    {"replay", task_replay},
    { NULL, NULL }
};

int luaopen_libtask(lua_State* L) 
{
    /* 参见《programming in lua 3》第29.3节  */
    luaL_newmetatable(L, TASK_ID);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, task_m, 0);

    luaL_newlib(L,task_f);
    return 1;
}
