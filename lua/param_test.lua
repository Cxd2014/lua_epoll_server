local task = require "libtask"
local paramlib = require "libparam"

-- 测试参数
local function callback(task)
	debug("test param function run!")

	local param = task:getParam()
	param:debug()

	debug(param:get("User-Agent"))

	local replay = {
		code = "200 OK",
		content = "text/html",
		location = "",
		body = "<h1>hello lua</h1>"
	} 

	task:replay(replay)
end

--访问 http://192.168.152.129/lua_hello.html?cxd=123&qwe=321
task.regExecutor("HTTPGET:/lua_hello.html", 0, callback)
