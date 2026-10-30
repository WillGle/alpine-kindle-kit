-- Export only connection state and our IPv4; never account, peers or auth URLs.
if arg[1]=='--diagnostic' then
    local text=''
    for _,name in ipairs({'login.log','tailscaled.log'}) do
        local f=io.open('/tmp/gleworks/'..name,'rb')
        if f then
            local size=f:seek('end') or 0
            f:seek('set',math.max(0,size-16384))
            text=text..'\n'..(f:read(16384) or '')
            f:close()
        end
    end
    text=text:lower()
    -- Fixed categories only: never copy keys, enrollment links or account details.
    if text:find('x509',1,true) or text:find('certificate',1,true) then
        print('Last VPN attempt: TLS/certificate error; check device date and certificate trust.')
    elseif text:find('no such host',1,true) or text:find('name resolution',1,true) then
        print('Last VPN attempt: DNS lookup failed; check Wi-Fi/DNS.')
    elseif text:find('network is unreachable',1,true) or text:find('no route to host',1,true) then
        print('Last VPN attempt: network unavailable; check Wi-Fi.')
    elseif text:find('timeout',1,true) or text:find('timed out',1,true) then
        print('Last VPN attempt: timed out; connectivity/enrollment is not confirmed.')
    elseif text:find('permission denied',1,true) then
        print('Last VPN attempt: permission denied.')
    elseif text:find('%S') then
        print('Last VPN attempt: unclassified; private logs remain in /tmp/gleworks/.')
    else print('VPN diagnostic: no saved runtime log (logs reset after reboot).') end
    return
end
local json=require('rapidjson')
local f=assert(io.open(assert(arg[1]),'rb'))
local raw=f:read(1024*1024+1);f:close()
assert(#raw<=1024*1024,'Oversized VPN status')
local state=assert(json.decode(raw))
local backend=state.BackendState
if type(backend)=='string' and backend:match('^%a+$') then print('VPN connection: '..backend) end
for _,ip in ipairs(type(state.TailscaleIPs)=='table' and state.TailscaleIPs or {}) do
    if type(ip)=='string' and #ip<=15 and ip:match('^%d+%.%d+%.%d+%.%d+$') then print('VPN IPv4: '..ip) end
end
if backend=='NeedsLogin' then print('Open GleWorks VPN Login to enroll.')
elseif backend=='Stopped' then print('VPN is stopped; open GleWorks VPN to start.') end
