local skynet = require "skynet"
require "skynet.manager"
local config = require "config"
local websocket = require "http.websocket"
local socket = require "skynet.socket"
local util_file = require "util.file"
local util_string = require "util.string"
local dns = require "skynet.dns"
local json = require "json"

local ws_id -- websocket 连接 ID
local cmds = {} -- 命令模块集合, 命令名: 模块

-- 搜索并加载已实现的命令
local function fetch_cmds()
    local t = util_file.scandir("test/cmds")
    for _,v in pairs(t) do
        local cmd = util_string.split(v, ".")[1]
        local cmd_mod = "test.cmds."..cmd
        cmds[cmd] = require(cmd_mod)
    end
end

-- 执行注册的命令
local function run_command(cmd, ...)
    print("run_command:", cmd, ...)
    print("ws_id:", ws_id)
    local cmd_mod = cmds[cmd]
    if cmd_mod then
        cmd_mod.run_command(ws_id, ...)
    end
end

-- 处理网络返回
local function handle_resp(ws_id, res)
    for _, cmd_mod in pairs(cmds) do
        if cmd_mod.handle_res then
            cmd_mod.handle_res(ws_id, res)
        end
    end
end

-- 网络循环
local function websocket_main_loop()
    -- 初始化网络并连接服务器
    local ws_protocol = config.get("ws_protocol")
    local ws_port = config.get("ws_port")
    local server_host = config.get("server_host")
    local url = string.format("%s://%s:%s/client", ws_protocol, server_host, ws_port)
    ws_id = websocket.connect(url)

    print("websocket connected. ws_id:", ws_id)
    -- 网络收包循环
	while true do
        local res, close_reason = websocket.read(ws_id)
        if not res then
            print("disconnect.")
            break
        end
        print("res:", ws_id, res)
        local ok, err = xpcall(handle_resp, debug.traceback, ws_id, json.decode(res))
        if not ok then
            print(err)
        end
        websocket.ping(ws_id)
	end
end

-- 切割命令
local function split_cmdline(cmdline)
	local split = {}
	for i in string.gmatch(cmdline, "%S+") do
		table.insert(split,i)
	end
	return split
end

-- 交互式命令主循环
local function console_main_loop()
	local stdin = socket.stdin()
	while true do
		local cmdline = socket.readline(stdin, "\n")
		if cmdline ~= "" then
            local split = split_cmdline(cmdline)
            local cmd = split[1]
            local ok, err = xpcall(run_command, debug.traceback, cmd, select(2, table.unpack(split)))
            if not ok then
                print(err)
            end
        end
	end
end

skynet.start(function()
	dns.server() -- 初始化 dns
    fetch_cmds() -- 搜索并加载已实现的命令
	skynet.fork(websocket_main_loop)
	skynet.fork(console_main_loop)
end)

