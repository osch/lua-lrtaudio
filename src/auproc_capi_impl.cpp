#include "main.hpp"
#include "auproc_capi_impl.hpp"
#include "controller.hpp"
#include "stream.hpp"
#include "channel.hpp"
#include "procbuf.hpp"

using namespace lrtaudio;
using stream::ProcReg;
using stream::ConnectorInfo;

/* ============================================================================================ */
namespace lrtaudio {
/* ============================================================================================ */

enum ObjectType
{
    TNONE        = 0,
    TCONTROLLER  = 1,
    TCHANNEL     = 2,
    TPROCBUF     = 4
};

/* ============================================================================================ */

typedef float* (*audiometh_getAudioBuffer)(auproc_connector* connector, uint32_t nframes);

typedef auproc_midibuf* (*midimeth_getMidiBuffer)(auproc_connector* connector, uint32_t nframes);

typedef void (*midimeth_clearBuffer)(auproc_midibuf* midibuf);
    
typedef uint32_t (*midimeth_getEventCount)(auproc_midibuf* midibuf);
    
typedef int (*midimeth_getMidiEvent)(auproc_midi_event* event,
                                     auproc_midibuf*    midibuf,
                                     uint32_t               event_index);
    
typedef unsigned char* (*midimeth_reserveMidiEvent)(auproc_midibuf*     midibuf,
                                                    uint32_t            time,
                                                    size_t              data_size);


/* ============================================================================================ */
} // namespace lrtaudio
/* ============================================================================================ */

