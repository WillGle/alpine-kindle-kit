-- Serial, bounded HTTP transfers; no request-controlled execution or background polling.
local ffi = require('ffi')
local bit = require('bit')
require('ffi/posix_h')
local socket = require('socket')
local lfs = require('libs/libkoreader-lfs')
ffi.cdef[[
int openat(int, const char *, int, ...);
int unlinkat(int, const char *, int);
]]
local C = ffi.C
-- ARM32 Linux assigns these flags differently from x86_64.
assert(ffi.os=='Linux' and (ffi.arch=='arm' or ffi.arch=='x64'), 'Unsupported platform')
local NOFOLLOW = ffi.arch=='arm' and 32768 or 131072
local DIRECTORY = ffi.arch=='arm' and 16384 or 65536
local EXCL = 128
local base = assert(os.getenv('GLE_KINDLE_HOME'), 'GLE_KINDLE_HOME required')
local private = os.getenv('GLE_KINDLE_PRIVATE') or '/var/local/gleworks'
local port = 8080
local max_upload, reserve, chunk = 128*1024*1024, 64*1024*1024, 32768
local function die(message) error(message, 0) end
local function open_dir(path)
    local fd = C.open(path, bit.bor(DIRECTORY, NOFOLLOW, C.O_CLOEXEC))
    if fd < 0 then die('Cannot open directory: '..path) end
    return tonumber(fd)
end
local function open_leaf(dir, name, flags)
    return tonumber(C.openat(dir, name, bit.bor(flags, NOFOLLOW, C.O_CLOEXEC), ffi.cast('int', 384)))
