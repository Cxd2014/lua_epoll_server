#include <stdio.h>
#include <string.h>

#include "lua_lib.h"
#include "main.h"

/* 从luaState中取得行号及文件名 */
const char *get_lua_file_line(lua_State *L, int *lineno)
{
	static char name[LUA_IDSIZE];
	lua_Debug ar = { 0 };
	char *slash;
	
	lua_getstack(L, 1, &ar);			/* 获得调用者的栈信息 */
	lua_getinfo(L, "Sl", &ar);			/* 获得行号信息 */
	
	*lineno = ar.currentline;
	slash = strrchr(ar.short_src, '/');
	if (slash)
		slash++;
	else
		slash = ar.short_src;
	
	strncpy(name, slash, LUA_IDSIZE);

	return name;
}

static int lua_debug(lua_State *L)
{
	int lineno;
	const char *fname = get_lua_file_line(L, &lineno);
	char outbuf[1024];
	int outcur = 0;

	int n = lua_gettop(L);
	int i;
	for (i = 1; i <= n; ++i) {
		switch (lua_type(L, i)) {
		case LUA_TNUMBER:
		case LUA_TSTRING:
			outcur += snprintf(outbuf + outcur, sizeof(outbuf) - outcur, "%s", lua_tostring(L, i));
			break;
		case LUA_TBOOLEAN:
			outcur += snprintf(outbuf + outcur, sizeof(outbuf) - outcur, "%s", lua_toboolean(L, i) ? "true" : "false");
			break;
		case LUA_TNIL:
			outcur += snprintf(outbuf + outcur, sizeof(outbuf) - outcur, "nil");
			break;
		default:
			outcur += snprintf(outbuf + outcur, sizeof(outbuf) - outcur, "%s: %p", luaL_typename(L, i), lua_topointer(L, i));
			break;
		}

		if (i > 1)
			outcur += snprintf(outbuf + outcur, sizeof(outbuf) - outcur, "\t");
	}

	log_lua_debug(fname, lineno, "%s", outbuf);
	return 0;
}

void lua_debug_init(lua_State *L)
{
	lua_pushcfunction(L, lua_debug);
	lua_setglobal(L, "debug");
}
