-- Bounded, read-only checks. No book names, account data, keys or login URLs.
local json=require('rapidjson')
local socket=require('socket')
local base=assert(os.getenv('GLE_KINDLE_HOME'))
local private=assert(os.getenv('GLE_KINDLE_PRIVATE'))
local function failed(category)
    pcall(os.execute,'/bin/sh /mnt/us/gleworks/scripts/record-failure.sh status 1 '..category)
end
local function read(path,limit)
    local f=io.open(path,'rb');if not f then return nil end
    local value=f:read(limit+1) or '';f:close()
    if #value>limit then return nil end
    return value
end
local raw=read(base..'/UPSTREAM.json',8192)
if raw then
    local ok,upstream=pcall(json.decode,raw)
    if ok and type(upstream)=='table' then
        for _,name in ipairs({'koreader','tailscale'}) do
            local item=upstream[name]
            local version=type(item)=='table' and (item.version or item.tag)
            if type(version)=='string' and version:match('^[%w.%-]+$') then print(name..': '..version) end
        end
    end
end
local settings=read('/mnt/us/koreader/settings.reader.lua',1024*1024)
if settings then
    local day=settings:match('%["full_refresh_count"%]%s*=%s*(%d+)')
    local night=settings:match('%["night_full_refresh_count"%]%s*=%s*(%d+)')
    if day and night then print('Full refresh (day/night): '..day..'/'..night..' pages') end
end
local token=read(private..'/http.token',66)
token=token and token:match('^([0-9a-f]+)\n$')
if not token or #token~=64 then failed('credential');print('Transfer credential: missing or invalid');return end
print('Transfer credential: present')
if arg[1]~='running' then print('Transfer API: not checked while stopped');return end
local client=assert(socket.tcp())
local deadline=socket.gettime()+2
local function remaining()
    local seconds=deadline-socket.gettime()
    if seconds<=0 then return false end
    client:settimeout(seconds)
    return true
end
remaining()
local ok=client:connect('127.0.0.1',8080)
if not ok then client:close();failed('http_connect');print('Transfer API: loopback connection failed');return end
local sent=remaining() and client:send('GET /api/books HTTP/1.1\r\nHost: 127.0.0.1:8080\r\nAuthorization: Bearer '..token..'\r\nConnection: close\r\n\r\n')
local line
-- Bound the status line instead of accepting an unbounded line from another listener.
if sent then
    local chars={}
    for _=1,128 do
        if not remaining() then break end
        local c=client:receive(1);if not c or c=='\n' then break end
        chars[#chars+1]=c
    end
    line=table.concat(chars)
end
client:close()
local code=line and line:match('^HTTP/1%.[01] (%d%d%d) ')
if code=='200' then print('Transfer API: authenticated request passed')
elseif code then failed('http_'..code);print('Transfer API: HTTP '..code)
else failed('http_timeout');print('Transfer API: invalid response or timeout') end