/* ============================================================================================ */
extern "C" {
/* ============================================================================================ */

static float* channel_getAudioBuffer(auproc_connector* connector, uint32_t nframes)
{
    ChannelUserData* udata = (ChannelUserData*) connector;
    if (nframes <= udata->ctrlUdata->stream->bufferFrames) {
        float* buffers;
        if (udata->isInput) {
            buffers = (float*)(udata->ctrlUdata->stream->currentInputBuffers);
        } else {
            buffers = (float*)(udata->ctrlUdata->stream->currentOutputBuffers);
        }
        return buffers + (nframes * (udata->index-1));
    } else {
        return NULL;
    }
}

static float* procbuf_getAudioBuffer(auproc_connector* connector, uint32_t nframes)
{
    ProcBufUserData* udata = (ProcBufUserData*) connector;
    if (nframes * sizeof(float) <= udata->bufferLength) {
        return (float*) udata->bufferData;
    } else {
        return NULL;
    }
}

static auproc_midibuf* procbuf_getMidiBuffer(auproc_connector* connector, uint32_t nframes)
{
    ProcBufUserData* udata = (ProcBufUserData*) connector;
    return (auproc_midibuf*) udata;
}
       
/* ============================================================================================ */

static const auproc_audiometh channel_audio_methods =
{
    (audiometh_getAudioBuffer) channel_getAudioBuffer
};

/* ============================================================================================ */

static const auproc_audiometh procbuf_audio_methods =
{
    (audiometh_getAudioBuffer) procbuf_getAudioBuffer
};

/* ============================================================================================ */

static const auproc_midimeth procbuf_midi_methods =
{
    (midimeth_getMidiBuffer)     procbuf_getMidiBuffer,
    (midimeth_clearBuffer)       lrtaudio_procbuf_clear_midi_buffer,
    (midimeth_getEventCount)     lrtaudio_procbuf_get_midi_event_count,
    (midimeth_getMidiEvent)      lrtaudio_procbuf_get_midi_event,
    (midimeth_reserveMidiEvent)  lrtaudio_procbuf_reserve_midi_event
};

/* ============================================================================================ */

static ObjectType internGetObjectType(void* udata, size_t len)
{
    if (udata) {
        if (   len == sizeof(ControllerUserData) 
            && ((ControllerUserData*)udata)->className == LRTAUDIO_CONTROLLER_CLASS_NAME) 
        {
            return lrtaudio::TCONTROLLER;
        }
        if (   len == sizeof(ChannelUserData) 
            && ((ChannelUserData*)udata)->className == LRTAUDIO_CHANNEL_CLASS_NAME) 
        {
            return lrtaudio::TCHANNEL;
        }
        if (   len == sizeof(ProcBufUserData) 
            && ((ProcBufUserData*)udata)->className == LRTAUDIO_PROCBUF_CLASS_NAME) 
        {
            return lrtaudio::TPROCBUF;
        }
    }
    return lrtaudio::TNONE;
}

/* ============================================================================================ */

static auproc_obj_type getObjectType(lua_State* L, int index)
{
    void* udata = lua_touserdata(L, index);
    if (udata) {
        size_t len = lua_rawlen(L, index);
        switch (internGetObjectType(udata, len)) {
            case lrtaudio::TCONTROLLER:  return AUPROC_TENGINE;
            case lrtaudio::TCHANNEL:     return AUPROC_TCONNECTOR;
            case lrtaudio::TPROCBUF:     return AUPROC_TCONNECTOR;
            case lrtaudio::TNONE: ;
        }
    }
    return AUPROC_TNONE;
}

/* ============================================================================================ */

static void getConnectorUdata(lua_State* L, int index, ChannelUserData** channelUdata, 
                                                       ProcBufUserData** procBufUdata)
{
    void* udata = lua_touserdata(L, index);
    if (udata) {
        size_t len = lua_rawlen(L, index);
        switch (internGetObjectType(udata, len)) {
            case lrtaudio::TCHANNEL:     *channelUdata = (ChannelUserData*) udata; return;
            case lrtaudio::TPROCBUF:     *procBufUdata = (ProcBufUserData*) udata; return;
            case lrtaudio::TNONE:
            case lrtaudio::TCONTROLLER: ;
        }
    }
}

/* ============================================================================================ */

static auproc_engine* getEngine(lua_State* L, int index, auproc_info* info)
{
    void*  udata = lua_touserdata(L, index);
    size_t len   = udata ? lua_rawlen(L, index) : 0;
    
    ObjectType type = internGetObjectType(udata, len);

    ControllerUserData* ctrlUdata = NULL;
    switch (type) {
        case lrtaudio::TCONTROLLER:  ctrlUdata = ((ControllerUserData*) udata); break;
        case lrtaudio::TCHANNEL:     ctrlUdata = ((ChannelUserData*)    udata)->ctrlUdata; break;
        case lrtaudio::TPROCBUF:     ctrlUdata = ((ProcBufUserData*)    udata)->ctrlUdata; break;
        case lrtaudio::TNONE: ;
    }
    if (ctrlUdata) {
        if (!ctrlUdata->isStreamOpen) {
            return (luaL_error(L, "stream has not been opened"), (auproc_engine*) NULL);
        }
    }
    if (info) {
        memset(info, 0, sizeof(auproc_info));
        info->sampleRate = ctrlUdata->stream->sampleRate;
    }
    return (auproc_engine*) ctrlUdata;
}

/* ============================================================================================ */

static int isEngineClosed(auproc_engine* engine)
{
    ControllerUserData* udata = (ControllerUserData*) engine;
    stream::handle_shutdown(udata);
    return !udata || !udata->isStreamOpen;
}

/* ============================================================================================ */

static void checkEngineIsNotClosed(lua_State* L, auproc_engine* engine)
{
    ControllerUserData* udata = (ControllerUserData*) engine;
    stream::check_not_closed(L, udata);
}

/* ============================================================================================ */

static auproc_con_type getConnectorType(lua_State* L, int index)
{
    ChannelUserData* channelUdata = NULL;
    ProcBufUserData* procBufUdata = NULL;
    getConnectorUdata(L, index, &channelUdata, &procBufUdata);
    if (channelUdata) {
        return AUPROC_AUDIO;
    }
    else if (procBufUdata) {
        if (procBufUdata->isAudio) return AUPROC_AUDIO;
        if (procBufUdata->isMidi)  return AUPROC_MIDI;
    }
    return (auproc_con_type)0;
}

/* ============================================================================================ */

static auproc_direction getPossibleDirections(lua_State* L, int index)
{
    ChannelUserData* channelUdata = NULL;
    ProcBufUserData* procBufUdata = NULL;
    getConnectorUdata(L, index, &channelUdata, &procBufUdata);
    auproc_direction rslt = AUPROC_NONE;
    if (channelUdata) {
        if      (channelUdata->isInput)               rslt = AUPROC_IN;
        else if (channelUdata->procUsageCounter == 0) rslt = AUPROC_OUT;
    }
    else if (procBufUdata) {
        if (procBufUdata->outUsageCounter == 0) rslt = AUPROC_OUT;
        else                                    rslt = AUPROC_IN;
    }
    return rslt;
}

/* ============================================================================================ */

static auproc_reg_err_type checkChannelReg(ControllerUserData* ctrlUdata,
                                           ChannelUserData*    udata,
                                           auproc_con_reg*     conReg)
{
    if (!udata || !udata->ctrlUdata) {
        return AUPROC_REG_ERR_CONNCTOR_INVALID;
    }
    if (udata->ctrlUdata != ctrlUdata)
    {
        return AUPROC_REG_ERR_ENGINE_MISMATCH;
    }
    if (   (conReg->conDirection == AUPROC_IN  && !udata->isInput)
        || (conReg->conDirection == AUPROC_OUT &&  (udata->isInput || udata->procUsageCounter > 0))) 
    {
        return AUPROC_REG_ERR_WRONG_DIRECTION;
    }
    
    if (conReg->conType == AUPROC_AUDIO) {
        return AUPROC_CAPI_REG_NO_ERROR;
    }
    else {
        return AUPROC_REG_ERR_WRONG_CONNECTOR_TYPE;
    }
}

/* ============================================================================================ */

static auproc_reg_err_type checkProcBufReg(ControllerUserData* ctrlUdata,
                                           ProcBufUserData*    udata,
                                           auproc_con_reg*     conReg)
{
    if (!udata || !udata->ctrlUdata) {
        return AUPROC_REG_ERR_CONNCTOR_INVALID;
    }
    if (udata->ctrlUdata != ctrlUdata) {
        return AUPROC_REG_ERR_ENGINE_MISMATCH;
    }
    if ( (conReg->conDirection == AUPROC_IN  && udata->outUsageCounter == 0)
      || (conReg->conDirection == AUPROC_OUT && udata->outUsageCounter != 0))
    {
        return AUPROC_REG_ERR_WRONG_DIRECTION;
    }
    
    if (conReg->conType == AUPROC_AUDIO && udata->isAudio) {
        return AUPROC_CAPI_REG_NO_ERROR;
    }
    else if (conReg->conType == AUPROC_MIDI && udata->isMidi) {
        return AUPROC_CAPI_REG_NO_ERROR;
    }
    else {
        return AUPROC_REG_ERR_WRONG_CONNECTOR_TYPE;
    }
}


/* ============================================================================================ */

static auproc_processor* registerProcessor(lua_State* L, 
                                           int firstConnectorIndex, int connectorCount,
                                           auproc_engine* engine, 
                                           const char* processorName,
                                           void* processorData,
                                           int  (*processCallback)(uint32_t nframes, void* processorData),
                                           int  (*bufferSizeCallback)(uint32_t nframes, void* processorData),
                                           void (*engineClosedCallback)(void* processorData),
                                           void (*engineReleasedCallback)(void* processorData),
                                           auproc_con_reg* conRegList,
                                           auproc_con_reg_err*  regError)
{
    ControllerUserData* ctrlUdata = (ControllerUserData*) engine;
    
    if (!ctrlUdata || !ctrlUdata->isStreamOpen || !processorName || !processorData || !processCallback) {
        if (regError) {
            regError->errorType = AUPROC_REG_ERR_CALL_INVALID;
            regError->conIndex = -1;
        }
        return NULL;
    }
    
    Stream* stream = ctrlUdata->stream;
    
    for (int i = 0; i < connectorCount; ++i) {
        ChannelUserData* channelUdata = NULL;
        ProcBufUserData* procBufUdata = NULL;
        getConnectorUdata(L, firstConnectorIndex + i, &channelUdata, &procBufUdata);
        if (channelUdata == NULL && procBufUdata == NULL) {
            if (regError) {
                regError->errorType = AUPROC_REG_ERR_ARG_INVALID;
                regError->conIndex = i;
            }
            return NULL;
        }
        auproc_con_reg*     conReg = conRegList + i;
        auproc_reg_err_type err;
        if (   (   conReg->conDirection == AUPROC_IN 
                || conReg->conDirection == AUPROC_OUT)

            && (   conReg->conType == AUPROC_AUDIO
                || conReg->conType == AUPROC_MIDI))
        {
            if (channelUdata) {
                err = checkChannelReg(ctrlUdata, channelUdata, conReg);
            }
            else if (procBufUdata) {
                err = checkProcBufReg(ctrlUdata, procBufUdata, conReg);
            }
            else {
                err = AUPROC_REG_ERR_ARG_INVALID;
            }
        } else {
            err = AUPROC_REG_ERR_CALL_INVALID;
        }
        if (err != AUPROC_CAPI_REG_NO_ERROR) {
            if (regError) {
                regError->errorType = err;
                regError->conIndex = i;
            }
            return NULL;
        }
    }
    
    lua_newtable(L);                                     /* -> connectorTable */
    for (int i = 0; i < connectorCount; ++i) {
        lua_pushvalue(L, firstConnectorIndex + i);       /* -> connectorTable, connector */
        lua_rawseti(L, -2, i + 1);                       /* -> connectorTable */
    }
    int connectorTableRef = luaL_ref(L, LUA_REGISTRYINDEX);   /* -> */
    
    ProcReg** oldList = stream->procRegList;
    int       n       = stream->procRegCount;
    
    ProcReg*        newReg    = (ProcReg*) calloc(1, sizeof(ProcReg));
    int             newLength = n + 1;
    ProcReg**       newList   = (ProcReg**) calloc(newLength + 1, sizeof(ProcReg*));
    ConnectorInfo*  conInfos  = (ConnectorInfo*) calloc(connectorCount, sizeof(ConnectorInfo));
    char*           procName  = (char*) malloc(strlen(processorName) + 1);
    if (!newReg || !newList || !conInfos || !procName) {
        if (newReg)   free(newReg);
        if (newList)  free(newList);
        if (conInfos) free(conInfos);
        if (procName) free(procName);
        luaL_unref(L, LUA_REGISTRYINDEX, connectorTableRef);
        luaL_error(L, "out of memory");
        return NULL;
    }
    if (oldList) {
        memcpy(newList, oldList, sizeof(ProcReg*) * n);
    }
    newList[newLength-1] = newReg;
    newList[newLength]   = NULL;
    strcpy(procName, processorName);

    newReg->processorName          = procName;
    newReg->processorData          = processorData;
    newReg->processCallback        = processCallback;
    newReg->bufferSizeCallback     = bufferSizeCallback;
    newReg->engineClosedCallback   = engineClosedCallback;
    newReg->engineReleasedCallback = engineReleasedCallback;
    newReg->connectorTableRef      = connectorTableRef;
    newReg->connectorCount         = connectorCount;
    newReg->connectorInfos         = conInfos;
      
    for (int i = 0; i < connectorCount; ++i) {
        ChannelUserData* channelUdata = NULL;
        ProcBufUserData* procBufUdata = NULL;
        getConnectorUdata(L, firstConnectorIndex + i, &channelUdata, &procBufUdata);
        if (channelUdata) {
            conInfos[i].isChannel = true;
            channelUdata->procUsageCounter += 1;
            if (conRegList[i].conDirection == AUPROC_IN) {
                conInfos[i].isInput  = true;
            } else {
                conInfos[i].isOutput = true;
            }
            conInfos[i].channelUdata = channelUdata;
        } 
        else if (procBufUdata) {
            conInfos[i].isProcBuf = true;
            procBufUdata->procUsageCounter += 1;
            if (conRegList[i].conDirection == AUPROC_IN) {
                procBufUdata->inpUsageCounter += 1;
                conInfos[i].isInput = true;
            } else {
                conInfos[i].isOutput = true;
                procBufUdata->outUsageCounter += 1;
            }
            conInfos[i].procBufUdata = procBufUdata;
        }
    }

    /* --------------------------------------------------------------------- */
    async_mutex_lock(&stream->processMutex);
    {
        int rc = 0;
        if (bufferSizeCallback) {
            rc = bufferSizeCallback(stream->bufferFrames, processorData);
        }
        if (rc != 0) {
            async_mutex_unlock(&stream->processMutex);
            free(newReg);
            free(newList);
            free(conInfos);
            free(procName);
            luaL_unref(L, LUA_REGISTRYINDEX, connectorTableRef);
            luaL_error(L, "error %d from bufferSizeCallback for processor '%s'", processorName);
            return NULL;
        }
        newReg->bufferFrames = stream->bufferFrames;

        stream->procRegList  = newList;
        stream->procRegCount = newLength;
        stream::activate_proc_list_LOCKED(stream, newList);
    }
    async_mutex_unlock(&stream->processMutex);
    /* --------------------------------------------------------------------- */
    
    if (oldList) {
        free(oldList);
    }
    
    for (int i = 0; i < connectorCount; ++i) {
        ChannelUserData* channelUdata = NULL;
        ProcBufUserData* procBufUdata = NULL;
        getConnectorUdata(L, firstConnectorIndex + i, &channelUdata, &procBufUdata);
        if (channelUdata) {
            conRegList[i].connector = (auproc_connector*)channelUdata;
            conRegList[i].audioMethods = &channel_audio_methods;
            conRegList[i].midiMethods  = NULL;
        } 
        else if (procBufUdata) {
            conRegList[i].connector = (auproc_connector*)procBufUdata;
            if (procBufUdata->isAudio) {
                conRegList[i].audioMethods = &procbuf_audio_methods;
                conRegList[i].midiMethods  = NULL;
            }
            else if (procBufUdata->isMidi) {
                conRegList[i].audioMethods = NULL;
                conRegList[i].midiMethods  = &procbuf_midi_methods;
            }
        }
    }
    return (auproc_processor*)newReg;
}

/* ============================================================================================ */

static void releaseProcReg(lua_State* L, ProcReg* reg)
{
    reg->processorData        = NULL;
    reg->processCallback      = NULL;
    reg->engineClosedCallback = NULL;
    
    bool wasActivated = reg->activated;
    if (wasActivated) {
        reg->activated = false;
    }
    if (reg->connectorTableRef != LUA_REFNIL) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, reg->connectorTableRef);  /* -> connectorTable */
        for (int i = 1; i <= reg->connectorCount; ++i) {
            lua_rawgeti(L, -1, i);                             /* -> connectorTable, connector */
            ChannelUserData* channelUdata = NULL;
            ProcBufUserData* procBufUdata = NULL;
            getConnectorUdata(L, -1, &channelUdata, &procBufUdata);
            if (channelUdata) {
                channelUdata->procUsageCounter -= 1;
            }
            else if (procBufUdata) {
                procBufUdata->procUsageCounter -= 1;
                if (reg->connectorInfos) {
                    if (reg->connectorInfos[i-1].isInput) {
                        procBufUdata->inpUsageCounter -= 1;
                        if (wasActivated) {
                            procBufUdata->inpActiveCounter -= 1;
                        }
                    } else {
                        procBufUdata->outUsageCounter -= 1;
                        if (wasActivated) {
                            procBufUdata->outActiveCounter -= 1;
                        }
                    }
                }
            }
            lua_pop(L, 1);                                     /* -> connectorTable */
        }
        lua_pop(L, 1);                                         /* -> */
        luaL_unref(L, LUA_REGISTRYINDEX, reg->connectorTableRef);
        reg->connectorTableRef = LUA_REFNIL;
        reg->connectorCount = 0;
    }
    if (reg->connectorInfos) {
        free(reg->connectorInfos);
        reg->connectorInfos = NULL;
    }
    if (reg->processorName) {
        free(reg->processorName);
        reg->processorName = NULL;
    }
    free(reg);
}

