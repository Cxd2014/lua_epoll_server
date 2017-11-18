local task = require "libtask"
local paramlib = require "libparam"

-- 测试同一个连接返回多次时，只接受第一个返回页面
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

local function callback2(task)
	debug("test redirect function run!")

	local param = task:getParam()
	param:debug()


	local replay = {
		code = "404 Not Fonud",
		content = "text/html",
		location = "http://www.baidu.com",
		body = ""
	} 

	task:replay(replay)
end

--访问 http://192.168.152.129/return.html
task.regExecutor("HTTPGET:/return.html", 1, callback)

task.regExecutor("HTTPGET:/return.html", 0, callback2)
