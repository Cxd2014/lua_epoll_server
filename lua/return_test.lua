local task = require "libtask"
local paramlib = require "libparam"

-- 返回302重定向页面
local function callback(task)
	debug("test redirect function run!")

	local param = task:getParam()
	param:debug()


	local replay = {
		code = "302 Moved Temporarily",
		content = "text/html",
		location = "http://www.baidu.com",
		body = ""
	} 

	task:replay(replay)
end

--访问 http://192.168.152.129/redirect.html
task.regExecutor("HTTPGET:/redirect.html", 0, callback)
