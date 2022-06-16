----------------------------------------------------------------------------------------------------
--[[
     This example lists all compiled [RtAudio] APIs and for each API it displays infos for 
     all available devices. 
--]]
----------------------------------------------------------------------------------------------------

local inspect  = require("inspect")   -- https://luarocks.org/modules/kikito/inspect
local lrtaudio = require("lrtaudio")


print("---------------------------------------------------------------------------------------------")

local apis = lrtaudio.getCompiledApi()

print("Compiled APIS:", inspect(apis))

for _, api in ipairs(apis) do
    print("-----------------------------------------------------------------------------------------")
    print("API: ", api)
    local ok, rslt = pcall(function()
        local controller = lrtaudio.new(api)
        return controller:getDeviceInfo()
    end)
    if ok then
        print("DeviceInfo", inspect(rslt))
    else
        print("Error", rslt)
    end
end

print("---------------------------------------------------------------------------------------------")
