local skynet = require "skynet"
local log = require "log"

local traceback = debug.traceback
local tpack = table.pack
local tunpack = table.unpack
local collectgarbage = collectgarbage
local xpcall = xpcall

local M = {}

local is_init = false   -- 初始化标记
local timer_inc_id = 1  -- 定时器自增 ID
local cur_frame = 0     -- 当前帧，一帧对应一秒
local timer2frame = {}  -- 定时器 ID 对应帧 timerid: frame
local frame2callouts = {} -- 帧对应此帧的回调函数 frame: callouts { timers : { timerid: {cb, args, is_repeat, sec} }, size: 1}
local timer_size = 0    -- 定时器数量
local frame_size = 0    -- 帧数量
local cur_timestamp = 0 -- 当前循环执行到哪一秒

-- 获取当前时间戳
local function now()
    return skynet.time() // 1
end

-- 删除定时器
local function del_timer(id)
    if not timer2frame[id] then return end

    local frame = timer2frame[id]

    local callouts = frame2callouts[frame]
    if not callouts then return end
    if not callouts.timers then return end

    if callouts.timers[id] then
        callouts.timers[id] = nil
        callouts.size = callouts.size - 1
    end

    if callouts.size == 0 then
        frame2callouts[frame] = nil
        frame_size = frame_size - 1
    end

    timer2frame[id] = nil
    timer_size = timer_size - 1
end

-- 初始化一个定时器
local function init_callout(id, sec, f, args, is_repeat)
    -- 校正帧数
    local frame = sec
    if now() > cur_timestamp then
        frame = frame + 1
    end

    -- 计算执行帧
    local fixframe = cur_frame + frame

    -- 初始化 callouts
    local callouts = frame2callouts[fixframe]
    if not callouts then
        callouts = {timers = {}, size = 1}
        frame2callouts[fixframe] = callouts
        frame_size = frame_size + 1
    else
        callouts.size = callouts.size + 1
    end

    -- 插入 f 和 args 等数据到 callouts
    callouts.timers[id] = {f, args, is_repeat, sec}
    timer2frame[id] = fixframe

    timer_size = timer_size + 1

    -- 定时器器数量过大时给出警告打印
    if timer_size == 50000 then
        log.warn("timer is too many!")
    end
end

-- 定时器循环
local function timer_loop()
    -- 下一秒继续进入此循环
    skynet.timeout(100, timer_loop)
    cur_timestamp = now()

    -- 帧数自增
    cur_frame = cur_frame + 1

    -- 没有定时器要执行，直接返回
    if timer_size <= 0 then return end

    -- 取出当前帧需要执行的定时器回调函数
    local callouts = frame2callouts[cur_frame]
    if not callouts then return end

    -- 当前帧的回调函数没有了
    if callouts.size <= 0 then
        frame2callouts[cur_frame] = nil
        frame_size = frame_size - 1
        return
    end

    -- 处理当前帧的回调函数列表
    for id, info in pairs(callouts.timers) do
        local f = info[1]
        local args = info[2]
        local ok, err = xpcall(f, traceback, tunpack(args, 1, args.n))
        if not ok then
            log.error("crontab is run in error:", err)
        end

        -- 处理完了，删掉定时器
        del_timer(id)

        -- 如果时循环定时器，则再次加入到定时器里面
        local is_repeat = info[3]
        if is_repeat then
            local sec = info[4]
            init_callout(id, sec, f, args, true)
        end
    end

    -- 当前帧已经处理完了，删掉当前帧
    if frame2callouts[cur_frame] then
        frame2callouts[cur_frame] = nil
        frame_size = frame_size - 1
    end
end

-- 检查定时器是否存在
function M.exist(id)
    if timer2frame[id] then return true end
    return false
end

-- 新增一个超时定时器，sec 秒后执行函数 f
-- 返回定时器 ID
function M.timeout(sec, f, ...)
    assert(sec > 0)
    timer_inc_id = timer_inc_id + 1
    init_callout(timer_inc_id, sec, f, tpack(...), false)
    return timer_inc_id
end

-- 取消一个定时器
function M.cancel(id)
    del_timer(id)
end

-- 新增一个循环定时器，sec 秒后执行，且间隔 sec 秒，执行函数 f
function M.timeout_repeat(sec, f, ...)
    assert(sec > 0)
    timer_inc_id = timer_inc_id + 1
    init_callout(timer_inc_id, sec, f, tpack(...), true)
    return timer_inc_id
end

-- 获取定时器还有多久执行
function M.get_remain(id)
    local frame = timer2frame[id]
    if frame then
        -- 每帧为1秒
        return frame - cur_frame
    end
    return -1
end

-- 打印定时器数据结构
function M.show()
    log.info("timer_size:", timer_size)
    log.info("frame_size:", frame_size)

    local util_table = require "util.table"
    log.info("timer2frame:", util_table.tostring(timer2frame))
    log.info("frame2callouts:", util_table.tostring(frame2callouts))
end

-- 初始化定时器
if not is_init then
    skynet.timeout(100, timer_loop)
    log.info("timer init succ.")
    is_init = true
end

return M
