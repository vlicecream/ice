local skynet = require "skynet"
local guid = require "guid"
local timer = require "timer"

local cfg = {
  host = "127.0.0.1",
  port = 27017,
  username = nil,
  password = nil,
  authdb = nil,
  dbname = "guidtest",
  tblname = "guid",
  idtypes = {uid = 10},
}

-- 申请 ID
local function test_new_uid()
    local uid = guid.get_guid("uid")
    skynet.error("--------------------uid:", uid)
end

skynet.init(function()
    guid.init(cfg)

    -- 开个循环定时器取申请 ID
    local id = timer.timeout_repeat(2, test_new_uid)

    -- 超时后关闭定时器
    skynet.timeout(18000, function ()
        skynet.error("cancel", id)
        timer.show()
        timer.cancel(id)
        timer.show()
    end)
end)
