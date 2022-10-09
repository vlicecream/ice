local skynet = require "skynet"
local mongo = require "skynet.db.mongo"

local CMD = {}

-- block_datas = {
--    [idtype] = {
--      blocks = {
--         [start_idx] = len,  -- 有效数据, 表示从某段开始, 长度为 len , 使用时段内自减
--      },
--      cur_idx = 0,  -- 记录当前在使用的段 ID , 当某段不足 step/10 时, 自动申请, 优先使用低段位
--      step = step,  -- 每次从 db 取的 id 数量
--      ids_cnt = 0,  -- 当前空闲 ID 总数
--    }
-- }
local block_datas     -- ID 池子
local creator         -- 生产协程
local is_busy = false -- 是否繁忙
local tbl_guid        -- 数据库操作对象

-- 从数据库申请新的 ID 段
local function new_db_id(idtype, step)
    local ret = tbl_guid:findAndModify({query = {idtype = idtype}, update = {["$inc"] = {nextid = step}}, upsert = true})

    local result = math.floor(ret.ok)
    if result ~= 1 then
        skynet.error("new_db_id not ret. idtype:", idtype, ",step:", step, ",msg:", ret.errmsg)
        return
    end

    if not ret.value.nextid then
        skynet.error("new_db_id failed. ignore first step. idtype:", idtype, ",step:", step)
        return
    end

    return ret.value.nextid
end

-- 从数据库申请新的 ID 段
local function init_generator(idtype, step)
    local id = new_db_id(idtype, step)
    if not id then
        return
    end

    skynet.error("get new id block. start:", id, ", step:", step)
    block_datas[idtype].blocks[id] = step
end

-- 申请新的 ID 段
local function try_init_generator(idtype)
    local info = block_datas[idtype]
    local cnt = 0
    for idx, size in pairs(info.blocks) do
        cnt = cnt + size
    end

    -- 申请新的 ID 段
    local step = info.step
    if cnt < step/10 then
        if not init_generator(idtype, step) then
            skynet.error("cannot get new id. idtype:", idtype)
        end
    end
end

-- 更新生成器: 重新计算 cur_idx 和 ids_cnt
local function update_generator(idtype)
    local info = block_datas[idtype]
    local old_block = info.cur_idx or 0
    local cnt = 0
    -- 选最小的 cur_idx 和 计算 ids_cnt
    for idx, size in pairs(info.blocks) do
        if not info.cur_idx then
            info.cur_idx = idx
        elseif info.cur_idx > idx then
            info.cur_idx = idx
        end
        cnt = cnt + size
    end

    info.ids_cnt = cnt
    if old_block ~= info.cur_idx then
        skynet.error("switch id block. idtype:", idtype, ",cur:", info.cur_idx, ",old:", old_block, ",ids_cnt:", cnt)
    end
end

-- 从池子里取一个 ID
local function get_new_id(idtype)
    local info = block_datas[idtype]

    if not info.cur_idx then
        -- 处理换 block 的情况
        if info.ids_cnt > 0 then
            -- 更新生成器
            update_generator(idtype)
            if not info.cur_idx then
                skynet.error("new guid too busy. idtype:", idtype)
                return
            end
        else
            skynet.error("id pool null. idtype:", idtype)
            return
        end
    end

    local cur_idx = info.cur_idx
    local diff = info.blocks[cur_idx]
    if diff <= 0 then
        --本段已经消耗完，正在切段
        skynet.error("id block all used. idtype:", idtype)
        return
    end

    -- 计算产出的 ID
    local new_id = diff + cur_idx
    if diff == 1 then
        skynet.error("id block used. cur:", cur_idx)
        info.blocks[cur_idx] = nil
        info.cur_idx = nil
    else
        info.blocks[cur_idx] = diff - 1
    end
    info.ids_cnt = info.ids_cnt - 1

    -- 当存量低于阀值
    if not is_busy and info.ids_cnt < info.step/10 then
        is_busy = true
        skynet.wakeup(creator)
    end

    skynet.error("consume ok. guid:", new_id)
    return new_id
end

local function create_new_ids()
    while true do
        for idtype,info in pairs(block_datas) do
            skynet.error("creator going to check id space. idtype:", idtype)
            if info.ids_cnt < info.step/10 then
                skynet.error("creator start update id space. idtype:", idtype)
                -- 申请新的 ID 段
                try_init_generator(idtype)
                -- 更新生成器
                update_generator(idtype)
                skynet.error("creator update id space ok. idtype:", idtype)
            else
                skynet.error("not need create new ids. idtype:", idtype)
            end
        end
        is_busy = false
        skynet.wait()
    end
end

-- cfg: {
--   host = "127.0.0.1",
--   port = 27107,
--   username = nil,
--   password = nil,
--   authdb = nil,
--   dbname = "guid",
--   tblname = "guid",
--   idtypes = {uid = step, teamid = step},
-- }
function CMD.init(cfg)
    local db = mongo.client(cfg)
    local db_guid = db[cfg.dbname]
    tbl_guid = db_guid[cfg.tblname]
    -- 创建索引
    tbl_guid:createIndex({{idtype = 1}, unique = true})

    -- 初始化 ID 池子
    block_datas = {}
    for idtype,step in pairs(cfg.idtypes) do
        block_datas[idtype] = {
            blocks = {},
            step = step,
        }
        -- 首次申请 ID
        try_init_generator(idtype)
        -- 更新生成器
        update_generator(idtype)
    end

    -- 生产协程
    creator = skynet.fork(create_new_ids)
end

function CMD.get_guid(idtype)
    assert(block_datas[idtype], "Unknow idtype. idtype:" .. idtype)
    return get_new_id(idtype)
end

skynet.start(function()
	skynet.dispatch("lua", function(_, _, cmd, ...)
		local f = assert(CMD[cmd])
        skynet.ret(skynet.pack(f(...)))
	end)
end)

