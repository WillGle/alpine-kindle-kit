-- SPDX-License-Identifier: GPL-3.0-or-later
-- Export the KOReader sleep screen for the Gleworks OS lock screen.
--
-- The OS shell cannot decode a full-screen PNG inside its 256 KiB LVGL heap.
-- This plugin renders the chosen sleep screen into an 8 bpp Blitbuffer and
-- writes raw rows to /mnt/us/gleworks/lockscreen.raw.  The shell then applies
-- that buffer through its normal EPDC path.

if os.getenv("GW_MANAGED_READER") ~= "1" then return { disabled = true } end

local WidgetContainer = require("ui/widget/container/widgetcontainer")
local UIManager = require("ui/uimanager")
local Notification = require("ui/widget/notification")
local InfoMessage = require("ui/widget/infomessage")
local InputDialog = require("ui/widget/inputdialog")
local ImageWidget = require("ui/widget/imagewidget")
local BookStatusWidget = require("ui/widget/bookstatuswidget")
local Blitbuffer = require("ffi/blitbuffer")
local util = require("util")
local _ = require("gettext")
local ffi = require("ffi")

local CONF = "/mnt/us/gleworks/lockscreen.conf"
local RAW = "/mnt/us/gleworks/lockscreen.raw"
local WIDTH = 1072
local HEIGHT = 1448

local GwLock = WidgetContainer:extend{
    name = "gw_lockscreen",
    is_doc_only = false,
}

local function read_conf()
    local t = {
        use_image = false,
        show_clock = true,
        show_date = true,
        message = "",
    }
    local file = io.open(CONF, "r")
    if not file then return t end
    for line in file:lines() do
        local key, value = line:match("^([%w_]+)=(.*)$")
        if key == "use_image" then
            t.use_image = value ~= "0"
        elseif key == "show_clock" then
            t.show_clock = value ~= "0"
        elseif key == "show_date" then
            t.show_date = value ~= "0"
        elseif key == "message" then
            t.message = value
        end
    end
    file:close()
    return t
end

local function write_conf(t)
    util.makePath("/mnt/us/gleworks")
    local tmp = CONF .. ".tmp"
    local file = io.open(tmp, "w")
    if not file then return false end
    file:write("version=1\n")
    file:write("use_image=" .. (t.use_image and "1" or "0") .. "\n")
    file:write("image=" .. RAW .. "\n")
    file:write("show_clock=" .. (t.show_clock and "1" or "0") .. "\n")
    file:write("show_date=" .. (t.show_date and "1" or "0") .. "\n")
    file:write("message=" .. (t.message or "") .. "\n")
    file:close()
    os.rename(tmp, CONF)
    return true
end

local function write_raw(bb)
    local tmp = RAW .. ".tmp"
    local file = io.open(tmp, "wb")
    if not file then return false end
    local stride = tonumber(bb.stride)
    for y = 0, bb.h - 1 do
        file:write(ffi.string(bb.data + y * stride, bb.w))
    end
    file:close()
    os.rename(tmp, RAW)
    return true
end

function GwLock:init()
    if self.ui.menu then
        self.ui.menu:registerToMainMenu(self)
    end
end

function GwLock:build_widget()
    local Screensaver = require("ui/screensaver")
    Screensaver:setup()
    local mode = Screensaver.screensaver_type

    if mode == "cover" or mode == "random_image" then
        local settings = {
            width = WIDTH,
            height = HEIGHT,
            scale_factor = G_reader_settings:isFalse("screensaver_stretch_images") and 0 or nil,
            stretch_limit_percentage = G_reader_settings:readSetting("screensaver_stretch_limit_percentage"),
        }
        if Screensaver.image then
            settings.image = Screensaver.image
            settings.image_disposable = true
        elseif Screensaver.image_file then
            settings.file = Screensaver.image_file
            settings.file_do_cache = false
            settings.alpha = true
        else
            return nil, Screensaver
        end
        return ImageWidget:new(settings), Screensaver
    elseif mode == "bookstatus" then
        return BookStatusWidget:new{ ui = self.ui, readonly = true }, Screensaver
    elseif mode == "readingprogress" and self.ui.statistics then
        return self.ui.statistics:onShowReaderProgress(true), Screensaver
    end
    return nil, Screensaver
end

function GwLock:export()
    local widget, saver = self:build_widget()
    if not widget then
        Notification:notify(_("This sleep screen mode has no image; using text only"))
        return self:text_only()
    end

    local bb = Blitbuffer.new(WIDTH, HEIGHT, Blitbuffer.TYPE_BB8)
    if saver.screensaver_background == "black" then
        bb:fill(Blitbuffer.COLOR_BLACK)
    else
        bb:fill(Blitbuffer.COLOR_WHITE)
    end
    widget:paintTo(bb, 0, 0)
    if widget.free then widget:free() end

    local ok = write_raw(bb)
    bb:free()
    if not ok then
        UIManager:show(InfoMessage:new{
            text = _("Could not write the lock screen image."),
        })
        return
    end

    local t = read_conf()
    t.use_image = true
    write_conf(t)
    Notification:notify(_("Lock screen updated"))
end

function GwLock:text_only()
    local t = read_conf()
    t.use_image = false
    write_conf(t)
    os.remove(RAW)
    Notification:notify(_("Lock screen set to text only"))
end

function GwLock:set_message()
    local t = read_conf()
    local dialog
    dialog = InputDialog:new{
        title = _("Lock screen message"),
        input = t.message,
        buttons = {{
            {
                text = _("Cancel"),
                callback = function() UIManager:close(dialog) end,
            },
            {
                text = _("Save"),
                is_enter_default = true,
                callback = function()
                    t.message = dialog:getInputText()
                    write_conf(t)
                    UIManager:close(dialog)
                    Notification:notify(_("Lock screen message saved"))
                end,
            },
        }},
    }
    UIManager:show(dialog)
    dialog:onShowKeyboard()
end

function GwLock:toggle(key)
    local t = read_conf()
    t[key] = not t[key]
    write_conf(t)
end

function GwLock:addToMainMenu(menu_items)
    menu_items.gw_lockscreen = {
        text = _("Gleworks lock screen"),
        sub_item_table = {
            {
                text = _("Use current sleep screen"),
                callback = function() self:export() end,
            },
            {
                text = _("Show clock"),
                checked_func = function() return read_conf().show_clock end,
                callback = function() self:toggle("show_clock") end,
            },
            {
                text = _("Show date"),
                checked_func = function() return read_conf().show_date end,
                callback = function() self:toggle("show_date") end,
            },
            {
                text = _("Set message..."),
                callback = function() self:set_message() end,
            },
            {
                text = _("Text only"),
                callback = function() self:text_only() end,
            },
        },
    }
end

return GwLock
