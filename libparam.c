#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lua_lib.h"
#include "main.h"

# if 0
static int new_param(lua_State *L)
{
    struct param_udata *param = (struct param_udata *)lua_newuserdata(L, sizeof(struct param_udata));
    param->param = xhash_create(hash_free_mem, NULL);

    luaL_getmetatable(L, PARAM_ID);
    lua_setmetatable(L, -2);
    return 1;
}

static int add_param(lua_State *L)
{   
    size_t lname = 0, lvalue = 0;
    struct param_udata *param = (struct param_udata *)luaL_checkudata(L, 1, PARAM_ID);
    const char *name = luaL_checklstring(L, 2, &lname);
    const char *value = luaL_checklstring(L, 3, &lvalue);

    char *new_value = (char *)zero_alloc(lvalue + 1);
    strcpy(new_value, value);
    xhash_insert(param->param, name, lname + 1, (void *)new_value);

    return 0;
}

static int del_param(lua_State *L)
{
    struct param_udata *param = (struct param_udata *)luaL_checkudata(L, 1, PARAM_ID);
    size_t lname = 0;
    const char *name = luaL_checklstring(L, 2, &lname);
    
    xhash_delete(param->param, name, lname);
    return 0;
}
#endif

static int get_param(lua_State *L)
{
    struct param_udata *param = (struct param_udata *)luaL_checkudata(L, 1, PARAM_ID);
    size_t lname = 0;
    const char *name = luaL_checklstring(L, 2, &lname);
    char *value = NULL;
    
    xhash_search(param->param, name, lname + 1, (void **)&value);
    if (value) {
        lua_pushstring(L, value);
    }  else {
        lua_pushnil(L);
    }   
    return 1;
}

static int dump_param_callback(const void *key, int klen, void *val, void *data)
{
    char *buf = (char *)data;
    strncat(buf, (char *)key, LOG_STR_LEN);
    strncat(buf, ":", LOG_STR_LEN);
    strncat(buf, (char *)val, LOG_STR_LEN);
    strncat(buf, "\n", LOG_STR_LEN);
    
    return 0;
}

static int dump_param(lua_State *L)
{
    struct param_udata *param = (struct param_udata *)luaL_checkudata(L, 1, PARAM_ID);
   
    char buf[LOG_STR_LEN] = {0};
    strncat(buf, "param debug:\n", LOG_STR_LEN);
    xhash_walk(param->param, buf, dump_param_callback);
   
    int line = 0;
    const char *fname = get_lua_file_line(L, &line);
    log_lua_debug(fname, line, buf);
    return 0;
}

static luaL_Reg paramlib_f[] = {
    //{"new", new_param},
    {NULL, NULL}
};

static luaL_Reg paramlib_m[] = {
    {"get", get_param},
    {"debug", dump_param},
    /* 下面两个接口用于在Lua中自己新建的task参数才可以使用，如果是HTTP请求传递过来的参数，
       不可以使用这两个接口，会导致内存错误 
    */
    //{"add", add_param},
    //{"del", del_param},
    {NULL, NULL}
};

int luaopen_libparam(lua_State* L)
{
    /* 参见《programming in lua 3》第29.3节  */
    luaL_newmetatable(L, PARAM_ID);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, paramlib_m, 0);

    luaL_newlib(L, paramlib_f);
    return 1;
}
