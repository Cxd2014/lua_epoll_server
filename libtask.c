#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lua_lib.h"
#include "main.h"

hash_st *task_hash = NULL;

/* 任务的注册 */
static int task_reg_executor(lua_State *L)
{
    struct task_st *task = (struct task_st *)zero_alloc(sizeof(struct task_st));

    const char *id = luaL_checkstring(L, 1);
    strncpy(task->id, id, MAX_ID_LEN);
	task->priority = luaL_checkint(L, 2);
    
    task->L = L; 
    lua_pushvalue(L, 3); /* 将第三个参数，也就是钩子函数的地址放在站顶 */
    task->ref = luaL_ref(L, LUA_REGISTRYINDEX); /* 将栈顶元素放到 LUA_REGISTRYINDEX 这个注册表中 */

    struct list_head *head = NULL;
    struct task_st *node = NULL;
    int flag = 0;
    if (xhash_search(task_hash, task->id, strlen(task->id) + 1, (void **)&head) == 0) {
        list_for_each_entry(node, head, list) { /* 哈希表中已经有这个ID则安照priority优先级顺序加入到链表中，priority越小优先级越高 */
            if (node->priority >= task->priority) { 
                flag = 1;
                list_add(&task->list, node->list.prev);
                break;
            }     
        }
        if (!flag) 
            list_add_tail(&task->list, head);

    } else { /* 哈希表中没有这个ID，则创建一个新的链表 */
        struct list_head *list_head =  (struct list_head *)zero_alloc(sizeof(struct list_head));
        INIT_LIST_HEAD(list_head);
        list_add(&task->list,list_head);
        xhash_insert(task_hash, task->id, strlen(task->id) + 1, (void *)list_head);
    }

	return 1;
}

static void pushtask(lua_State *L, struct task_st *task, struct http_request *request)
{
    /* 在Lua中分配一个空间存储用户自定义类型的数据 -- 一个任务结构体的指针 */
    struct task_udata *tsku = lua_newuserdata(L, sizeof(struct task_udata));
    tsku->task = task;
    tsku->task->request = request;

    /* 将自定义数据压入栈中，作为钩子函数的参数 */
	luaL_getmetatable(L, TASK_ID);
	lua_setmetatable(L, -2);
}

/* 任务的执行 */
int task_run_executor(const char *id, struct http_request *request)
{
    struct list_head *head = NULL;
    struct task_st *node = NULL;

    if (xhash_search(task_hash, id, strlen(id) + 1, (void **)&head) == 0) {
        list_for_each_entry(node, head, list) { /* 遍历链表 */
            
            lua_rawgeti(node->L, LUA_REGISTRYINDEX, node->ref); /* 将 LUA_REGISTRYINDEX 这个注册表中的钩子函数取出来放在栈上 */
            pushtask(node->L, node, request); /* 设置钩子函数需要传入的参数 */

            /* 调用钩子函数 */
            if (lua_pcall(node->L, 1, 0, 0)) {
                log_error("error executing task, error: %s\n", lua_tostring(node->L, -1));
                lua_pop(node->L, 1);
                return -1;
            }
        }
    } else { /* 没有找到此URI注册的钩子函数 */
        log_debug("No such task: %s", id);
        return -1;
    }

    return 1;
}



static int task_run_executor_l(lua_State *L)
{
    const char *id = luaL_checkstring(L, 1);
	return task_run_executor(id, NULL);
}

/* 获取HTTP请求中携带的所有参数 */
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
        /* 将一个table中的数据放到栈顶 */
        lua_getfield(L, 2, "code");
        lua_getfield(L, 2, "content");
        lua_getfield(L, 2, "location");
        lua_getfield(L, 2, "body");
        
        /* 从栈上取出table中的数据 */
        const char *body = luaL_checkstring(L, -1);
        const char *location = luaL_checkstring(L, -2);
        const char *type = luaL_checkstring(L, -3);
        const char *code = luaL_checkstring(L, -4);

        combie_http_lua_replay(task->task->request, code, type, location, body);
        lua_settop(L, -5); /* 还原栈顶 */
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
