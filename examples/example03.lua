----------------------------------------------------------------------------------------------------
--[[
     This example generates audio samples from Lua script code on the main thread and lets the
     user interactively modify oscillator parameters.
     This example demonstrates the usage of 
     [Auproc audio sender object](https://github.com/osch/lua-auproc/blob/master/doc/README.md#auproc_new_audio_sender)
     and
     [Auproc audio mixer object](https://github.com/osch/lua-auproc/blob/master/doc/README.md#auproc_new_audio_mixer).
--]]
----------------------------------------------------------------------------------------------------

local nocurses = require("nocurses") -- https://github.com/osch/lua-nocurses
local carray   = require("carray")   -- https://github.com/osch/lua-carray
local mtmsg    = require("mtmsg")    -- https://github.com/osch/lua-mtmsg
local auproc   = require("auproc")   -- https://github.com/osch/lua-auproc
local lrtaudio = require("lrtaudio")

----------------------------------------------------------------------------------------------------

local BUFFER_FRAMES       = 512
local BUFFER_QUEUE_LENGTH = 3

----------------------------------------------------------------------------------------------------

local pi       = math.pi
local sin      = math.sin
local format   = string.format

local function printbold(...)
    nocurses.setfontbold(true) print(...) nocurses.resetcolors()
end

----------------------------------------------------------------------------------------------------

local function choose(title, list, printFilter, noneAllowed)
    printbold(format("List %s:", title))
    for i, p in ipairs(list) do
        if printFilter then
            local txt = printFilter(p)
            if txt then print(i, txt) end
        else
            print(i, p)
        end
    end
    if #list == 0 then
        print(format("No %s found", title))
        os.exit()
    end
    while true do
        io.write(format("Choose %s (1-%d%s): ", title, #list, noneAllowed and " or none" or ""))
        local inp = io.read()
        if inp == "q" then os.exit() end
        if inp ~= "" then
            local p = list[tonumber(inp)]
            if p then
                print()
                return tonumber(inp), p
            end
        elseif noneAllowed then
            return
        end
    end
end

----------------------------------------------------------------------------------------------------

local _, audioApi = choose("RtAudio API", lrtaudio.getCompiledApi())

local controller = lrtaudio.new(audioApi)

----------------------------------------------------------------------------------------------------

local deviceId = choose("Output device", 
                         controller:getDeviceInfo(), 
                         function(d) 
                            return format("%s (out=%d)", d.name, d.outputChannels)
                         end)

----------------------------------------------------------------------------------------------------

print("RtAudio API:", controller:getCurrentApi())

controller:openStream { outputDevice    = deviceId,
                        outputChannels  = 2,
                        bufferFrames    = BUFFER_FRAMES,
                        numberOfBuffers = BUFFER_QUEUE_LENGTH }

----------------------------------------------------------------------------------------------------

local rate    = controller:getStreamSampleRate()
local nframes = controller:getStreamBufferFrames()

print("Output device:", controller:getOutputDeviceInfo().name)
print("Sample rate:",   rate)
print("Buffer frames:", nframes)
print("Buffer queue:",  BUFFER_QUEUE_LENGTH)


----------------------------------------------------------------------------------------------------

local sampleBuffer      = carray.new("float", nframes)
local sampleQueue       = mtmsg.newbuffer()
sampleQueue:notifier(nocurses, "<", BUFFER_QUEUE_LENGTH)

local out1Ctrl = mtmsg.newbuffer()
local out2Ctrl = mtmsg.newbuffer()

local soundBuffer = controller:newStreamBuffer()

local soundSender = auproc.new_audio_sender(soundBuffer, sampleQueue)
local out1 = auproc.new_audio_mixer(soundBuffer, controller:getStreamOutput(1), out1Ctrl)
local out2 = auproc.new_audio_mixer(soundBuffer, controller:getStreamOutput(2), out2Ctrl)

out1:activate()
out2:activate()

function setBalance(bal)
    out1Ctrl:addmsg(1, 0.5 - 0.5*bal)
    out2Ctrl:addmsg(1, 0.5 + 0.5*bal)
end

----------------------------------------------------------------------------------------------------

local f1, f2, v1, v2, bal
local frameTime0, frameTime

local function reset()
    f1 = 440
    f2 = 0.2
    v1 = 0.05
    v2 = 0.1
    bal = 0
    frameTime0 = 0
    frameTime  = 0
    setBalance(bal)
end
reset()

----------------------------------------------------------------------------------------------------

local function printValues()
    print(format("v1: %8.2f, f1: %8.2f, v2: %8.2f, f2: %8.2f, bal: %3.2f", v1, f1, v2, f2, bal))
end
local function printHelp()
    printbold("Press keys <,>,v,V,b,B,f,F,g,G,h,H,r,p for modifying parameters, q for Quit")
end

print()
printHelp()
printValues()

----------------------------------------------------------------------------------------------------

soundSender:activate()
controller:startStream()
while true do
    while sampleQueue:msgcnt() < BUFFER_QUEUE_LENGTH do
        for i = 1, nframes do 
            local t = (frameTime - frameTime0 + i)/rate
            sampleBuffer:set(i, v1*sin(t*(f1+v2*sin(t*f2*2*pi))*2*pi))
        end
        sampleQueue:addmsg(sampleBuffer)
        frameTime = frameTime + nframes
    end
    local c = nocurses.getch()
    if c then
        c = string.char(c)
        if c == "Q" or c == "q" then
            soundSender:deactivate()
            printbold("Quit.")
            break

        elseif c == "F" then
            f1 = f1 + 20
        elseif c == "f" then
            f1 = f1 - 20

        elseif c == "G" then
            f2 = f2 + 0.01
        elseif c == "g" then
            f2 = f2 - 0.01

        elseif c == "H" then
            f2 = f2 + 0.1
        elseif c == "h" then
            f2 = f2 - 0.1

        elseif c == "V" then
            v1 = v1 + 0.01
        elseif c == "v" then
            v1 = v1 - 0.01
        
        elseif c == "B" then
            v2 = v2 + 0.01
        elseif c == "b" then
            v2 = v2 - 0.01
        
        elseif c == "<" then
            bal = bal - 0.05
            if bal < -1 then bal = -1 end
            setBalance(bal)
        elseif c == ">" then
            bal = bal + 0.05
            if bal > 1 then bal = 1 end
            setBalance(bal)
        
        elseif c == 'p' then
            if soundSender:active() then
                printbold("Paused.")
                soundSender:deactivate()
            else
                printbold("Continue.")
                sampleQueue:clear()
                soundSender:activate()
            end
        elseif c == 'r' then
            printbold("Reset.")
            reset()
            soundSender:activate()
        else
            printHelp()
        end
        printValues()
    end
end

----------------------------------------------------------------------------------------------------
