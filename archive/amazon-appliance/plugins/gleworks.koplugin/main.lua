local WidgetContainer=require('ui/widget/container/widgetcontainer')
local UIManager=require('ui/uimanager')
local InfoMessage=require('ui/widget/infomessage')
local ButtonDialog=require('ui/widget/buttondialog')
local ConfirmBox=require('ui/widget/confirmbox')
local Menu=require('ui/widget/menu')
local MultiInputDialog=require('ui/widget/multiinputdialog')
local PluginLoader=require('pluginloader')
local Store=dofile('/mnt/us/koreader/plugins/gleworks.koplugin/store.lua')
local GleWorks=WidgetContainer:extend{name='gleworks',is_doc_only=false}
local consumed_start=false
local function record_failure(category)
    -- Constant metadata only; errors may contain private book/calendar text.
    pcall(os.execute,'/bin/sh /mnt/us/gleworks/scripts/record-failure.sh native-ui 1 '..category)
end
local function notify(text,kind)
    local screen=require('device').screen
    UIManager:show(InfoMessage:new{
        text='GleWorks '..(kind or 'INFO')..'\n\n'..text,show_icon=false,unmovable=true,honor_silent_mode=false,
        width=math.floor(screen:getWidth()*0.72),height=math.floor(screen:getHeight()*0.35),
    })
end
function GleWorks:safely(fn)
    local ok,err=pcall(fn)
    if not ok then record_failure('callback');notify('Không thực hiện được:\n'..tostring(err),'ERROR') end
end
function GleWorks:database()
    if not self.store then self.store=Store.open('/mnt/us/gleworks/data') end
    return self.store
end
function GleWorks:init()
    self.ui.menu:registerToMainMenu(self)
    if not consumed_start then
        local app=os.getenv('GLE_OPEN_APP');consumed_start=true
        if app=='planner' then UIManager:nextTick(function() self:calendar() end)
        elseif app=='terminal' then UIManager:nextTick(function() self:terminal() end) end
    end
end
function GleWorks:terminal()
    local terminal=PluginLoader:getPluginInstance('terminal')
    if not terminal then record_failure('terminal_missing');notify('Bật plugin Terminal emulator trong cài đặt plugin của KOReader, rồi khởi động lại KOReader.','ERROR');return end
    self:safely(function() terminal:onTerminalStart() end)
end
function GleWorks:edit(table_name,row,date,back)
    local fields={{description='Tiêu đề',text=row and row[2] or ''}}
    if table_name=='tasks' then
        fields[2]={description='Hạn YYYY-MM-DD (có thể trống)',text=row and row[3] or (date or '')}
    else
        fields[2]={description='Ngày YYYY-MM-DD',text=row and row[3] or date or os.date('%Y-%m-%d')}
        fields[3]={description='Giờ HH:MM (trống = cả ngày)',text=row and row[4] or ''}
    end
    local dialog
    dialog=MultiInputDialog:new{title=table_name=='tasks' and 'Việc cần làm' or 'Sự kiện',fields=fields,
        buttons={{{text='Hủy',callback=function() UIManager:close(dialog);back() end},
        {text='Lưu',callback=function()
            self:safely(function()
                local values=dialog:getFields();local db=self:database()
                if table_name=='tasks' then db:save_task(row and row[1],values[1],values[2])
                else db:save_event(row and row[1],values[1],values[2],values[3]) end
                UIManager:close(dialog);back()
            end)
        end}}}}
    UIManager:show(dialog);dialog:onShowKeyboard()
