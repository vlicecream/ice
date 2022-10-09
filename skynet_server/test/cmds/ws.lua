local websocket = require "http.websocket"
local json = require "json"
local mng = require "test.mng" -- 用于绑定 ws_id 和 uid
local md5 = require "md5"
local timer = require "timer"

local M = {} -- 模块接口
local CMD = {} -- 命令接口实现
local RPC = {} -- RPC 消息处理实现

-- 实现登录指令
function CMD.login(ws_id, acc)
    local token = "token"
    local checkstr = token .. acc
    local sign = md5.sumhexa(checkstr)
    local req = {
        pid = "c2s_login",
        acc = acc,
        token = token,
        sign = sign,
    }
    -- 把消息发给服务器
    websocket.write(ws_id, json.encode(req))
end

function CMD.echo(ws_id, msg)
    local req = {
        pid = "c2s_echo",
        msg = msg,
    }
    websocket.write(ws_id, json.encode(req))
end

function CMD.getname(ws_id)
    local req = {
        pid = "c2s_get_username",
    }
    websocket.write(ws_id, json.encode(req))
end

function CMD.setname(ws_id, name)
    local req = {
        pid = "c2s_set_username",
        username = name,
    }
    websocket.write(ws_id, json.encode(req))
end

-- 发送心跳协议
local function send_heartbeat(ws_id)
    local req = {
        pid = "c2s_heartbeat",
    }
    -- 把消息发给服务器
    websocket.write(ws_id, json.encode(req))
end

-- 处理登录成功的协议
function RPC.s2c_login(ws_id, res)
    mng.set_uid(res.uid)

    -- 每5秒发送一次心跳协议
    timer.timeout_repeat(5, send_heartbeat, ws_id)
end

-- 处理网络消息
function M.handle_res(ws_id, res)
    local f = RPC[res.pid]
    if f then
        f(ws_id, res)
    else
        print("recv:", json.encode(res))
    end
end

-- 执行指令
function M.run_command(ws_id, cmd, ...)
    local f = CMD[cmd]
    if not f then
        print("not exist cmd")
        return
    end
    f(ws_id, ...)
end

return M
