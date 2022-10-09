local lru = require "lru"
local util_table = require "util.table"
local timer = require "timer"
local queue = require "skynet.queue"
local log = require "log"
local config = require "config"
local mongo = require "skynet.db.mongo"
local skynet = require "skynet"

local M = {}
local CMD = {}
local cache_list -- 缓存列表 ( LRU 对象 )
local dirty_list -- 脏数据列表
local load_queue -- 数据加载队列
local dat_tbl -- MongoDB 操作对象
local init_cb_list = {} -- 数据加载后的初始化回调函数列表

-- 加载完数据后执行 mod 对应的所有初始化函数
local function run_init_cb(mod, id, dat_data)
    for sub_mod, cb in pairs(init_cb_list[mod] or {}) do
        cb(id, dat_data)
    end
end

-- 从数据库中加载数据
local function load_db(key, mod, id)
    local ret = dat_tbl:findOne({_key = key})
    if not ret then
        local dat_data = {
            _key = key,
        }
        local ok, msg, ret = dat_tbl:safe_insert(dat_data)
        if (ok and ret and ret.n == 1) then
            log.info("new dat succ. key:", key, ret._key)
            run_init_cb(mod, id, dat_data)
            return key, dat_data
        else
            return 0, "new dat error:"..msg
        end
    else
        if not ret._key then
            return 0, "cannot load dat. key:"..key
        end
        run_init_cb(mod, id, ret)
        return ret._key, ret
    end
end

-- mod 和 id 用下划线拼接成数据库的索引字段 _key
local function get_key(mod, id)
    return string.format("%s_%s", mod, id)
end

-- 加载缓存
function M.load_cache(mod, id)
    local key = get_key(mod, id)
    local cache = cache_list:get(key)
    if cache then
        cache._ref = cache._ref + 1 -- 引用计数自增
        dirty_list[key] = true -- 标记此 cache 已脏
        return cache
    end

    -- 加载数据
    local _key, cache = load_queue(load_db, key, mod, id)
    assert(_key == key)
    cache_list:set(key, cache) -- 把 cache 存入 cache_list
    cache._ref = 1 -- 初始化引用计数
    dirty_list[key] = true -- 标记此 cache 已脏
    return cache
end

-- 释放缓存
function M.release_cache(mod, id, cache)
    local key = get_key(mod, id)
    cache._ref = cache._ref - 1
    if cache._ref < 0 then
        log.error("cache ref wrong. key:", key, ",ref:", ref)
    end
end

-- 获取执行函数
function M.get_func(mod, sub_mod, func_name)
    -- 函数名由 mod, sub_mod, func_name 用下划线连接拼接
    func_name = string.format("%s_%s_%s", mod, sub_mod, func_name)
    log.debug("func_name:", func_name)
    local func = assert(CMD[func_name])
    -- 封装函数闭包，确保函数执行完之后能够执行 release_cache 函数释放 cache
    return function (id, cache, ...)
        local ret = table.pack(pcall(func, id, cache, ...))
        M.release_cache(mod, id, cache)
        return select(2, table.unpack(ret))
    end
end

-- 注册执行函数
function M.regist_cmd(mod, sub_mod, func_list)
    for func_name, func in pairs(func_list) do
        func_name = string.format("%s_%s_%s", mod, sub_mod, func_name)
        CMD[func_name] = func
    end
end

-- 注册数据初始化函数
function M.regist_init_cb(mod, sub_mod, init_cb)
    if not init_cb_list[mod] then
        init_cb_list[mod] = {}
    end
    init_cb_list[mod][sub_mod] = init_cb
end

-- 移除缓存回调
local function cache_remove_cb(key, cache)
    if cache._ref > 0 or dirty_list[key] then
        -- push again
        cache_list:set(key, cache, true)
    end
end

-- 把缓存同步到数据库
local function do_save(key, cache)
    local data = {
        ["$set"] = cache
    }
    local _ok, ok, _, ret = xpcall(dat_tbl.safe_update, debug.traceback, dat_tbl, {_key = key}, data, true, false)
    if not _ok or not (ok and ret and ret.n == 1) then
        log.error("save dat error. key:", key, _ok, ok, util_table.tostring(ret))
    end
end

-- 把脏的缓存写到数据库
function M.do_save_loop()
    for key,_ in pairs(dirty_list) do
        log.info("save. key:", key)
        local cache = cache_list:get(key)
        if cache then
            do_save(key, cache)
        else
            log.error("save but no cache. key:", key)
        end
        dirty_list[key] = nil
    end
end

-- 初始化数据库连接
local function init_db()
    local cfg = config.get_db_conf()
    local dbs = mongo.client(cfg)

    local db_name = "ppt"
    log.info("connect to db:", db_name)
    local db = dbs[db_name]

    dat_tbl = db.dat
    dat_tbl:createIndex({{_key = 1}, unique = true})
end

function M.init()
    init_db() -- 初始化数据库连接
    local max_cache_cnt = config.get("max_cache_cnt")
    cache_list = lru.new(max_cache_cnt, cache_remove_cb)
    dirty_list = {}
    load_queue = queue()
    local save_interval = config.get("save_interval")
    timer.timeout_repeat(save_interval, M.do_save_loop)
end

-- 推送消息给客户端
-- TODO: 有优化的余地，可以在此处判断客户端不在线
function M.send_to_client(uid, res)
    skynet.send(".ws_agent", "lua", "send_to_client", tonumber(uid), res)
end

return M
