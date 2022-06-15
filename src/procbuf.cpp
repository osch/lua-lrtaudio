#include <rtaudio/RtAudio.h>

#include "main.hpp"
#include "controller.hpp"
#include "stream.hpp"
#include "procbuf.hpp"

using namespace lrtaudio;

/* ============================================================================================ */

const char* const LRTAUDIO_PROCBUF_CLASS_NAME = "lrtaudio.ProcessBuffer";

/* ============================================================================================ */
extern "C" {
/* ============================================================================================ */

void lrtaudio_procbuf_clear_midi_buffer(auproc_midibuf* midibuf)
{
    ProcBufUserData* udata = (ProcBufUserData*) midibuf;
    
    udata->midiEventCount  = 0;
    if (udata->bufferData) {
        udata->midiEventsBegin = (auproc_midi_event*)udata->bufferData;
        udata->midiDataBegin   = udata->bufferData + udata->bufferLength;
    } else {
        udata->midiEventsBegin = NULL;
        udata->midiDataBegin   = NULL;
    }
    udata->midiEventsEnd = udata->midiEventsBegin;
    udata->midiDataEnd   = udata->midiDataBegin;
}

/* ============================================================================================ */

uint32_t lrtaudio_procbuf_get_midi_event_count(auproc_midibuf* midibuf)
{
    ProcBufUserData* udata = (ProcBufUserData*) midibuf;

    return udata->midiEventCount;
}

/* ============================================================================================ */

int lrtaudio_procbuf_get_midi_event(auproc_midi_event*  event,
                                    auproc_midibuf*     midibuf,
                                    uint32_t            event_index)
{
    ProcBufUserData* udata = (ProcBufUserData*) midibuf;

    auproc_midi_event* e = udata->midiEventsBegin + event_index;
    if (e < udata->midiEventsEnd) {
        *event = *e;
        return 0;
    } else {
        return ENODATA;
    }
}

/* ============================================================================================ */

unsigned char* lrtaudio_procbuf_reserve_midi_event(auproc_midibuf*  midibuf,
                                                   uint32_t         time,
                                                   size_t           data_size)
{
    ProcBufUserData* udata = (ProcBufUserData*) midibuf;

    auproc_midi_event* e = udata->midiEventsEnd;
    unsigned char* eBuf = udata->midiDataBegin - data_size;
    if ((unsigned char*)(e + 1) <= eBuf) {
        e->time = time;
        e->size = data_size;
        e->buffer = eBuf;
        udata->midiEventsEnd += 1;
        udata->midiDataBegin = eBuf;
        udata->midiEventCount += 1;
        return eBuf;
    } else {
        return NULL;
    }
}

/* ============================================================================================ */

static void setupProcBufMeta(lua_State* L);

static int pushProcBufMeta(lua_State* L)
{
    if (luaL_newmetatable(L, LRTAUDIO_PROCBUF_CLASS_NAME)) {
        setupProcBufMeta(L);
    }
    return 1;
}
/* ============================================================================================ */
} // extern "C"
/* ============================================================================================ */

ProcBufUserData* procbuf::push_new_procbuf(lua_State* L, ControllerUserData* ctrlUdata, bool isMidi)
{
    ProcBufUserData* udata = (ProcBufUserData*) lua_newuserdata(L, sizeof(ProcBufUserData));
    memset(udata, 0, sizeof(ProcBufUserData));              /* -> udata */

    pushProcBufMeta(L);                                     /* -> udata, meta */
    lua_setmetatable(L, -2);                                /* -> udata */
    udata->className = LRTAUDIO_PROCBUF_CLASS_NAME;
    udata->ctrlUdata = ctrlUdata;
    udata->isMidi    = isMidi;
    udata->isAudio   = !isMidi;

    Stream* stream = ctrlUdata->stream;

    udata->nextProcBufUserData = stream->firstProcBufUserData;
    if (udata->nextProcBufUserData) {
        udata->nextProcBufUserData->prevNextProcBufUserData = &udata->nextProcBufUserData;
    }
    udata->prevNextProcBufUserData = &stream->firstProcBufUserData;
    stream->firstProcBufUserData = udata;
    
    {
        size_t size = isMidi ? (8192 * sizeof(float))
                             : (stream->bufferFrames * sizeof(float));
        udata->bufferData = (unsigned char*) malloc(size);
        
        if (udata->bufferData) {
            // TODO mlock?
            udata->bufferLength = size;
            procbuf::clear_midi_events(udata);
        } else {
            return (luaL_error(L, "error allocating process buffer"), (ProcBufUserData*) NULL);
        }
    }

    return udata;
}

