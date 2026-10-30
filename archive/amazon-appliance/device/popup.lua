-- Fixed bounded panels. Payload files live only in the private /tmp directory.
local directory=assert(arg[1])
assert(directory:match('^/tmp/gleworks/popup%-%d+$'),'Unsafe popup directory')
local title=arg[2] or 'GleWorks'
local text=io.read(16385) or ''
assert(#text<=16384,'Oversized notification')
local width,rows=44,14
local function chars(value)
    local out={}
    for char in value:gmatch('[%z\1-\127\194-\244][\128-\191]*') do
        if char:byte()>=32 and char:byte()~=127 then out[#out+1]=char end
    end
    return out
end
local lines={}
text=text:gsub('\r\n','\n'):gsub('\r','\n')
for line in (text..'\n'):gmatch('(.-)\n') do
    local c=chars(line)
    if #c==0 then lines[#lines+1]='' end
    for i=1,#c,width do lines[#lines+1]=table.concat(c,'',i,math.min(#c,i+width-1)) end
end
local function padded(value)
    local c=chars(value);local out=table.concat(c,'',1,math.min(width,#c))
    return '| '..out..string.rep(' ',math.max(0,width-#c))..' |'
end
local pages=math.max(1,math.ceil(#lines/rows))
assert(pages<=32,'Too many notification pages')
for page=1,pages do
    local panel={'+-'..string.rep('-',width)..'-+',padded(title),'+-'..string.rep('-',width)..'-+'}
    for row=1,rows do panel[#panel+1]=padded(lines[(page-1)*rows+row] or '') end
    panel[#panel+1]='+-'..string.rep('-',width)..'-+'
    panel[#panel+1]=padded('GleWorks  |  '..page..'/'..pages)
    panel[#panel+1]='+-'..string.rep('-',width)..'-+'
    local f=assert(io.open(directory..'/'..page,'wb'));assert(f:write(table.concat(panel,'\n')));assert(f:close())
end
print(pages)
