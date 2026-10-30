-- Personal data stays outside release directories; SQL values are always bound.
local sql=require('lua-ljsqlite3/init')
local lfs=require('libs/libkoreader-lfs')
local Store={}
function Store.days(year,month)
    assert(year>=1970 and year<=2099 and month>=1 and month<=12,'Ngày ngoài khoảng hỗ trợ')
    local days=({31,28,31,30,31,30,31,31,30,31,30,31})[month]
    if month==2 and year%4==0 and (year%100~=0 or year%400==0) then days=29 end
    return days
end
function Store.date(value,optional)
    if optional and value=='' then return value end
    assert(type(value)=='string','Ngày không hợp lệ')
    local y,m,d=value:match('^(%d%d%d%d)%-(%d%d)%-(%d%d)$')
    y,m,d=tonumber(y),tonumber(m),tonumber(d)
    assert(y and m and d and y>=1970 and y<=2099 and m>=1 and m<=12 and d>=1 and d<=Store.days(y,m),'Dùng ngày YYYY-MM-DD hợp lệ')
    return value
end
function Store.weekday(year,month,day)
    local offsets={0,3,2,5,0,3,5,1,4,6,2,4}
    if month<3 then year=year-1 end
    local sunday=(year+math.floor(year/4)-math.floor(year/100)+math.floor(year/400)+offsets[month]+day)%7
    return (sunday+6)%7 -- Monday = 0, independent of 32-bit time_t.
end
local function title(value)
    assert(type(value)=='string','Tiêu đề không hợp lệ')
    value=value:match('^%s*(.-)%s*$')
    assert(#value>0 and #value<=256 and not value:find('[%z\1-\31\127]'),'Tiêu đề cần 1–256 byte, trên một dòng')
    return value
end
local function kind(value) assert(value=='tasks' or value=='events','Loại mục không hợp lệ');return value end
local function id(value) assert(type(value)=='number' and value>0 and value%1==0,'ID không hợp lệ');return value end
function Store.open(directory)
    assert(not lfs.symlinkattributes(directory) or lfs.symlinkattributes(directory,'mode')=='directory','Thư mục dữ liệu không hợp lệ')
    if not lfs.attributes(directory) then assert(lfs.mkdir(directory)) end
    local path=directory..'/planner.sqlite3'
    assert(not lfs.symlinkattributes(path) or lfs.symlinkattributes(path,'mode')=='file','Tệp dữ liệu không hợp lệ')
    local db=sql.open(path,'rwc');db:set_busy_timeout(1000)
    db:exec([[PRAGMA journal_mode=DELETE; PRAGMA synchronous=FULL;
        CREATE TABLE IF NOT EXISTS tasks(id INTEGER PRIMARY KEY,title TEXT NOT NULL,due TEXT NOT NULL DEFAULT '',done INTEGER NOT NULL DEFAULT 0 CHECK(done IN (0,1)));
        CREATE TABLE IF NOT EXISTS events(id INTEGER PRIMARY KEY,title TEXT NOT NULL,date TEXT NOT NULL,time TEXT NOT NULL DEFAULT '');
        CREATE INDEX IF NOT EXISTS events_date ON events(date);
        CREATE INDEX IF NOT EXISTS tasks_due ON tasks(due)]])
    return setmetatable({db=db},{__index=Store})
end
function Store:statement(query,values,rows)
    local stmt=self.db:prepare(query)
    local ok,result=pcall(function()
        for i,value in ipairs(values or {}) do stmt:bind1(i,value) end
        if not rows then stmt:step();return end
        local out={}
        while true do local row=stmt:step();if not row then break end
            -- SQLite returns int64 cdata; UI callbacks use ordinary numeric IDs.
            for i,value in ipairs(row) do if type(value)=='cdata' then row[i]=assert(tonumber(value)) end end
            out[#out+1]=row end
        return out
    end)
    stmt:close();assert(ok,result);return result
end
function Store:save_task(task_id,text,due)
    text=title(text);due=Store.date(due or '',true)
    if task_id then self:statement('UPDATE tasks SET title=?,due=? WHERE id=?',{text,due,id(task_id)})
    else
        assert(self.db:rowexec('SELECT count(*) FROM tasks')<2000,'Đã đạt giới hạn 2.000 việc')
        self:statement('INSERT INTO tasks(title,due) VALUES(?,?)',{text,due})
    end
end
function Store:save_event(event_id,text,date,time)
    text=title(text);date=Store.date(date);time=time or ''
    local h,m=time:match('^(%d%d):(%d%d)$')
    assert(time=='' or (h and tonumber(h)<24 and tonumber(m)<60),'Dùng giờ HH:MM hoặc để trống')
    if event_id then self:statement('UPDATE events SET title=?,date=?,time=? WHERE id=?',{text,date,time,id(event_id)})
    else
        assert(self.db:rowexec('SELECT count(*) FROM events')<2000,'Đã đạt giới hạn 2.000 sự kiện')
        self:statement('INSERT INTO events(title,date,time) VALUES(?,?,?)',{text,date,time})
    end
end
function Store:tasks(date)
    if date then return self:statement('SELECT id,title,due,done FROM tasks WHERE due=? ORDER BY done,id LIMIT 500',{Store.date(date)},true) end
    return self:statement("SELECT id,title,due,done FROM tasks ORDER BY done,(due=''),due,id DESC LIMIT 500",{},true)
end
function Store:events(date)
    return self:statement('SELECT id,title,date,time FROM events WHERE date=? ORDER BY time,id LIMIT 500',{Store.date(date)},true)
end
function Store:month(year,month)
    local first=string.format('%04d-%02d-01',year,month)
    local last=string.format('%04d-%02d-%02d',year,month,Store.days(year,month))
    return self:statement([[SELECT date,count(*) FROM (
        SELECT date FROM events WHERE date>=? AND date<=?
        UNION ALL SELECT due AS date FROM tasks WHERE due>=? AND due<=? AND done=0
        ) GROUP BY date]],{first,last,first,last},true)
end
function Store:complete(task_id,done) self:statement('UPDATE tasks SET done=? WHERE id=?',{done and 1 or 0,id(task_id)}) end
function Store:delete(table_name,item_id) self:statement('DELETE FROM '..kind(table_name)..' WHERE id=?',{id(item_id)}) end
function Store:close() self.db:close() end
return Store