/* ============================================================================================ */

static void unregisterProcessor(lua_State* L,
                                auproc_engine* engine,
                                auproc_processor* processor)
{
    ControllerUserData* ctrlUdata = (ControllerUserData*) engine;
    ProcReg*            reg       = (ProcReg*)            processor;
    Stream*             stream    = ctrlUdata->stream;
    
    int       n       = stream->procRegCount;
    ProcReg** oldList = stream->procRegList;

    int index = -1;
    if (oldList) {
        for (int i = 0; i < n; ++i) {
            if (oldList[i] == reg) {
                index = i;
                break;
            }
        }
    }
    if (index < 0) {
        luaL_error(L, "processor is not registered");
        return;
    }

    for (int i = 0; i < reg->connectorCount; ++i) {
        ConnectorInfo* info = reg->connectorInfos + i;
        if (info->isProcBuf && info->isOutput) {
            if (info->procBufUdata->inpUsageCounter > 0) {
                luaL_error(L, "process buffer %p data is used by another registered processor", info->procBufUdata);
                return;
            }
        }
    }

    /* --------------------------------------------------------------------- */
    
    ProcReg** newList = (ProcReg**) malloc(sizeof(ProcReg*) * (n - 1 + 1));
    
    if (!newList) {
        luaL_error(L, "out of memory");
        return;
    }
    if (index > 0) {
        memcpy(newList, oldList, sizeof(ProcReg*) * index);
    }
    if (index + 1 <= n) {
        memcpy(newList + index, oldList + index + 1, sizeof(ProcReg*) * ((n + 1) - (index + 1)));
    }
    
    async_mutex_lock(&stream->processMutex);
    {
        stream->procRegList  = newList;
        stream->procRegCount = n - 1;
        
        stream::activate_proc_list_LOCKED(stream, newList);
    }
    async_mutex_unlock(&stream->processMutex);
    
    releaseProcReg(L, reg);

    free(oldList);
}

