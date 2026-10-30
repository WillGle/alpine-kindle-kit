-- KOReader on native Alpine: kernel devices replace Amazon service integration.
require("setupkoenv")
local Device = require("device/kindle/device")
local PowerD = require("device/kindle/powerd")
local ffiUtil = require("ffi/util")
local function read_file(path)
    local file = io.open(path, "r")
    if not file then return nil end
    local value = file:read("*all")
    file:close()
    return value
end
local yes = function() return true end
local no = function() return false end

Device.isSpecialOffers = false
Device.isStartupScriptUpToDate = yes
Device.canSuspend = no
Device.canReboot = yes
Device.canPowerOff = no
Device.hasOTAUpdates = no
Device.hasWifiToggle = no
Device.hasWifiManager = no
Device.hasWifiRestore = no
Device.canTurnFrontlightOff = yes
Device.home_dir = "/mnt/us/documents"

function Device:openInputDevices()
    -- The exact PW3 touchscreen is event1 after the on-key device.
    self.input:open("/dev/input/event1")
end

function Device:reboot()
    os.execute("/bin/busybox kill -QUIT 1")
end

function Device:initNetworkManager(manager)
    manager.isWifiOn = no
    manager.isConnected = no
end

-- Keep the familiar 0..24 slider while driving the hardware directly.
function PowerD:frontlightIntensityHW()
    local raw = tonumber(read_file("/sys/class/backlight/max77696-bl/brightness")) or 0
    return math.floor(math.sqrt(raw / 4095) * 24 + 0.5)
end

function PowerD:setIntensityHW(value)
    value = math.max(0, math.min(24, value))
    local raw = math.floor((value / 24)^2 * 4095 + 0.5)
    ffiUtil.writeToSysfs(raw, "/sys/class/backlight/max77696-bl/brightness")
    self:_decideFrontlightState()
end

local init = Device.init
function Device:init()
    init(self)
    self.powerd.batt_capacity_file = "/sys/class/power_supply/max77696-battery/capacity"
    self.powerd.isChargingHW = function()
        local status = read_file("/sys/class/power_supply/max77696-battery/status") or ""
        return status:find("Charging", 1, true) ~= nil
    end
end

dofile("reader.lua")
