local task = require "libtask"
local paramlib = require "libparam"

-- 测试任务注册

local function callback1(task)
	debug("callback1 function run!")
end

local function callback2(task)
	debug("callback2 function run!")
end

local function callback3()
    debug("callback3 function run!")
end

local function callback4()
	debug("callback4 function run!")
end

local function callback5()
	debug("callback5 function run!")
end

local function callback6()
	debug("callback6 function run!")
end

local function callback7()
	debug("callback7 function run!")
end

task.regExecutor("HTTPGET:/lua_hello.html", 6, callback7)
task.regExecutor("HTTPGET:/lua_hello.html", 5, callback6)
task.regExecutor("HTTPGET:/lua_hello.html", 4, callback5)
task.regExecutor("HTTPGET:/lua_hello.html", 3, callback4)
task.regExecutor("HTTPGET:/lua_hello.html", 2, callback3)
task.regExecutor("HTTPGET:/lua_hello.html", 1, callback2)
task.regExecutor("HTTPGET:/lua_hello.html", 0, callback1)

--访问 http://192.168.152.129/lua_hello.html?cxd=123&qwe=321