end
function GleWorks:actions(table_name,row,back)
    local dialog
    local buttons={}
    if table_name=='tasks' then
        buttons[#buttons+1]={{text=row[4]==1 and 'Đánh dấu chưa xong' or 'Hoàn thành',callback=function()
            self:safely(function() self:database():complete(row[1],row[4]~=1);UIManager:close(dialog);back() end)
        end}}
    end
    buttons[#buttons+1]={{text='Sửa',callback=function() UIManager:close(dialog);self:edit(table_name,row,nil,back) end}}
    buttons[#buttons+1]={{text='Xóa',callback=function()
        UIManager:show(ConfirmBox:new{text='Xóa mục này?',ok_callback=function()
            self:safely(function() self:database():delete(table_name,row[1]);UIManager:close(dialog);back() end)
        end})
    end}}
    buttons[#buttons+1]={{text='Đóng',callback=function() UIManager:close(dialog);back() end}}
    dialog=ButtonDialog:new{title=row[2],buttons=buttons};UIManager:show(dialog)
end
function GleWorks:list(date)
    self:safely(function()
        local db=self:database();local items={};local menu
        local function again() self:list(date) end
        local function close() UIManager:close(menu) end
        items[#items+1]={text='+ Thêm việc',callback=function() close();self:edit('tasks',nil,date,again) end}
        if date then items[#items+1]={text='+ Thêm sự kiện',callback=function() close();self:edit('events',nil,date,again) end} end
        if date then
            for _,entry in ipairs(db:events(date)) do
                local row=entry
                items[#items+1]={text=(row[4]~='' and row[4] or 'Cả ngày')..' · '..row[2],callback=function() close();self:actions('events',row,again) end}
            end
        end
        for _,entry in ipairs(db:tasks(date)) do
            local row=entry
            items[#items+1]={text=(row[4]==1 and '[x] ' or '[ ] ')..row[2]..(row[3]~='' and ' · '..row[3] or ''),callback=function() close();self:actions('tasks',row,again) end}
        end
        menu=Menu:new{title=date or 'Việc cần làm (tối đa 500 mục)',item_table=items}
        UIManager:show(menu)
    end)
end
function GleWorks:calendar(year,month)
    self:safely(function()
        local now=os.date('*t');year=year or now.year;month=month or now.month
        local marks={};for _,row in ipairs(self:database():month(year,month)) do marks[row[1]]=true end
        local dialog;local buttons={}
        local function change(delta)
            local y,m=year,month+delta
            if m<1 then y,m=y-1,12 elseif m>12 then y,m=y+1,1 end
            if y<1970 or y>2099 then return end
            UIManager:close(dialog);self:calendar(y,m)
        end
        buttons[1]={{text='<',callback=function() change(-1) end},{text='Hôm nay',callback=function() UIManager:close(dialog);self:calendar() end},{text='>',callback=function() change(1) end}}
        local weekdays={};for _,day in ipairs({'T2','T3','T4','T5','T6','T7','CN'}) do weekdays[#weekdays+1]={text=day,enabled=false} end
        buttons[2]=weekdays
        local first=Store.weekday(year,month,1);local total=Store.days(year,month)
        for week=0,math.ceil((first+total)/7)-1 do
            local row={}
            for column=0,6 do
                local day=week*7+column-first+1
                if day<1 or day>total then row[#row+1]={text=' ',enabled=false}
                else
                    local date=string.format('%04d-%02d-%02d',year,month,day)
                    local text=tostring(day)..(marks[date] and '*' or '')
                    if date==os.date('%Y-%m-%d') then text='['..text..']' end
                    row[#row+1]={text=text,callback=function() UIManager:close(dialog);self:list(date) end}
                end
            end
            buttons[#buttons+1]=row
        end
        buttons[#buttons+1]={{text='Việc cần làm',callback=function() UIManager:close(dialog);self:list() end},{text='Đóng',callback=function() UIManager:close(dialog) end}}
        dialog=ButtonDialog:new{title=string.format('Lịch %02d/%04d · * có lịch/việc',month,year),buttons=buttons};UIManager:show(dialog)
    end)
end
function GleWorks:updateApps()
    local f=io.open('/mnt/us/gleworks-updates/READY','rb')
    if not f then notify('Chưa có gói cập nhật. Chép gói đã xác minh qua USB rồi mở Update Apps. Cập nhật qua mạng sẽ có khi máy chủ GleWorks được triển khai.');return end
    f:close()
    UIManager:show(ConfirmBox:new{text='Đóng KOReader và cập nhật toàn bộ ứng dụng từ gói đã chép?',ok_callback=function()
        self:safely(function()
            local request=assert(io.open('/tmp/gleworks/update-request','wb'));assert(request:write('update\n'));assert(request:close())
            UIManager:broadcastEvent(require('ui/event'):new('Exit'))
        end)
    end})
end
function GleWorks:addToMainMenu(menu_items)
    menu_items.gleworks={text='GleWorks',sorting_hint='more_tools',sub_item_table={
        {text='Lịch',callback=function() self:calendar() end},
        {text='Việc cần làm',callback=function() self:list() end},
        {text='Terminal',callback=function() self:terminal() end},
        {text='Update Apps',callback=function() self:updateApps() end},
    }}
end
function GleWorks:onCloseWidget() if self.store then self.store:close();self.store=nil end end
GleWorks.onClose=GleWorks.onCloseWidget
return GleWorks