end
local function random_token()
    local f = assert(io.open('/dev/urandom', 'rb'))
    local data = assert(f:read(32)); f:close()
    assert(#data == 32, 'Short random read')
    return (data:gsub('.', function(c) return string.format('%02x', c:byte()) end))
end
local function write_all(fd, data)
    local offset = 0
    while offset < #data do
        local n = tonumber(C.write(fd, data:sub(offset+1), #data-offset))
        if n <= 0 then die('File write failed') end
        offset = offset+n
    end
end
local private_fd = open_dir(private)
if arg[1] == '--init-token' then
    local fd = open_leaf(private_fd, 'http.token', bit.bor(C.O_WRONLY, C.O_CREAT, EXCL))
    if fd < 0 then die('Token already exists or cannot be created') end
    local ok, err = pcall(function() write_all(fd, random_token()..'\n'); assert(C.fsync(fd)==0) end)
    C.close(fd); C.close(private_fd)
    if not ok then die(err) end
    print('Private HTTP token created'); return
end
local token_fd = open_leaf(private_fd, 'http.token', C.O_RDONLY)
C.close(private_fd)
assert(token_fd >= 0, 'Run init first')
local token_buf = ffi.new('char[66]')
local token_n = tonumber(C.read(token_fd, token_buf, 66)); C.close(token_fd)
local token = ffi.string(token_buf, math.max(0, token_n)):match('^([0-9a-f]+)\n$')
assert(token and #token == 64, 'Invalid private token')
local books = os.getenv('GLE_KINDLE_BOOKS') or '/mnt/us/documents/GleWorks'
local roots = {books={path=books, fd=open_dir(books)}, files={path=base..'/files', fd=open_dir(base..'/files')}}
local assets = {}
for path, name in pairs({['/']='index.html', ['/app.js']='app.js'}) do
    local f = assert(io.open(base..'/web/'..name,'rb')); assets[path]=f:read('*a'); f:close()
    assert(#assets[path] < 65536, 'Oversized web asset')
end
if arg[1] == '--check' then
    for _, root in pairs(roots) do C.close(root.fd) end
    print('Runtime, token, roots and web assets loaded; network not started'); return
end
local function authenticated(value)
    value = value or ''
    if #value ~= 71 then return false end
    local diff = 0
    local expected = 'Bearer '..token
    for i=1,#expected do diff=bit.bor(diff, bit.bxor(value:byte(i), expected:byte(i))) end
    return diff==0
end
local function quote(value)
    return '"'..value:gsub('[%z\1-\31\\"]', function(c)
        return string.format('\\u%04x', c:byte())
    end)..'"'
end
local function utf8_valid(s)
    local i=1
    while i<=#s do
        local a=s:byte(i); local n, low, high=0,128,191
        if a<128 then n=1
        elseif a>=194 and a<=223 then n=2
        elseif a>=224 and a<=239 then n=3; if a==224 then low=160 elseif a==237 then high=159 end
        elseif a>=240 and a<=244 then n=4; if a==240 then low=144 elseif a==244 then high=143 end
        else return false end
        if i+n-1>#s then return false end
        if n>1 then
            local b=s:byte(i+1); if b<low or b>high then return false end
            for j=i+2,i+n-1 do local c=s:byte(j); if c<128 or c>191 then return false end end
        end
        i=i+n
    end
    return true
end
local function valid_name(name)
    if #name==0 or #name>200 or name:sub(1,1)=='.' or name:find('[%z\1-\31\127/\\:*?"<>|]')
        or name:find('[ .]$') or not utf8_valid(name) then return false end
    local stem = name:match('^([^.]*)'):upper()
    return not (stem=='CON' or stem=='PRN' or stem=='AUX' or stem=='NUL' or stem:match('^COM[1-9]$') or stem:match('^LPT[1-9]$'))
end
local function decode(s)
    if s:find('%%[^%x]') or s:find('%%$') or s:find('%%[%x]$') then return nil end
    return (s:gsub('%%(%x%x)',function(h) return string.char(tonumber(h,16)) end))
end
local reasons={[200]='OK',[201]='Created',[400]='Bad Request',[401]='Unauthorized',[403]='Forbidden',[404]='Not Found',[405]='Method Not Allowed',[408]='Request Timeout',[409]='Conflict',[413]='Content Too Large',[417]='Expectation Failed',[431]='Headers Too Large',[500]='Internal Server Error',[507]='Insufficient Storage'}
local function record_failure(code)
    -- Only an internal numeric status reaches this fixed recorder command.
    if not reasons[code] then code=500 end
    pcall(os.execute,'/bin/sh /mnt/us/gleworks/scripts/record-failure.sh transfer-api 1 http_'..code)
end
local function serve(client)
    local deadline=socket.gettime()+10
    local total=0
    local function line()
        local bytes={}
        while true do
            if socket.gettime()>deadline then error({408}) end
            local c=client:receive(1)
            if not c then error({408}) end
            total=total+1
            if total>16384 or #bytes>4096 then error({431}) end
            if c=='\n' then
                if bytes[#bytes]~='\r' then error({400}) end
                bytes[#bytes]=nil; return table.concat(bytes)
            end
            bytes[#bytes+1]=c
        end
    end
    local function send(data)
        local offset=1
        while offset<=#data do
            if socket.gettime()>deadline then die('Send timeout') end
            local n,err,last=client:send(data,offset)
            if not n then die('Send failed: '..tostring(err)) end
            offset=n+1
        end
    end
    local response_started=false
    local function head(code, length, content_type)
        response_started=true
        send('HTTP/1.1 '..code..' '..reasons[code]..'\r\nConnection: close\r\nContent-Length: '..length..'\r\nContent-Type: '..(content_type or 'text/plain; charset=utf-8')..'\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\nContent-Security-Policy: default-src \'none\'; script-src \'self\'; style-src \'unsafe-inline\'; connect-src \'self\'; base-uri \'none\'; frame-ancestors \'none\'; form-action \'self\'\r\n\r\n')
    end
    local function response(code, data, kind) data=data or reasons[code]..'\n'; head(code,#data,kind); send(data) end
    local fd, uploaded_root, uploaded_name
    local ok, failure=pcall(function()
        local method,path=line():match('^(%u+) (/[^ ]*) HTTP/1%.[01]$')
        if not method then error({400}) end
        local h={}
        for i=1,65 do
            local value=line(); if value=='' then break end
            if i==65 then error({431}) end
            local key,v=value:match('^([%w!#$%%&\'*+.^_`|~-]+):[ \t]*(.-)[ \t]*$')
            if not key or v:find('[%z\1-\8\10-\31\127]') then error({400}) end
            key=key:lower(); if h[key] then error({400}) end; h[key]=v
        end
        if not h.host or not h.host:match('^[%w.:%[%]%-]+$') then error({400}) end
        if h['transfer-encoding'] then error({400}) end
        if h.expect then error({417}) end
        if h.origin and h.origin~='http://'..h.host and h.origin~='https://'..h.host then error({403}) end
        local length=h['content-length']
        if length and (not length:match('^%d+$') or #length>10) then error({400}) end
        length=tonumber(length or '0')
        if method~='PUT' and length~=0 then error({400}) end
        deadline=socket.gettime()+300
        if assets[path] and method=='GET' then
            response(200,assets[path],path=='/' and 'text/html; charset=utf-8' or 'application/javascript'); return
        end
        if not authenticated(h.authorization) then error({401}) end
        local area, raw_name=path:match('^/api/(%a+)/(.*)$')
        if not area then area=path:match('^/api/(%a+)$') end
        local root=roots[area]; if not root then error({404}) end
        if not raw_name then
            if method~='GET' then error({405}) end
            local items={}
            for name in lfs.dir(root.path) do
                if valid_name(name) then
                    local a=lfs.symlinkattributes(root.path..'/'..name)
                    if a and a.mode=='file' then
                        if #items>=2000 then error({413}) end
                        items[#items+1]='{"name":'..quote(name)..',"size":'..string.format('%.0f',a.size)..'}'
                    end
                end
            end
            response(200,'['..table.concat(items,',')..']','application/json'); return
        end
        local name=decode(raw_name)
        if not name or not valid_name(name) then error({400}) end
        if method=='GET' then
            local a=lfs.symlinkattributes(root.path..'/'..name)
            if not a or a.mode~='file' then error({404}) end
            fd=open_leaf(root.fd,name,bit.bor(C.O_RDONLY,C.O_NONBLOCK))
            if fd<0 then error({404}) end
            local opened=lfs.attributes('/proc/self/fd/'..fd)
            if not opened or opened.mode~='file' then error({404}) end
            -- Obtain size from the opened descriptor, not a raced pathname.
            local size=tonumber(C.lseek(fd,0,2)); if size<0 or C.lseek(fd,0,0)<0 then error({404}) end
            head(200,size,'application/octet-stream')
            local buf=ffi.new('char[?]',chunk)
            while size>0 do
                local n=tonumber(C.read(fd,buf,math.min(chunk,size)))
                if n<=0 then die('File read failed') end
                send(ffi.string(buf,n)); size=size-n
            end
            C.close(fd); fd=nil
        elseif method=='PUT' then
            if not h['content-length'] then error({400}) end
            if length>max_upload then error({413}) end
            local space=ffi.new('struct statvfs[1]')
            if C.statvfs(root.path,space)~=0 or tonumber(space[0].f_bavail)*tonumber(space[0].f_frsize)<length+reserve then error({507}) end
            fd=open_leaf(root.fd,name,bit.bor(C.O_WRONLY,C.O_CREAT,EXCL))
            if fd<0 then error({409}) end
            uploaded_root,uploaded_name=root,name
            while length>0 do
                if socket.gettime()>deadline then error({408}) end
                local data=client:receive(math.min(chunk,length))
                if not data then error({408}) end
                write_all(fd,data); length=length-#data
            end
            if C.fsync(fd)~=0 then error({507}) end
            C.close(fd); fd=nil
            uploaded_root=nil; uploaded_name=nil
            response(201)
        else error({405}) end
    end)
    if fd then C.close(fd) end
    if uploaded_root then C.unlinkat(uploaded_root.fd,uploaded_name,0) end
    if not ok then
        -- Never log request headers, token, paths or file content.
        local code=type(failure)=='table' and failure[1] or 500
        if not reasons[code] then code=500 end
        record_failure(code)
        deadline=socket.gettime()+3
        if not response_started then pcall(response,code) end
    end
end
local listener=assert(socket.bind('127.0.0.1',port,4))
print('GleWorks HTTP listening on loopback port '..port)
while true do
    local client=listener:accept() -- Blocks while idle; no polling timer.
    if client then
        client:settimeout(10)
        local ok=pcall(serve,client)
        if not ok then record_failure(500) end
        client:close()
        collectgarbage('step',64)
    end
end
