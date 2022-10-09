local mongo = require "skynet.db.mongo"
local config = require "config"
local guid = require "guid"
local log = require "log"
local util_table = require "util.table"

local M = {}

local account_tbl -- account 表操作对象

function M.init_db()
    local cfg = config.get_db_conf()
    local dbs = mongo.client(cfg)

    local db_name = "shiyanlou"
    local db = dbs[db_name]
    log.info("connect to db:", db_name)

    account_tbl = db.account

    -- 设两个唯一索引，一个账号对应一个角色
    account_tbl:createIndex({{acc = 1}, unique = true})
    account_tbl:createIndex({{uid = 1}, unique = true})
    -- 方便登录查询名字
    account_tbl:createIndex({{name = 1}, unique = false})

    -- guid 模块初始化
    cfg.dbname = config.get("guid_db_name")
    cfg.tblname = config.get("guid_tbl_name")
    cfg.idtypes = config.get_tbl("guid_idtypes")
    guid.init(cfg)
end

local function call_create_new_user(acc, init_data)
    -- 分配一个唯一的玩家ID
    local uid = guid.get_guid("uid")

    -- new user
    local user_data = {
        uid = uid,
        acc = acc,
    }
    -- 插入一个玩家数据
    local ok, msg, ret = account_tbl:safe_insert(user_data)
    if (ok and ret and ret.n == 1) then
        log.info("acc new uid succ. acc:", acc, "uid:", uid)
        return uid, user_data
    else
        return 0, "new user error:"..msg
    end
end

local function _call_load_user(acc)
    local ret = account_tbl:findOne({acc = acc})
    if not ret then
        return call_create_new_user(acc)
    else
        if not ret.uid then
            return 0, "cannot load user. acc:"..acc
        end
        return ret.uid, ret
    end
end

-- 根据 acc 查找玩家
local loading_user = {}
function M.find_and_create_user(acc)
    if loading_user[acc] then
        log.info("account is loading. acc:", acc)
        return 0, "already loading"
    end
    loading_user[acc] = true
    local ok, uid, data = xpcall(_call_load_user, debug.traceback, acc)
    loading_user[acc] = nil
    if not ok then
        local err = uid
        log.error("load user error. acc:", acc, " err:", err)
        return 0, err
    end
    return uid, data
end

-- 更新玩家名字
function M.update_username(uid, name)
    local data = {
        ["$set"] = {
            name = name,
        }
    }
    local _ok, ok, _, ret = xpcall(account_tbl.safe_update, debug.traceback, account_tbl, {uid = uid}, data, true, false)
    if not _ok or not (ok and ret and ret.n == 1) then
        log.error("update_username error. key:", key, _ok, ok, util_table.tostring(ret))
    end
end

-- 根据名字查找，忽略大小写
function M.find_by_name(name, limit)
    local query = {
        name = {
            ["$regex"] = name,
            ["$options"] = 'i',
        },
    }
    local selector = {
        ["_id"] = 0,
        ["uid"] = 1,
        ["name"] = 1,
    }
    local ret = account_tbl:find(query, selector):limit(limit)
    local ret_list = {}
    while ret:hasNext() do
        local data = ret:next()
        ret_list[data.uid] = data
    end
    return ret_list
end

return M
