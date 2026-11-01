-- KOReader on native Alpine: kernel devices replace Amazon service integration.
require("setupkoenv")
local Device = require("device/kindle/device")
local PowerD = require("device/kindle/powerd")
local ffiUtil = require("ffi/util")
local managed = os.getenv("GW_MANAGED_READER") == "1"
local last_light, last_battery, last_charging = 0, 0, false
local managed_light_available = false

-- Managed mode has no direct-hardware fallback if the system service fails.
local function system_query(command)
    local pipe = io.popen("/usr/sbin/gwctl " .. command .. " 2>/dev/null", "r")
    if not pipe then return nil end
    local response = pipe:read("*l")
    local ok = pipe:close()
    if not ok then return nil end
    return response
end
if managed then
    local status = system_query("status")
    local caps = system_query("caps")
    local battery = status and tonumber(status:match(" battery=(%-?%d+)"))
    local power = status and status:match(" power=(%a+)")
    local percent = status and tonumber(status:match(" light=(%-?%d+)"))
    assert(battery and battery >= 0 and battery <= 100 and
           (power == "charging" or power == "battery") and caps,
           "Managed reader requires system-service status")
    managed_light_available = caps:find(" light-set=1", 1, true) ~= nil
    if managed_light_available then
        assert(percent and percent >= 0 and percent <= 100, "Managed frontlight state is unavailable")
    end
    last_battery = battery
    last_charging = power == "charging"
    if percent and percent >= 0 and percent <= 100 then
        last_light = math.floor(math.sqrt(percent / 100) * 24 + 0.5)
    end
end
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
Device.canReboot = managed and no or yes
if managed then Device.canRestart = no end
Device.canPowerOff = no
Device.hasOTAUpdates = no
Device.hasWifiToggle = no
Device.hasWifiManager = no
Device.hasWifiRestore = no
Device.canTurnFrontlightOff = yes
if managed and not managed_light_available then Device.hasFrontlight = no end
Device.home_dir = "/mnt/us/documents"

function Device:openInputDevices()
    -- The exact PW3 touchscreen is event1 after the on-key device.
    self.input:open("/dev/input/event1")
end

function Device:reboot()
    if not managed then os.execute("/bin/busybox kill -QUIT 1") end
end

function Device:initNetworkManager(manager)
    manager.isWifiOn = no
    manager.isConnected = no
end

-- Keep the familiar 0..24 slider while driving the hardware directly.
function PowerD:frontlightIntensityHW()
    if managed then
        local response = system_query("light-get")
        local percent = response and tonumber(response:match("^LIGHT (%d+)$"))
        if percent and percent <= 100 then last_light = math.floor(math.sqrt(percent / 100) * 24 + 0.5) end
        return last_light
    end
    local raw = tonumber(read_file("/sys/class/backlight/max77696-bl/brightness")) or 0
    return math.floor(math.sqrt(raw / 4095) * 24 + 0.5)
end

function PowerD:setIntensityHW(value)
    value = math.max(0, math.min(24, value))
    if managed then
        local percent = math.floor((value / 24)^2 * 100 + 0.5)
        if system_query("light-set " .. percent) == "OK" then last_light = value end
        self:_decideFrontlightState()
        return
    end
    local raw = math.floor((value / 24)^2 * 4095 + 0.5)
    ffiUtil.writeToSysfs(raw, "/sys/class/backlight/max77696-bl/brightness")
    self:_decideFrontlightState()
end

if managed then
    function PowerD:getCapacityHW()
        local status = system_query("status")
        local capacity = status and tonumber(status:match(" battery=(%-?%d+)"))
        if capacity and capacity >= 0 and capacity <= 100 then last_battery = capacity end
        return last_battery
    end
    function PowerD:isChargingHW()
        local status = system_query("status")
        local power = status and status:match(" power=(%a+)")
        if power == "charging" then last_charging = true
        elseif power == "battery" then last_charging = false end
        return last_charging
    end
    PowerD.isChargedHW = no
end

local init = Device.init
function Device:init()
    init(self)
    if managed then return end
    self.powerd.batt_capacity_file = "/sys/class/power_supply/max77696-battery/capacity"
    self.powerd.isChargingHW = function()
        local status = read_file("/sys/class/power_supply/max77696-battery/status") or ""
        return status:find("Charging", 1, true) ~= nil
    end
end

dofile("reader.lua")
