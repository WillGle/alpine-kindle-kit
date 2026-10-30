-- Retain unrelated preferences; backup is kept on USB without account secrets.
local lfs = require('libs/libkoreader-lfs')
local path='settings.reader.lua'
local attr=lfs.symlinkattributes(path)
assert(not attr or attr.mode=='file', 'Unsafe settings file')
local backup=path..'.gleworks-before-balanced'
assert(not lfs.symlinkattributes(backup), 'Profile already applied; restore the backup before reapplying')
local settings={}
if attr then
    assert(attr.size and attr.size<1024*1024,'Oversized settings file')
    local f=assert(io.open(path,'rb')); local original=assert(f:read('*a')); f:close()
    -- Settings are data. Do not allow imported preferences to execute Lua APIs.
    local chunk=assert(load(original,'settings.reader.lua','t',{}))
    jit.off(chunk,true)
    local instructions=0
    debug.sethook(function() instructions=instructions+1000; assert(instructions<=500000,'Settings execution limit exceeded') end,'',1000)
    local ok,result=pcall(chunk)
    debug.sethook()
    assert(ok,result); settings=result
    assert(type(settings)=='table','Invalid settings')
    local seen,count={},0
    local function data_only(value,depth)
        local kind=type(value)
        assert(depth<24,'Settings nesting too deep')
        if kind=='table' then
            assert(not seen[value],'Cyclic settings'); seen[value]=true
            for k,v in pairs(value) do
                count=count+1; assert(count<=20000,'Too many settings')
                assert(type(k)=='string' or type(k)=='number','Invalid settings key')
                data_only(k,depth+1); data_only(v,depth+1)
            end
            seen[value]=nil
        elseif kind=='number' then assert(value==value and value~=math.huge and value~=-math.huge,'Invalid settings number')
        elseif kind=='string' then assert(#value<1024*1024,'Oversized settings value')
        else assert(kind=='boolean','Settings must contain only data') end
    end
    data_only(settings,0)
    local b=assert(io.open(backup,'wb')); assert(b:write(original)); assert(b:close())
else
    local b=assert(io.open(backup,'wb')); assert(b:write('return {}\n')); assert(b:close())
end
local LuaSettings=require('luasettings')
local s=LuaSettings:wrap(settings)
s.file=path
s:saveSetting('full_refresh_count',4)
s:saveSetting('night_full_refresh_count',4)
s:saveSetting('swipe_animations',false)
s:saveSetting('disable_double_tap',true)
s:flush()
local check=assert(loadfile(path)); setfenv(check,{})
local applied=check()
assert(applied.full_refresh_count==4 and applied.night_full_refresh_count==4 and applied.swipe_animations==false and applied.disable_double_tap==true,'Profile write verification failed')
print('Balanced profile applied: full refresh every 4 pages; animations off; double tap disabled.')
print('Restore settings.reader.lua.gleworks-before-balanced with KOReader closed to undo.')
