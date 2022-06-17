#include <rtaudio/RtAudio.h>

#define AUPROC_CAPI_IMPLEMENT_SET_CAPI 1
#define RECEIVER_CAPI_IMPLEMENT_GET_CAPI 1

#include "controller.hpp"
#include "stream.hpp"
#include "main.hpp"
#include "channel.hpp"
#include "procbuf.hpp"
#include "auproc_capi.h"
#include "auproc_capi_impl.hpp"
#include "receiver_capi.h"

using namespace lrtaudio;
using stream::open_stream;
using stream::close_stream;
using stream::release_stream;

/* ============================================================================================ */

const char* const LRTAUDIO_CONTROLLER_CLASS_NAME = "lrtaudio.Controller";

/* ============================================================================================ */
extern "C" {
/* ============================================================================================ */

static void setupControllerMeta(lua_State* L);

static int pushControllerMeta(lua_State* L)
{
    if (luaL_newmetatable(L, LRTAUDIO_CONTROLLER_CLASS_NAME)) {
        setupControllerMeta(L);
    }
    return 1;
}

/* ============================================================================================ */

static ControllerUserData* checkCtrlUdata(lua_State* L, int arg)
{
    ControllerUserData* udata = (ControllerUserData*) luaL_checkudata(L, arg, LRTAUDIO_CONTROLLER_CLASS_NAME);
    if (!udata->api) {
        luaL_argerror(L, arg, "invalid controller object");
        return NULL;
    }
    return udata;
}

/* ============================================================================================ */

static ControllerUserData* checkCtrlUdataOpen(lua_State* L, int arg, bool shouldBeOpen)
{
    ControllerUserData* udata = checkCtrlUdata(L, arg);
    if (shouldBeOpen && !udata->isStreamOpen) {
        luaL_argerror(L, arg, "stream has not been opened");
    }
    else if (!shouldBeOpen && udata->isStreamOpen) {
        luaL_argerror(L, arg, "stream is already open");
    }
    return udata;
}

/* ============================================================================================ */

static int Controller_new(lua_State* L)
{
    try {
        int arg = 1;
        int lastArg = lua_gettop(L);
    
        RtAudio::Api apiType = RtAudio::UNSPECIFIED;
        if (lua_isnil(L, arg)) {
            arg += 1;
        } else if (arg <= lastArg && lua_type(L, arg) == LUA_TSTRING) {
            const char* apiTypeName = luaL_checkstring(L, arg);
            apiType = RtAudio::getCompiledApiByName(apiTypeName);
            if (apiType == RtAudio::UNSPECIFIED) {
                return luaL_argerror(L, arg, "unknown rtaudio api");
            }
            arg += 1;
        }
        const receiver_capi* receiverCapi = NULL;
        receiver_object*     receiver     = NULL;
        if (arg <= lastArg) {
            int versErr = 0;
            receiverCapi = receiver_get_capi(L, arg, &versErr);
            if (receiverCapi) {
                receiver = receiverCapi->toReceiver(L, arg);
                ++arg;
            } else {
                if (versErr) {
                    return luaL_argerror(L, arg, "receiver capi version mismatch");
                }
            }
            if (!receiver) {
                return luaL_argerror(L, arg, "expected receiver object");
            }
        }
        ControllerUserData* udata = (ControllerUserData*) lua_newuserdata(L, sizeof(ControllerUserData));
        memset(udata, 0, sizeof(ControllerUserData));              /* -> udata */
        
        pushControllerMeta(L);                                  /* -> udata, meta */
        lua_setmetatable(L, -2);                                /* -> udata */
        udata->className = LRTAUDIO_CONTROLLER_CLASS_NAME;
        if (receiver) {
            udata->statusReceiverCapi = receiverCapi;
            udata->statusReceiver     = receiver;
            receiverCapi->retainReceiver(receiver);
        }
        udata->api = new RtAudio(apiType);
        
        if (!udata->api) {
            return luaL_error(L, "cannot create RtAudio object");
        }
        udata->api->showWarnings(false);
        return 1;
    }
    catch (...) { return lrtaudio::handleException(L); }
}

/* ============================================================================================ */

static int Controller_release(lua_State* L)
{
    try {
        ControllerUserData* udata = (ControllerUserData*)luaL_checkudata(L, 1, LRTAUDIO_CONTROLLER_CLASS_NAME);

        release_stream(L, udata);

        if (udata->statusReceiver) {
            udata->statusReceiverCapi->releaseReceiver(udata->statusReceiver);
            udata->statusReceiverCapi = NULL;
            udata->statusReceiver     = NULL;
        }

        if (udata->api) {
            delete udata->api;
            udata->api = NULL;
        }
        return 0;
    }
    catch (...) { return lrtaudio::handleException(L); }
}

/* ============================================================================================ */

static int Controller_closeStream(lua_State* L)
{
    try {
        ControllerUserData* udata = checkCtrlUdataOpen(L, 1, true);

        stream::release_stream(L, udata);

        return 0;
    }
    catch (...) { return lrtaudio::handleException(L); }
}

/* ============================================================================================ */

static int Controller_toString(lua_State* L)
{
    ControllerUserData* udata = (ControllerUserData*) luaL_checkudata(L, 1, LRTAUDIO_CONTROLLER_CLASS_NAME);
    lua_pushfstring(L, "%s: %p", LRTAUDIO_CONTROLLER_CLASS_NAME, udata);
    return 1;
}

/* ============================================================================================ */

static void pushed(lua_State* L, int* n) 
{
    *n += 1;
    if (lua_gettop(L) != *n) {
        lua_replace(L, *n);
        lua_settop(L, *n);
    }
}

static int Controller_info(lua_State* L)
{
    ControllerUserData* udata = (ControllerUserData*) luaL_checkudata(L, 1, LRTAUDIO_CONTROLLER_CLASS_NAME);
    if (!udata->api) {
        lua_pushfstring(L, 
            "{\n"
            "    %s,\n"
            "}\n",
    
            quoteString(L, lua_pushfstring(L, "%s: %p", LRTAUDIO_CONTROLLER_CLASS_NAME, udata))
        );
            
    }
    else {
        int n  = lua_gettop(L);
        int n0 = n + 1;
        
            lua_pushfstring(L, 
                "{\n"
                "    %s,\n\n"
                "    currentApi         = %s,\n"
                "    streamStatus       = \"%s\",\n",
                
                quoteString(L, lua_pushfstring(L, "%s: %p", LRTAUDIO_CONTROLLER_CLASS_NAME, udata)),
                quoteString(L, RtAudio::getApiName(udata->api->getCurrentApi()).c_str()),
                udata->isStreamOpen ? "open" : "closed"
            ); pushed(L, &n);
        
        Stream* stream = udata->isStreamOpen ? udata->stream : NULL;
        
        if (stream && stream->inputDeviceId > 0) {
            lua_pushfstring(L, 
                "\n"
                "    inputDevice        = %d,\n"
                "    inputChannels      = %d,\n"
                "    firstInputChannel  = %d,\n",
                
                stream->inputDeviceId,
                stream->inputs.max  - stream->inputs.min + 1,
                stream->inputs.min
            ); pushed(L, &n);
        }
        if (stream && stream->outputDeviceId > 0) {
            lua_pushfstring(L, 
                "\n"
                "    outputDevice       = %d,\n"
                "    outputChannels     = %d,\n"
                "    firstOutputChannel = %d,\n",
                
                stream->outputDeviceId,
                stream->outputs.max - stream->outputs.min + 1,
                stream->outputs.min
            ); pushed(L, &n);
        }
        if (stream) {
            lua_pushfstring(L, 
                "\n"
                "    streamName         = %s,\n"
                "    sampleRate         = %d,\n"
                "    bufferFrames       = %d,\n"
                "    numberOfBuffers    = %d,\n",
                
                stream->streamNameRef != LUA_REFNIL 
                    ? quoteString(L, (lua_rawgeti(L, LUA_REGISTRYINDEX, stream->streamNameRef),
                                      lua_tostring(L, -1))) : "nil",
                udata->api->getStreamSampleRate(),
                stream->bufferFrames,
                stream->numberOfBuffers
            ); pushed(L, &n);
        }
            lua_pushfstring(L, 
                "}\n"
            ); pushed(L, &n);

        lua_concat(L, n - n0 + 1);
    }
    return 1;
}

/* ============================================================================================ */

static int Controller_getCurrentApi(lua_State* L)
{
    try {
        ControllerUserData* udata = checkCtrlUdata(L, 1);
        lua_pushstring(L, RtAudio::getApiName(udata->api->getCurrentApi()).c_str());
        return 1;
    }
    catch (...) { return lrtaudio::handleException(L); }
}

/* ============================================================================================ */

static int Controller_getDeviceCount(lua_State* L)
{
    try {
        ControllerUserData* udata = checkCtrlUdata(L, 1);
        lua_pushinteger(L, udata->api->getDeviceCount());
        return 1;
    }
    catch (...) { return lrtaudio::handleException(L); }
}

/* ============================================================================================ */

static int Controller_getDefaultInputDevice(lua_State* L)
{
    try {
        ControllerUserData* udata = checkCtrlUdata(L, 1);
        lua_pushinteger(L, 1 + udata->api->getDefaultInputDevice());
        return 1;
    }
    catch (...) { return lrtaudio::handleException(L); }
}

/* ============================================================================================ */

static int Controller_getDefaultOutputDevice(lua_State* L)
{
    try {
        ControllerUserData* udata = checkCtrlUdata(L, 1);
        lua_pushinteger(L, 1 + udata->api->getDefaultOutputDevice());
        return 1;
    }
    catch (...) { return lrtaudio::handleException(L); }
}

/* ============================================================================================ */

static int pushDeviceInfo(lua_State* L, int index, const RtAudio::DeviceInfo& deviceInfo)
{
    lua_newtable(L);       // -> deviceInfo

    lua_pushstring(L, "deviceId");// -> deviceInfo, key
    lua_pushinteger(L, index);    // -> deviceInfo, key, value
    lua_rawset(L, -3);            // -> deviceInfo

    lua_pushstring(L, "probed");           // -> deviceInfo, key
    lua_pushboolean(L, deviceInfo.probed); // -> deviceInfo, key, value
    lua_rawset(L, -3);                     // -> deviceInfo

    lua_pushstring(L, "name");                  // -> deviceInfo, key
    lua_pushstring(L, deviceInfo.name.c_str()); // -> deviceInfo, key, value
    lua_rawset(L, -3);                          // -> deviceInfo

    lua_pushstring(L, "outputChannels");           // -> deviceInfo, key
    lua_pushinteger(L, deviceInfo.outputChannels); // -> deviceInfo, key, value
    lua_rawset(L, -3);                             // -> deviceInfo

    lua_pushstring(L, "inputChannels");           // -> deviceInfo, key
    lua_pushinteger(L, deviceInfo.inputChannels); // -> deviceInfo, key, value
    lua_rawset(L, -3);                            // -> deviceInfo

    lua_pushstring(L, "duplexChannels");           // -> deviceInfo, key
    lua_pushinteger(L, deviceInfo.duplexChannels); // -> deviceInfo, key, value
    lua_rawset(L, -3);                             // -> deviceInfo

    lua_pushstring(L, "isDefaultOutput");           // -> deviceInfo, key
    lua_pushboolean(L, deviceInfo.isDefaultOutput); // -> deviceInfo, key, value
    lua_rawset(L, -3);                              // -> deviceInfo

    lua_pushstring(L, "isDefaultInput");           // -> deviceInfo, key
    lua_pushboolean(L, deviceInfo.isDefaultInput); // -> deviceInfo, key, value
    lua_rawset(L, -3);                             // -> deviceInfo

    lua_pushstring(L, "sampleRates");                  // -> deviceInfo, key
    lua_newtable(L);                                   // -> deviceInfo, key, value
    for (int i = 0; i < deviceInfo.sampleRates.size(); ++i) {
        lua_pushinteger(L, deviceInfo.sampleRates[i]); // -> deviceInfo, key, value, rate
        lua_rawseti(L, -2, i+1);                       // -> deviceInfo, key, value
    }
    lua_rawset(L, -3);                                 // -> deviceInfo

    lua_pushstring(L, "preferredSampleRate");           // -> deviceInfo, key
    lua_pushinteger(L, deviceInfo.preferredSampleRate); // -> deviceInfo, key, value
    lua_rawset(L, -3);                                  // -> deviceInfo

    lua_pushstring(L, "nativeFormats");                // -> deviceInfo, key
    lua_newtable(L);                                   // -> deviceInfo, key, value
    {
        int i = 1;
        if (deviceInfo.nativeFormats & RTAUDIO_SINT8) {
            lua_pushstring(L, "SINT8");                // -> deviceInfo, key, value, format
            lua_rawseti(L, -2, i++);                   // -> deviceInfo, key, value
        }
        if (deviceInfo.nativeFormats & RTAUDIO_SINT16) {
            lua_pushstring(L, "SINT16");               // -> deviceInfo, key, value, format
            lua_rawseti(L, -2, i++);                   // -> deviceInfo, key, value
        }
        if (deviceInfo.nativeFormats & RTAUDIO_SINT24) {
            lua_pushstring(L, "SINT24");               // -> deviceInfo, key, value, format
            lua_rawseti(L, -2, i++);                   // -> deviceInfo, key, value
        }
        if (deviceInfo.nativeFormats & RTAUDIO_SINT32) {
            lua_pushstring(L, "SINT32");               // -> deviceInfo, key, value, format
            lua_rawseti(L, -2, i++);                   // -> deviceInfo, key, value
        }
        if (deviceInfo.nativeFormats & RTAUDIO_FLOAT32) {
            lua_pushstring(L, "FLOAT32");              // -> deviceInfo, key, value, format
            lua_rawseti(L, -2, i++);                   // -> deviceInfo, key, value
        }
        if (deviceInfo.nativeFormats & RTAUDIO_FLOAT64) {
            lua_pushstring(L, "FLOAT64");              // -> deviceInfo, key, value, format
            lua_rawseti(L, -2, i++);                   // -> deviceInfo, key, value
        }
    }
    lua_rawset(L, -3);                                 // -> deviceInfo

    return 1;
}

/* ============================================================================================ */

static int Controller_getDeviceInfo(lua_State* L)
{
    try {
        ControllerUserData* udata = checkCtrlUdata(L, 1);
        lua_Integer deviceId;
        bool hasDeviceId = false;
        if (!lua_isnoneornil(L, 2)) {
            deviceId = luaL_checkinteger(L, 2);
            hasDeviceId = true;
        }
        if (hasDeviceId) {
            if (deviceId <= 0 || deviceId > udata->api->getDeviceCount()) {
                return luaL_argerror(L, 2, "invalid deviceId");
            }
            pushDeviceInfo(L, deviceId, udata->api->getDeviceInfo(deviceId - 1));
        } else {
            lua_newtable(L);   // -> list
            for (int i = 0; i < udata->api->getDeviceCount(); ++i) {
                pushDeviceInfo(L, i+1, udata->api->getDeviceInfo(i)); // -> list, deviceInfo
                lua_rawseti(L, -2, i+1); // -> list
            }
        }
        return 1;
    }
    catch (...) { return lrtaudio::handleException(L); }
}

/* ============================================================================================ */

static int pushChannels(lua_State* L, ControllerUserData* udata, stream::Channels* list)
{
    bool isInput = list->isInput;
    
    int index1;
    int index2;
    bool onlyGet = false;

    if (lua_gettop(L) > 1) {
        index1 = -1;
        if (lua_isinteger(L, 2)) {
            index1 = lua_tointeger(L, 2);
        }
        if (index1 < 1) {
            return luaL_argerror(L, 2, "positive integer expected");
        }
        if (index1 < list->min || index1 > list->max) {
            return luaL_argerror(L, 2, "invalid index");
        }
        index2 = -1;
        if (lua_isnoneornil(L, 3)) {
            index2 = index1;
        }
        else if (lua_isinteger(L, 3)) {
            index2 = lua_tointeger(L, 3);
        }
        if (index2 < 0) {
            return luaL_argerror(L, 3, "non negative integer expected");
        }
        if (index2 > list->max) {
            return luaL_argerror(L, 3, "invalid index");
        }
    }
    else {
        index1 = list->min;
        index2 = list->max;
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, list->tableRef);  /* -> channelTable */
    
    int channelTable = lua_gettop(L);
    int count = 0;
    for (int i = index1; i <= index2; ++i) {
        lua_rawgeti(L, channelTable, i);                            /* -> ..., channel */
        count += 1;
    }
    return count;
}

/* ============================================================================================ */

static int Controller_input(lua_State* L)
{
    try {
        ControllerUserData* udata = checkCtrlUdataOpen(L, 1, true);
        return pushChannels(L, udata, &udata->stream->inputs);
    }
    catch (...) { return lrtaudio::handleException(L); }
}

/* ============================================================================================ */

static int Controller_output(lua_State* L)
{
    try {
        ControllerUserData* udata = checkCtrlUdataOpen(L, 1, true);
        return pushChannels(L, udata, &udata->stream->outputs);
    }
    catch (...) { return lrtaudio::handleException(L); }
}

/* ============================================================================================ */


// value must be on top of stack
static bool checkArgTableValueType(lua_State* L, int argTable, const char* key, const char* expectedKey, int expectedType)
{
    if (strcmp(key, expectedKey) == 0) {
        if (lua_type(L, -1) == expectedType) {
            return true;
        } else {
            return luaL_argerror(L, argTable, 
                                    lua_pushfstring(L, "%s expected as '%s' value, got %s", 
                                             lua_typename(L, expectedType),
                                             expectedKey, 
                                             luaL_typename(L, -1)));
        }
    }
    return false;
}


static bool checkArgTableValueInt(lua_State* L, int argTable, const char* key, const char* expectedKey, int minValue, lua_Integer* out)
{
    if (strcmp(key, expectedKey) == 0) {
        if (lua_type(L, -1) == LUA_TNUMBER) {
            if (lua_isinteger(L, -1)) {
                lua_Integer val = lua_tointeger(L, -1);
                if (val >= minValue) {
                    *out = val;
                    return true;
                }
            }
            return luaL_argerror(L, argTable, 
                                    lua_pushfstring(L, "'%s' value should be integer >= %d", 
                                             expectedKey,
                                             minValue));
        } else {
            return luaL_argerror(L, argTable, 
                                    lua_pushfstring(L, "%s expected as '%s' value, got %s", 
                                             lua_typename(L, LUA_TNUMBER),
                                             expectedKey, 
                                             luaL_typename(L, -1)));
        }
    }
    return false;
}

static int fillStreamParameters(lua_State* L, RtAudio* api,
                                              lua_Integer device, bool isInput, RtAudio::DeviceInfo* deviceInfo,
                                              lua_Integer firstChannel, lua_Integer channels,
                                              RtAudio::StreamParameters* params)
{
    bool isDefault = false;
    if (device <= 0) {
        device = 1 + (isInput ? api->getDefaultInputDevice()
                              : api->getDefaultOutputDevice());
        isDefault = true;
    }
    *deviceInfo = api->getDeviceInfo(device-1);

    params->firstChannel = firstChannel - 1;
    params->nChannels    = channels;
    params->deviceId     = device - 1;
    return 0;
}

static int Controller_openStream(lua_State* L)
{
    try {
        ControllerUserData* udata = checkCtrlUdataOpen(L, 1, false);

        int initArg = 2;
        
        lua_Integer inputDevice = -1;
        lua_Integer outputDevice = -1;

        lua_Integer inputChannels = -1;
        lua_Integer outputChannels = -1;
        
        lua_Integer firstInputChannel = -1;
        lua_Integer firstOutputChannel = -1;
        
        lua_Integer sampleRate = -1;
        lua_Integer bufferFrames = 256;
        lua_Integer numberOfBuffers = -1;

        RtAudio::StreamOptions options;
        options.flags = RTAUDIO_NONINTERLEAVED; //|RTAUDIO_SCHEDULE_REALTIME|RTAUDIO_MINIMIZE_LATENCY;
        
        if (!lua_isnoneornil(L, initArg)) 
        {
            luaL_checktype(L, initArg, LUA_TTABLE);
            lua_pushnil(L);                 /* -> nil */
            while (lua_next(L, initArg)) {  /* -> key, value */
                if (lua_type(L, -2) != LUA_TSTRING) {
                    return luaL_argerror(L, initArg, 
                                         lua_pushfstring(L, "got table key of type %s, but string expected", 
                                                         lua_typename(L, lua_type(L, -2))));
                }
                const char* key = lua_tostring(L, -2);

                     if (checkArgTableValueInt(L, initArg, key, "inputDevice", 1, &inputDevice))
                {}
                else if (checkArgTableValueInt(L, initArg, key, "outputDevice", 1, &outputDevice)) 
                {}
                else if (checkArgTableValueInt(L, initArg, key, "inputChannels", 0, &inputChannels)) 
                {}
                else if (checkArgTableValueInt(L, initArg, key, "outputChannels", 0, &outputChannels)) 
                {}
                else if (checkArgTableValueInt(L, initArg, key, "firstInputChannel", 1, &firstInputChannel)) 
                {}
                else if (checkArgTableValueInt(L, initArg, key, "firstOutputChannel", 1, &firstOutputChannel)) 
                {}
                else if (checkArgTableValueInt(L, initArg, key, "sampleRate", 1, &sampleRate)) 
                {}
                else if (checkArgTableValueInt(L, initArg, key, "bufferFrames", 0, &bufferFrames)) 
                {}
                else if (checkArgTableValueInt(L, initArg, key, "numberOfBuffers", 1, &numberOfBuffers)) 
                {
                    if (numberOfBuffers >= 0) {
                        options.numberOfBuffers = numberOfBuffers;
                    }
                }
                else if (checkArgTableValueType(L, initArg, key, "streamName", LUA_TSTRING)) 
                {
                    options.streamName = lua_tostring(L, -1);
                }
                else {
                    return luaL_argerror(L, initArg, 
                                         lua_pushfstring(L, "unexpected table key '%s'", 
                                                         key));
                }                           /* -> key, value */
                lua_pop(L, 1);              /* -> key */
            }                               /* -> */
        }

        RtAudio::StreamParameters inStreamParams;
        RtAudio::StreamParameters outStreamParams;

        RtAudio::StreamParameters* inpParams = NULL;
        RtAudio::StreamParameters* outParams = NULL;

        if (inputChannels <= 0 && outputChannels <= 0) {
            return luaL_error(L, "cannot open stream without input and output channels");
        }

        if (firstInputChannel > inputChannels) {
            return luaL_argerror(L, initArg, "invalid firstInputChannel");
        }
        if (firstOutputChannel > outputChannels) {
            return luaL_argerror(L, initArg, "invalid firstOutputChannel");
        }
        if (firstInputChannel < 0) {
            firstInputChannel = 1;
        }
        if (firstOutputChannel < 0) {
            firstOutputChannel = 1;
        }
        RtAudio::DeviceInfo inputDeviceInfo;
        RtAudio::DeviceInfo outputDeviceInfo;

        if (inputChannels > 0) {
            inpParams = &inStreamParams;
            fillStreamParameters(L, udata->api,
                                    inputDevice, true, &inputDeviceInfo,
                                    firstInputChannel, inputChannels, inpParams);
        }
        if (outputChannels > 0) {
            outParams = &outStreamParams;
            fillStreamParameters(L, udata->api,
                                    outputDevice, false, &outputDeviceInfo, 
                                    firstOutputChannel, outputChannels, outParams);
        }
 
        if (sampleRate < 0) {
            if (inpParams && inputDeviceInfo.preferredSampleRate > sampleRate) {
                sampleRate = inputDeviceInfo.preferredSampleRate;
            }
            if (outParams && outputDeviceInfo.preferredSampleRate > sampleRate) {
                sampleRate = outputDeviceInfo.preferredSampleRate;
            }
            if (sampleRate < 0) {
                return luaL_error(L, "error obtaining default sampleRate");
            }
        }

        open_stream(L, udata, sampleRate, bufferFrames, &options,
                              outParams, inpParams);
        
        if (!udata->stream) {
            return luaL_error(L, "error allocating stream");
        }
        
        return 0;
    }
    catch (...) { return lrtaudio::handleException(L); }
}

/* ============================================================================================ */

static int Controller_startStream(lua_State* L)
{
    try {
        ControllerUserData* udata = checkCtrlUdataOpen(L, 1, true);
        if (!udata->stream->isRunning) {
            udata->api->startStream();
            udata->stream->isRunning = true;
        }
        return 0;
    }
    catch (...) { return lrtaudio::handleException(L); }
}

/* ============================================================================================ */

static int Controller_stopStream(lua_State* L)
{
    try {
        ControllerUserData* udata = checkCtrlUdataOpen(L, 1, true);
        if (udata->stream->isRunning) {
            udata->api->stopStream();
            udata->stream->isRunning = false;
        }
        return 0;
    }
    catch (...) { return lrtaudio::handleException(L); }
}

/* ============================================================================================ */

static int Controller_getFrameTime(lua_State* L)
{
    try {
        ControllerUserData* udata = checkCtrlUdataOpen(L, 1, true);
        lua_pushinteger(L,   udata->stream->processBeginFrameTime 
                           + udata->stream->bufferFrames);
        return 1;
    }
    catch (...) { return lrtaudio::handleException(L); }
}

/* ============================================================================================ */

static int Controller_getBufferFrames(lua_State* L)
{
    try {
        ControllerUserData* udata = checkCtrlUdataOpen(L, 1, true);
        lua_pushinteger(L, udata->stream->bufferFrames);
        return 1;
    }
    catch (...) { return lrtaudio::handleException(L); }
}

/* ============================================================================================ */

static int Controller_getSampleRate(lua_State* L)
{
    try {
        ControllerUserData* udata = checkCtrlUdataOpen(L, 1, true);
        lua_pushinteger(L, udata->stream->sampleRate);
        return 1;
    }
    catch (...) { return lrtaudio::handleException(L); }
}

/* ============================================================================================ */

static int Controller_getInputInfo(lua_State* L)
{
    try {
        ControllerUserData* udata = checkCtrlUdataOpen(L, 1, true);
        if (udata->stream->inputDeviceId > 0) {
            return pushDeviceInfo(L, udata->stream->inputDeviceId,
                                     udata->api->getDeviceInfo(udata->stream->inputDeviceId - 1));
        }
        return 0;
    }
    catch (...) { return lrtaudio::handleException(L); }
}

/* ============================================================================================ */

static int Controller_getOutputInfo(lua_State* L)
{
    try {
        ControllerUserData* udata = checkCtrlUdataOpen(L, 1, true);
        if (udata->stream->outputDeviceId > 0) {
            return pushDeviceInfo(L, udata->stream->outputDeviceId, 
                                     udata->api->getDeviceInfo(udata->stream->outputDeviceId - 1));
        }
        return 0;
    }
    catch (...) { return lrtaudio::handleException(L); }
}

/* ============================================================================================ */

static const char* const procBufTypes[] =
{
    "MIDI",
    "AUDIO",
    NULL
};

static int Controller_newProcessBuffer(lua_State* L)
{
    int arg = 1;
    ControllerUserData* ctrlUdata = checkCtrlUdataOpen(L, arg++, true);
    int type = luaL_checkoption(L, arg, "AUDIO", procBufTypes);
    
    bool isMidi = (type == 0);

    procbuf::push_new_procbuf(L, ctrlUdata, isMidi);
    
    return 1;

}

/* ============================================================================================ */

static const luaL_Reg ControllerMethods[] = 
{
    { "getCurrentApi",           Controller_getCurrentApi          },
    { "getDeviceCount",          Controller_getDeviceCount         },
    { "getDeviceInfo",           Controller_getDeviceInfo          },
    { "getDefaultInputDevice",   Controller_getDefaultInputDevice  },
    { "getDefaultOutputDevice",  Controller_getDefaultOutputDevice },
    { "input",                   Controller_input                  },
    { "output",                  Controller_output                 },
    { "openStream",              Controller_openStream             },
    { "startStream",             Controller_startStream            },
    { "stopStream",              Controller_stopStream             },
    { "closeStream",             Controller_closeStream            },
    { "close",                   Controller_release                },
    { "getFrameTime",            Controller_getFrameTime           },
    { "getBufferFrames",         Controller_getBufferFrames        },
    { "getSampleRate",           Controller_getSampleRate          },
    { "getInputInfo",            Controller_getInputInfo           },
    { "getOutputInfo",           Controller_getOutputInfo          },
    { "info",                    Controller_info                   },
    { "newProcessBuffer",        Controller_newProcessBuffer       },
    { NULL,                      NULL } /* sentinel */
};

static const luaL_Reg ControllerMetaMethods[] = 
{
    { "__gc",       Controller_release  },
    { "__tostring", Controller_toString },

    { NULL,       NULL } /* sentinel */
};

static const luaL_Reg ModuleFunctions[] = 
{
    { "new", Controller_new  },
    { NULL,        NULL } /* sentinel */
};

/* ============================================================================================ */

static void setupControllerMeta(lua_State* L)
{                                                      /* -> meta */
    lua_pushstring(L, LRTAUDIO_CONTROLLER_CLASS_NAME);        /* -> meta, className */
    lua_setfield(L, -2, "__metatable");                /* -> meta */

    luaL_setfuncs(L, ControllerMetaMethods, 0);       /* -> meta */
    
    lua_newtable(L);                                   /* -> meta, RtaudioClass */
    luaL_setfuncs(L, ControllerMethods, 0);           /* -> meta, RtaudioClass */
    lua_setfield (L, -2, "__index");                   /* -> meta */
    auproc_set_capi(L, -1, &auproc::capi_impl);
}


/* ============================================================================================ */

int lrtaudio_controller_init_module(lua_State* L, int module)
{
    if (luaL_newmetatable(L, LRTAUDIO_CONTROLLER_CLASS_NAME)) {
        setupControllerMeta(L);
    }
    lua_pop(L, 1);
    
    lua_pushvalue(L, module);
        luaL_setfuncs(L, ModuleFunctions, 0);
    lua_pop(L, 1);
    
    return 0;
}

/* ============================================================================================ */
} // extern "C"
/* ============================================================================================ */
