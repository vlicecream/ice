local lru = {}
local lru_mt = { __index = lru }

local function addnode(self, node)
    local head = self._head
    node._next = head
    if head then
        head._pre = node
    end
    self._head = node
    if self._tail == nil then
        self._tail = node
    end

    local key = node.key
    self.map[key] = node
end

local function delnode(self, node)
    local head = self._head
    if node == head then
        self._head = node._next
    end

    local tail = self._tail
    if node == tail then
        self._tail = node._pre
    end

    local pre = node._pre
    local next = node._next
    if pre then
        pre._next = next
    end
    if next then
        next._pre = pre
    end

    local key = node.key
    self.map[key] = nil
    node._pre = nil
    node._next = nil

    local fun = self.remove_cb
    if fun then
        fun(key, node.value)
    end
end

local function tohead(self, node)
    delnode(self, node)
    addnode(self, node)
end

function lru.new(size, remove_cb)
    local self = setmetatable({}, lru_mt)
    self.map = {}
    self.size = size
    self.count = 0
    self.remove_cb = remove_cb
    return self
end

function lru.get(self, key)
    local node = self.map[key]
    if node == nil then
        return
    end

    tohead(self, node)

    return self.map[key].value
end

function lru.set(self, key, value, force)
    local node = self.map[key]
    if node then
        node.value = value
        tohead(self, node)
        return
    end

    local newnode = {
        key = key,
        value = value,
    }
    addnode(self, newnode)

    self.count = self.count + 1
    if self.count > self.size and not force then
        delnode(self, self._tail)
        self.count = self.count - 1
    end
end

function lru.dump(self)
    local node = self._head
    while node do
        print(node.key, node.value)
        node = node._next
    end
end

return lru