/* ============================================================================================ */

void procbuf::release_procbuf(lua_State* L, ProcBufUserData* udata)
{
    if (udata->bufferData) {
        free(udata->bufferData);
        udata->bufferData = NULL;
    }
    if (udata->prevNextProcBufUserData) {
        *udata->prevNextProcBufUserData = udata->nextProcBufUserData;
        if (udata->nextProcBufUserData) {
            udata->nextProcBufUserData->prevNextProcBufUserData = udata->prevNextProcBufUserData;
            udata->nextProcBufUserData = NULL;
        }
        udata->prevNextProcBufUserData = NULL;
    }
    udata->isMidi    = false;
    udata->isAudio   = false;
    udata->ctrlUdata = NULL;
}

/* ============================================================================================ */

void procbuf::clear_midi_events(ProcBufUserData* udata)
{
    udata->midiEventCount  = 0;
    if (udata->bufferData) {
        udata->midiEventsBegin = (auproc_midi_event*)udata->bufferData;
        udata->midiDataBegin   = udata->bufferData + udata->bufferLength;
    } else {
        udata->midiEventsBegin = NULL;
        udata->midiDataBegin   = NULL;
    }
    udata->midiEventsEnd = udata->midiEventsBegin;
    udata->midiDataEnd   = udata->midiDataBegin;
}

/* ============================================================================================ */
extern "C" {
/* ============================================================================================ */

static int ProcBuf_release(lua_State* L)
{
    ProcBufUserData* udata = (ProcBufUserData*) luaL_checkudata(L, 1, LRTAUDIO_PROCBUF_CLASS_NAME);
    procbuf::release_procbuf(L, udata);
    return 0;
}

/* ============================================================================================ */

static int ProcBuf_toString(lua_State* L)
{
    ProcBufUserData* udata = (ProcBufUserData*) luaL_checkudata(L, 1, LRTAUDIO_PROCBUF_CLASS_NAME);
    if (udata->ctrlUdata) {
        lua_pushfstring(L, "%s: %p (%s)", LRTAUDIO_PROCBUF_CLASS_NAME, udata, 
                                            udata->isMidi ? "MIDI" : "AUDIO");
    } else {
        lua_pushfstring(L, "%s: %p", LRTAUDIO_PROCBUF_CLASS_NAME, udata);
    }
    return 1;
}

/* ============================================================================================ */

static ProcBufUserData* checkProcBufUdata(lua_State* L, int arg)
{
    ProcBufUserData* udata = (ProcBufUserData*) luaL_checkudata(L, arg, LRTAUDIO_PROCBUF_CLASS_NAME);
    if (!udata->ctrlUdata) {
        luaL_argerror(L, arg, "invalid ProcessBuffer object");
        return NULL;
    }
    return udata;
}

/* ============================================================================================ */

static const luaL_Reg ProcBufMethods[] = 
{
    { NULL,       NULL } /* sentinel */
};

static const luaL_Reg ProcBufMetaMethods[] = 
{
    { "__gc",       ProcBuf_release  },
    { "__tostring", ProcBuf_toString },

    { NULL,       NULL } /* sentinel */
};

/* ============================================================================================ */

static void setupProcBufMeta(lua_State* L)
{                                                /* -> meta */
    lua_pushstring(L, LRTAUDIO_PROCBUF_CLASS_NAME);/* -> meta, className */
    lua_setfield(L, -2, "__metatable");          /* -> meta */

    luaL_setfuncs(L, ProcBufMetaMethods, 0);       /* -> meta */
    
    lua_newtable(L);                             /* -> meta, ProcBufClass */
    luaL_setfuncs(L, ProcBufMethods, 0);           /* -> meta, ProcBufClass */
    lua_setfield (L, -2, "__index");             /* -> meta */
//   auproc_set_capi(L, -1, &auproc_capi_impl);
}

/* ============================================================================================ */
} // extern "C"
/* ============================================================================================ */
