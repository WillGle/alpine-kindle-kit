-- Validate the entire staged tree before any staged program is executed.
local lfs=require('libs/libkoreader-lfs')
local json=require('rapidjson')
local stage='/mnt/us/gleworks-updates'
local function read(path,limit)
    local f=assert(io.open(path,'rb'));local data=f:read(limit+1) or '';f:close();assert(#data<=limit,'Oversized update metadata');return data
end
assert(read(stage..'/READY',128)=='GleWorks Kindle bundle v1\n','Incomplete update')
local manifest=read(stage..'/SHA256SUMS',512*1024)
local files={READY=true,SHA256SUMS=true};local hashes={};local directories={};local count=0
for line in manifest:gmatch('[^\n]+') do
    local hash,name=line:match('^([0-9a-f]+)  (.+)$')
    assert(hash and #hash==64 and not files[name],'Invalid or duplicate checksum entry')
    assert(not name:find('[%z\1-\31\127\\]') and name:sub(1,1)~='/' and not name:find('//',1,true) and name:sub(-1)~='/','Unsafe update name')
    for part in name:gmatch('[^/]+') do assert(part~='.' and part~='..','Unsafe path component') end
    assert(name:match('^koreader/') or name:match('^gleworks/') or name=='gleworks-install.sh' or name=='gleworks-rollback.sh','Unmanaged update path')
    assert(not name:match('^gleworks/data[/]?') and not name:match('^gleworks/files[/]?'),'Personal data cannot be in update package')
    files[name]=true;hashes[name]=hash;count=count+1
    local parent=name:match('^(.*)/[^/]+$')
    while parent do directories[parent]=true;parent=parent:match('^(.*)/[^/]+$') end
end
assert(count>0 and count<30000,'Invalid update file count')
local function walk(path,relative)
    assert(lfs.symlinkattributes(path,'mode')=='directory','Update contains linked directory')
    for name in lfs.dir(path) do
        if name~='.' and name~='..' then
            local child=relative=='' and name or relative..'/'..name
            local full=path..'/'..name;local mode=lfs.symlinkattributes(full,'mode')
            if mode=='directory' then assert(directories[child],'Unmanaged update directory: '..child);walk(full,child)
            else assert(mode=='file' and files[child],'Unverified or linked update member: '..child);files[child]=nil end
        end
    end
end
walk(stage,'');assert(next(files)==nil,'Missing update files')
for _,name in ipairs({
    'koreader/luajit',
    'koreader/reader.lua',
    'koreader/koreader.sh',
    'koreader/setupkoenv.lua',
    'gleworks/bin/tailscale',
    'gleworks/bin/tailscaled',
    'gleworks/scripts/kindle-services.sh',
    'gleworks/scripts/apply-bundle.sh',
    'gleworks/scripts/show-message.sh',
    'gleworks/scripts/record-failure.sh',
    'gleworks/device/popup.lua',
    'gleworks/scripts/rollback.sh',
    'gleworks/scripts/gleworks-inspect.sh',
    'gleworks-install.sh',
    'gleworks/UPSTREAM.json',
    'koreader/plugins/gleworks.koplugin/_meta.lua',
    'koreader/plugins/gleworks.koplugin/main.lua',
    'koreader/plugins/gleworks.koplugin/store.lua',
    'gleworks/device/appliance-health.lua',
    'gleworks/device/package-records.lua',
    'gleworks/device/reader-profile.lua',
    'gleworks/device/server.lua',
    'gleworks/device/validate-update.lua',
    'gleworks/device/vpn-state.lua',
    'gleworks/web/app.js',
    'gleworks/web/index.html',
    'gleworks/scriptlets/gleworks-key.sh',
    'gleworks/scriptlets/gleworks-planner.sh',
    'gleworks/scriptlets/gleworks-reader.sh',
    'gleworks/scriptlets/gleworks-restore-apps.sh',
    'gleworks/scriptlets/gleworks-status.sh',
    'gleworks/scriptlets/gleworks-terminal.sh',
    'gleworks/scriptlets/gleworks-transfer.sh',
    'gleworks/scriptlets/gleworks-update.sh',
    'gleworks/scriptlets/gleworks-vpn-login.sh',
    'gleworks/scriptlets/gleworks-vpn.sh',
}) do
    assert(hashes[name],'Missing required app: '..name)
end
if arg[1]=='--record' then
    local document_hashes={}
    for name,hash in pairs(hashes) do
        local script=name:match('^gleworks/scriptlets/(gleworks%-[^/]+%.sh)$')
        if script then document_hashes['documents/'..script]=hash end
    end
    document_hashes['documents/gleworks-rollback.sh']=hashes['gleworks/scripts/rollback.sh']
    document_hashes['documents/gleworks-inspect.sh']=hashes['gleworks/scripts/gleworks-inspect.sh']
    document_hashes['documents/gleworks-install.sh']=hashes['gleworks-install.sh']
    for name,hash in pairs(document_hashes) do hashes[name]=hash end
    local record={patch='appliance-v1',files=hashes,runtime_acceptance='pending'}
    local f=assert(io.open('/tmp/gleworks/update-release.json','wb'));assert(f:write(json.encode(record)));assert(f:close())
end
print('Staged update tree validated: '..count..' files')
