-- SPDX-License-Identifier: GPL-3.0-or-later
-- Convert an uploaded PNG/JPEG into the PW3 lock-screen raw format:
--   1072 x 1448, one byte per pixel, row-major, 8 bpp grayscale.
--
-- Runs under the KOReader LuaJIT + compat loader because that is the only
-- image decoder present on the device.  One process per image; the caller
-- writes to a temporary path and renames atomically.

local ffi = require("ffi")
package.path = "/opt/koreader/?.lua;/opt/koreader/?/init.lua;/opt/koreader/frontend/?.lua;/opt/koreader/frontend/?/init.lua;" .. package.path
package.cpath = "/opt/koreader/?.so;/opt/koreader/libs/?.so;" .. package.cpath
require("ffi/loadlib")
local Png = require("ffi/png")
local Jpeg = require("ffi/jpeg")

local W, H = 1072, 1448
local src_path, dst_path = arg[1], arg[2]
if not src_path or not dst_path then
    io.stderr:write("usage: convert-lockscreen.lua <source> <dest>\n")
    os.exit(2)
end

local function is_png(path)
    local f = io.open(path, "rb")
    if not f then return false end
    local header = f:read(8)
    f:close()
    return header == "\137PNG\r\n\26\n"
end

local width, height, data, stride, ncomp

if is_png(src_path) then
    local ok, info = Png.decodeFromFile(src_path, 1)
    if not ok then
        io.stderr:write("png decode failed: " .. tostring(info) .. "\n")
        os.exit(1)
    end
    width, height, data, ncomp = info.width, info.height, info.data, info.ncomp
    stride = width * ncomp
else
    local bb, w, h = Jpeg.openDocument(src_path, false)
    if not bb then
        io.stderr:write("jpeg decode failed\n")
        os.exit(1)
    end
    width, height = w, h
    data = ffi.cast("uint8_t*", bb.data)
    stride = bb.stride
    ncomp = 1
end

if width <= 0 or height <= 0 then
    io.stderr:write("invalid image dimensions\n")
    os.exit(1)
end

-- Cover-fit: scale so both dimensions cover the panel, then crop the centre.
-- Nearest neighbour is deliberate: e-ink has no subpixel detail to preserve.
local scale = math.max(W / width, H / height)
local crop_w = W / scale
local crop_h = H / scale
local x0 = (width - crop_w) / 2
local y0 = (height - crop_h) / 2

local out = ffi.new("uint8_t[?]", W * H)
for y = 0, H - 1 do
    local sy = math.floor(y0 + (y + 0.5) / scale)
    if sy >= height then sy = height - 1 end
    if sy < 0 then sy = 0 end
    local srow = sy * stride
    local drow = y * W
    for x = 0, W - 1 do
        local sx = math.floor(x0 + (x + 0.5) / scale)
        if sx >= width then sx = width - 1 end
        if sx < 0 then sx = 0 end
        local idx = srow + sx * ncomp
        if ncomp == 1 then
            out[drow + x] = data[idx]
        else
            local r = data[idx]
            local g = data[idx + 1]
            local b = data[idx + 2]
            out[drow + x] = math.floor((299 * r + 587 * g + 114 * b + 500) / 1000)
        end
    end
end

local f = io.open(dst_path, "wb")
if not f then
    io.stderr:write("cannot open destination: " .. dst_path .. "\n")
    os.exit(1)
end
f:write(ffi.string(out, W * H))
f:close()
os.exit(0)