/* ============================================================================================ */

static void activateProcessor(lua_State* L,
                              auproc_engine* engine,
                              auproc_processor* processor)
{
    ControllerUserData* ctrlUdata = (ControllerUserData*) engine;
    ProcReg*            reg       = (ProcReg*)         processor;
    Stream*             stream    = ctrlUdata->stream;

    if (!reg->activated) 
    {
        for (int i = 0; i < reg->connectorCount; ++i) {
            ConnectorInfo* info = reg->connectorInfos + i;
            if (info->isProcBuf) {
                if (info->isInput) {
                    info->procBufUdata->inpActiveCounter += 1;
                } else {
                    info->procBufUdata->outActiveCounter += 1;
                }
            }
        }
        reg->activated = true;
    }
}

/* ============================================================================================ */

static void deactivateProcessor(lua_State* L,
                                auproc_engine* engine,
                                auproc_processor* processor)
{
    ControllerUserData* ctrlUdata = (ControllerUserData*) engine;
    ProcReg*            reg       = (ProcReg*)         processor;
    Stream*             stream    = ctrlUdata->stream;

    if (reg->activated)
    {
        for (int i = 0; i < reg->connectorCount; ++i) {
            ConnectorInfo* info = reg->connectorInfos + i;
            if (info->isProcBuf) {
                if (info->isInput) {
                    info->procBufUdata->inpActiveCounter -= 1;
                } else {
                    info->procBufUdata->outActiveCounter -= 1;
                }
            }
        }
        reg->activated = false;
    }
}

