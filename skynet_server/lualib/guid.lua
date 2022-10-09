local skynet = require "skynet"

local M = {}
local guidd

-- cfg: {
--   host = "127.0.0.1", -- 数据库 IP
--   port = 27107,       -- 数据库端口
--   username = nil,     -- 数据库账号
--   password = nil,     -- 数据库密码
--   authdb = nil,       -- 认证数据库
--   dbname = "guid",    -- 存储数据的数据库名
--   tblname = "guid",   -- 存储数据的表名
--   idtypes = {"uid" = step, "teamid" = step}, -- 配置每个 ID 类型和对应的步长
-- }
-- 初始化配置
function M.init(cfg)
	skynet.call(guidd, "lua", "init", cfg)
end

-- 取一个 ID
function M.get_guid(idtype)
	return skynet.call(guidd, "lua", "get_guid", idtype)
end

-- 创建 guidd 服务
skynet.init(function()
	guidd = skynet.uniqueservice("guidd")
end)

return M
