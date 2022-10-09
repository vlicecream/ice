--require "test.timer"
require "test.guid"
local skynet = require "skynet"

skynet.start(function()
    skynet.error("start test service")
end)