/* ============================================================================================ */

static uint32_t getProcessBeginFrameTime(auproc_engine* engine)
{
    ControllerUserData* ctrlUdata = (ControllerUserData*) engine;
    Stream*             stream    = ctrlUdata->stream;

    return stream->processBeginFrameTime;
}

/* ============================================================================================ */

static void logError(auproc_engine* engine, const char* fmt, ...)
{
    bool finished;
    do {
        va_list args;
        va_start(args, fmt);
            finished = lrtaudio::log_errorV(fmt, args);
        va_end(args);
    } while (!finished);
}

/* ============================================================================================ */

static void logInfo(auproc_engine* engine, const char* fmt, ...)
{
    bool finished;
    do {
        va_list args;
        va_start(args, fmt);
            finished = lrtaudio::log_infoV(fmt, args);
        va_end(args);
    } while (!finished);
}

/* ============================================================================================ */
} // extern "C"
/* ============================================================================================ */


const auproc_capi auproc::capi_impl = 
{
    AUPROC_CAPI_VERSION_MAJOR,
    AUPROC_CAPI_VERSION_MINOR,
    AUPROC_CAPI_VERSION_PATCH,
    
    NULL, /* next_capi */

    getObjectType,
    getEngine,
    isEngineClosed,
    checkEngineIsNotClosed,
    getConnectorType,
    getPossibleDirections,
    registerProcessor,
    unregisterProcessor,
    activateProcessor,
    deactivateProcessor,
    getProcessBeginFrameTime,
    "stream", /* engine_category_name */    
    logError,
    logInfo
};
