#include <rtaudio/RtAudio.h>

#include "main.hpp"
#include "controller.hpp"
#include "stream.hpp"
#include "channel.hpp"
#include "procbuf.hpp"

#include "receiver_capi.h"

using namespace lrtaudio;

/* ============================================================================================ */

typedef int ProcessCallback(uint32_t nframes, void* processorData);

/* ============================================================================================ */

static void releaseChannelList(lua_State* L, stream::Channels* channels)
{
    if (channels->tableRef != LUA_REFNIL)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, channels->tableRef);  /* -> channelTable */
        for (int i = channels->min; i <= channels->max; ++i) {
            lua_rawgeti(L, -1, i);                                /* -> channelTable, channel */
            ChannelUserData* channelUdata= (ChannelUserData*) lua_touserdata(L, -1);
            if (channelUdata) {
                channel::release_channel(L, channelUdata);
            }
            lua_pop(L, 1);                                        /* -> channelTable */
        }
        lua_pop(L, 1);                                            /* -> */
        luaL_unref(L, LUA_REGISTRYINDEX, channels->tableRef);
        channels->tableRef = LUA_REFNIL;
    }
}

/* ============================================================================================ */

static int setupChannelList(lua_State* L, ControllerUserData* udata,  RtAudio::StreamParameters* params, stream::Channels* channels)
{
    lua_newtable(L);                                          /* -> table */
    channels->tableRef = luaL_ref(L, LUA_REGISTRYINDEX);      /* ->  */

    unsigned int numberChannels = params ? params->nChannels : 0;
    unsigned int firstChannel   = numberChannels ? params->firstChannel + 1 : 0;

    channels->min = firstChannel;
    channels->max = firstChannel + numberChannels - 1;
    
    lua_rawgeti(L, LUA_REGISTRYINDEX, channels->tableRef);          /* -> table */
    for (lua_Integer i = firstChannel; i <= channels->max; ++i) {
        channel::push_new_channel(L, udata, i, channels->isInput); /* -> table, channel */
        lua_rawseti(L, -2, i);                                      /* -> table */
    }
    lua_pop(L, 1);                                                  /* -> */

    return 0;
}


/* ============================================================================================ */

static void setStreamNameRef(lua_State* L, Stream* stream, const char* name)
{
    if (stream->streamNameRef != LUA_REFNIL) {
        luaL_unref(L, LUA_REGISTRYINDEX, stream->streamNameRef);
        stream->streamNameRef = LUA_REFNIL;
    }
    if (name) {
        lua_pushstring(L, name);
        stream->streamNameRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }
}

/* ============================================================================================ */

static void addIntegerToWriter(Stream* stream, lua_Integer i)
{
    stream->statusReceiverCapi->addIntegerToWriter(stream->statusWriter, i);
}

static void addStringToWriter(Stream* stream, const char* str)
{
    stream->statusReceiverCapi->addStringToWriter(stream->statusWriter, str, strlen(str));
}

static void addMsgToReceiver(Stream* stream)
{
    int rc = stream->statusReceiverCapi->msgToReceiver(stream->statusReceiver, stream->statusWriter, 
                                                       false, false, NULL, NULL);
    if (rc != 0) {
        fprintf(stderr, "lrtaudio: error calling status receiver.");
    }
}

/* ============================================================================================ */

int stream::rtaudio_callback(void* outputBuffer, void* inputBuffer, 
                             unsigned int nframes, double streamTime, 
                             RtAudioStreamStatus status, void* voidData)
{
printf("ProcessCallback %d\n", nframes);

    Stream* stream = (Stream*) voidData;
    
    ProcReg** list = stream->activeProcRegList;

    if (stream->confirmedProcRegList != list)
    {
        if (async_mutex_trylock(&stream->processMutex)) {
            stream->confirmedProcRegList = list;
            async_mutex_notify(&stream->processMutex);
            async_mutex_unlock(&stream->processMutex);
        }
    }
    if (!stream->shutdownReceived)
    {
        if (list) {
            stream->currentOutputBuffers = outputBuffer;
            stream->currentInputBuffers  = inputBuffer;
            int i = 0;
            while (true) 
            {
                ProcReg* reg = list[i];
                if (!reg) {
                    break;
                }
                ProcessCallback* processCallback = reg->processCallback;
                if (reg->activated) {
                    reg->outBuffersCleared = false;
                    int rc = processCallback(nframes, reg->processorData);
                    if (rc != 0) {
                        async_mutex_lock(&stream->processMutex);
                        {
                            lrtaudio::log_error("lrtaudio: stream invalidated because processor '%s' returned processing error %d.", reg->processorName, rc);
                            stream->severeProcessingError = true;
                            stream->shutdownReceived = true;
                            async_mutex_notify(&stream->processMutex);

                            if (stream->statusWriter) {
                                addStringToWriter (stream, "ProcessingError");
                                addStringToWriter (stream, "stream invalidated because processor returned processing error");
                                addStringToWriter (stream, reg->processorName);
                                addIntegerToWriter(stream, rc);
                                addMsgToReceiver  (stream);
                            }
                        }
                        async_mutex_unlock(&stream->processMutex);
                        return rc;
                    }
                } else if (!reg->outBuffersCleared) {
                    for (int i = 0, n = reg->connectorCount; i < n; ++i) {
                        ConnectorInfo* info = reg->connectorInfos + i;
                        if (info->isOutput) {
                            if (info->isChannel) {
                                float* b = ((float*)outputBuffer) + (info->channelUdata->index-1) * nframes;
                                memset(b, 0, nframes * sizeof(float));
                            } else if (info->isProcBuf) {
                                if (info->procBufUdata->isAudio) {
                                    float* b = (float*)info->procBufUdata->bufferData;
                                    memset(b, 0, nframes * sizeof(float));
                                }
                                
                            }
                        }
                    }
                    reg->outBuffersCleared = true;
                }
                ++i;
            }
        }
    }
    stream->processBeginFrameTime += nframes;
    return 0;
}

