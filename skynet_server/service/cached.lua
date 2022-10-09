local skynet = require "skynet"
require "skynet.manager"
local log = require "log"
local mng = require "cached.mng"
local user = require "cached.user"
local item = require "cached.item"

local CMD = {}

function CMD.run(func_name, mod, sub_mod, id, ...)
    local func = mng.get_func(mod, sub_mod, func_name)
    local cache = mng.load_cache(mod, id)
    return func(id, cache, ...)
end

-- 关服前保存一次
function CMD.SIGHUP()
    log.info("SIGHUP to save db")
    mng.do_save_loop()
    log.info("SIGHUP save db ok")
end

skynet.start(function()
    skynet.register(".cached")
	skynet.dispatch("lua", function(_, _, cmd, ...)
		local f = assert(CMD[cmd])
        skynet.ret(skynet.pack(f(...)))
	end)
    mng.init()
    user.init()
    item.init()
end)
