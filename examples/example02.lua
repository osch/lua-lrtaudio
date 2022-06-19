----------------------------------------------------------------------------------------------------
--[[
     This examples plays a sinus tone for 4 seconds using
     an [Auproc audio sender object](https://github.com/osch/lua-auproc/blob/master/doc/README.md#auproc_new_audio_sender).
--]]
----------------------------------------------------------------------------------------------------

local nocurses = require("nocurses") -- https://github.com/osch/lua-nocurses
local carray   = require("carray")   -- https://github.com/osch/lua-carray
local mtmsg    = require("mtmsg")    -- https://github.com/osch/lua-mtmsg
local auproc   = require("auproc")   -- https://github.com/osch/lua-auproc
local lrtaudio = require("lrtaudio")

----------------------------------------------------------------------------------------------------

local pi       = math.pi
local sin      = math.sin
local format   = string.format

----------------------------------------------------------------------------------------------------

local controller = lrtaudio.new()

print("RtAudio API:", controller:getCurrentApi())

controller:openStream { outputChannels = 1 }
controller:startStream()

----------------------------------------------------------------------------------------------------

local rate    = controller:getStreamSampleRate()
local nframes = controller:getStreamBufferFrames()

print("Sample rate:",   rate)
print("Buffer frames:", nframes)

----------------------------------------------------------------------------------------------------

local sampleQueue = mtmsg.newbuffer()
local sender      = auproc.new_audio_sender(controller:getStreamOutput(1), sampleQueue)

----------------------------------------------------------------------------------------------------

local duration = 4 -- seconds

local sampleBuffer = carray.new("float", nframes)

for i = 1, math.floor((duration * rate) / nframes + 1) do
    for s = 1, nframes do
        local t = (i * nframes + s) / rate
        sampleBuffer:set(s, 0.05*sin(t*440*2*pi))
    end
    sampleQueue:addmsg(sampleBuffer)
end

----------------------------------------------------------------------------------------------------

sampleQueue:notifier(nocurses, "<", 1) -- notifies nocurses if buffer empty

print("Playing sinus for "..duration.." seconds, press <Q> for Quit")
sender:activate()

while sampleQueue:msgcnt() > 0 do
    local c = nocurses.getch()
    if c then
        c = string.char(c)
        if c == "Q" or c == "q" then
            print("Quit.")
            break
        end
    end
end
print("Finished.")

----------------------------------------------------------------------------------------------------