/* ============================================================================================ */


static void errorCallback(RtAudioError::Type type, const std::string& errorText)
{
    switch (type) {
        case RtAudioError::WARNING:
        case RtAudioError::DEBUG_WARNING:
                    lrtaudio::log_info("%s", errorText.c_str()); break;
        
        default:    lrtaudio::log_error("%s", errorText.c_str()); break;
    }
}

/* ============================================================================================ */

int stream::open_stream(lua_State* L, ControllerUserData* udata, 
                        uint32_t sampleRate, uint32_t bufferFrames,
                        RtAudio::StreamOptions*    options,
                        RtAudio::StreamParameters* outParams,
                        RtAudio::StreamParameters* inpParams)
{
    if (udata->stream) {
        stream::release_stream(L, udata);
    }
    udata->stream = (Stream*) calloc(1, sizeof(Stream));
    if (udata->stream) {
        Stream* stream = udata->stream;
        stream->inputs.tableRef  = LUA_REFNIL;
        stream->inputs.max       = -1;
        stream->inputs.isInput   = true;
        stream->outputs.tableRef = LUA_REFNIL;
        stream->outputs.max      = -1;
        stream->streamNameRef    = LUA_REFNIL;
        async_mutex_init(&stream->processMutex);

        if (udata->statusReceiver) {
            stream->statusReceiverCapi = udata->statusReceiverCapi;
            stream->statusReceiver     = udata->statusReceiver;
            stream->statusWriter       = udata->statusReceiverCapi->newWriter(1024, 2);
            if (!stream->statusWriter) {
                return luaL_error(L, "error creating writer for status receiver");
            }
        }

        udata->api->openStream(outParams, inpParams, RTAUDIO_FLOAT32, sampleRate, &bufferFrames,
                               stream::rtaudio_callback, stream, options, errorCallback);
        if (bufferFrames == 0) {
            udata->api->closeStream();
            return luaL_error(L, "error: zero bufferFrames");
        }
        stream->isOpen = true;
        
        if (inpParams) {
            stream->inputDeviceId  = inpParams->deviceId + 1;
            setupChannelList(L, udata, inpParams,  &stream->inputs);
        }
        if (outParams) {
            stream->outputDeviceId = outParams->deviceId + 1;
            setupChannelList(L, udata, outParams, &stream->outputs);
        }
        
        stream->sampleRate   = udata->api->getStreamSampleRate();
        stream->bufferFrames = bufferFrames;
        stream->numberOfBuffers = options->numberOfBuffers;

        setStreamNameRef(L, stream, NULL);
     
        if (options->streamName.length() > 0) {
            setStreamNameRef(L, stream, options->streamName.c_str());
        }
        
        udata->isStreamOpen = true;
    }
    return 0;
}

/* ============================================================================================ */

void stream::close_stream(ControllerUserData* udata)
{
    if (udata->stream && udata->stream->isOpen) 
    {
        Stream* stream = udata->stream;
        if (stream->isRunning) {
            async_mutex_lock  (&stream->processMutex);
                activate_proc_list_LOCKED(stream, NULL);
            async_mutex_unlock(&stream->processMutex);
        }
        udata->api->closeStream();
        
        udata->isStreamOpen = false;
        stream->isOpen      = false;
        stream->isRunning   = false;
        {
            ChannelUserData* c = stream->firstChannelUserData;
            while (c) {
                c->ctrlUdata = NULL;
                c->index     = 0;
                c = c->nextChannelUserData;
            }
        }
        {
            ProcBufUserData* p = stream->firstProcBufUserData;
            while (p) {
                p->ctrlUdata = NULL;
                p = p->nextProcBufUserData;
            }
        }
        if (stream->statusWriter) {
            stream->statusReceiverCapi->freeWriter(stream->statusWriter);
            stream->statusWriter       = NULL;
        }
        stream->statusReceiverCapi = NULL;
        stream->statusReceiver     = NULL;
    }
}

/* ============================================================================================ */

void stream::release_stream(lua_State* L, ControllerUserData* udata)
{
    if (udata->stream) {
        stream::close_stream(udata);

        Stream* stream = udata->stream;
        setStreamNameRef(L, stream, NULL);
        releaseChannelList(L, &stream->inputs);
        releaseChannelList(L, &stream->outputs);
        
        udata->stream = NULL;
    }
}


/* ============================================================================================ */

void stream::handle_shutdown(ControllerUserData* udata)
{
    if (udata && udata->stream && atomic_get(&udata->stream->shutdownReceived)) {
        stream::close_stream(udata);
    }
}

/* ============================================================================================ */

void stream::check_not_closed(lua_State* L, ControllerUserData* udata)
{
    stream::handle_shutdown(udata);
    if (udata && !udata->isStreamOpen) {
        if (udata->stream && atomic_get(&udata->stream->severeProcessingError)) {
            luaL_error(L, "error: stream was closed because of severe processing error");
        } else {
            luaL_error(L, "error: stream is closed");
        }
    }
    if (!udata || !udata->stream) {
        luaL_error(L, "error: no stream");
    }
}

/* ============================================================================================ */

void stream::activate_proc_list_LOCKED(Stream*    stream, 
                                       ProcReg**  newList)
{
    stream->activeProcRegList = newList;
    
    if (stream->isRunning) {
        while (   atomic_get(&stream->shutdownReceived) == 0
               && stream->confirmedProcRegList != newList) 
        {
            async_mutex_wait(&stream->processMutex);
        }
    }
    stream->confirmedProcRegList = newList;
}

/* ============================================================================================ */


