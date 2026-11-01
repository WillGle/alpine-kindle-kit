if os.getenv("GW_MANAGED_READER") ~= "1" then return { disabled = true } end

local WidgetContainer = require("ui/widget/container/widgetcontainer")
local UIManager = require("ui/uimanager")
local Home = WidgetContainer:extend{ name = "gw_home", is_doc_only = false }

function Home:init()
    self.ui.menu:registerToMainMenu(self)
    self.lock_poll = function()
        local pipe = io.popen("/usr/sbin/gwctl status 2>/dev/null", "r")
        local status = pipe and pipe:read("*l")
        if pipe then pipe:close() end
        if status and status:find(" lock=locked", 1, true) then
            -- A power-button lock must hand the display to the OS lock screen,
            -- not restart KOReader's own UI and leave the panel dark.
            self.lock_exiting = true
            self.ui.menu:exitOrRestart(function() UIManager:quit() end)
            return
        end
        UIManager:scheduleIn(2, self.lock_poll)
    end
    UIManager:scheduleIn(2, self.lock_poll)
end

function Home:onCloseWidget()
    if self.lock_poll then UIManager:unschedule(self.lock_poll) end
end

function Home:addToMainMenu(menu_items)
    -- KOReader's built-in Exit closes the reader to its own file manager.
    -- Close through its normal menu path, then quit for the system shell.
    menu_items.exit = {
        text = "Return to Home",
        callback = function()
            self.ui.menu:exitOrRestart(function()
                UIManager:quit()
            end)
        end,
    }
    menu_items.restart_koreader = nil
end

return Home
