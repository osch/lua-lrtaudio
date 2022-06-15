#include <rtaudio/RtAudio.h>

#include "main.hpp"
#include "controller.hpp"
#include "stream.hpp"
#include "channel.hpp"

using namespace lrtaudio;

/* ============================================================================================ */

const char* const LRTAUDIO_CHANNEL_CLASS_NAME = "lrtaudio.Channel";

/* ============================================================================================ */
extern "C" {
/* ============================================================================================ */

static void setupChannelMeta(lua_State* L);

static int pushChannelMeta(lua_State* L)
{
    if (luaL_newmetatable(L, LRTAUDIO_CHANNEL_CLASS_NAME)) {
        setupChannelMeta(L);
    }
    return 1;
}
/* ============================================================================================ */
} // extern "C"
/* ============================================================================================ */

ChannelUserData* channel::push_new_channel(lua_State* L, ControllerUserData* ctrlUdata, int index, bool isInput)
{
    ChannelUserData* udata = (ChannelUserData*) lua_newuserdata(L, sizeof(ChannelUserData));
    memset(udata, 0, sizeof(ChannelUserData));              /* -> udata */

    pushChannelMeta(L);                                     /* -> udata, meta */
    lua_setmetatable(L, -2);                                /* -> udata */
    udata->className = LRTAUDIO_CHANNEL_CLASS_NAME;
    udata->ctrlUdata = ctrlUdata;
    udata->index     = index;
    udata->isInput   = isInput;

    Stream* stream = ctrlUdata->stream;

    udata->nextChannelUserData = stream->firstChannelUserData;
    if (udata->nextChannelUserData) {
        udata->nextChannelUserData->prevNextChannelUserData = &udata->nextChannelUserData;
    }
    udata->prevNextChannelUserData = &stream->firstChannelUserData;
    stream->firstChannelUserData = udata;
    
    return udata;
}

/* ============================================================================================ */

void channel::release_channel(lua_State* L, ChannelUserData* udata)
{
    if (udata->prevNextChannelUserData) {
        *udata->prevNextChannelUserData = udata->nextChannelUserData;
        if (udata->nextChannelUserData) {
            udata->nextChannelUserData->prevNextChannelUserData = udata->prevNextChannelUserData;
            udata->nextChannelUserData = NULL;
        }
        udata->prevNextChannelUserData = NULL;
    }
    udata->index     = 0;
    udata->ctrlUdata = NULL;
}

/* ============================================================================================ */
extern "C" {
/* ============================================================================================ */

static int Channel_release(lua_State* L)
{
    ChannelUserData* udata = (ChannelUserData*) luaL_checkudata(L, 1, LRTAUDIO_CHANNEL_CLASS_NAME);
    channel::release_channel(L, udata);
    return 0;
}

/* ============================================================================================ */

static int Channel_toString(lua_State* L)
{
    ChannelUserData* udata = (ChannelUserData*) luaL_checkudata(L, 1, LRTAUDIO_CHANNEL_CLASS_NAME);
    if (udata->ctrlUdata) {
        lua_pushfstring(L, "%s: %p (%s%d)", LRTAUDIO_CHANNEL_CLASS_NAME, udata, 
                                            udata->isInput ? "IN" : "OUT", udata->index);
    } else {
        lua_pushfstring(L, "%s: %p", LRTAUDIO_CHANNEL_CLASS_NAME, udata);
    }
    return 1;
}

/* ============================================================================================ */

static ChannelUserData* checkChannelUdata(lua_State* L, int arg)
{
    ChannelUserData* udata = (ChannelUserData*) luaL_checkudata(L, arg, LRTAUDIO_CHANNEL_CLASS_NAME);
    if (!udata->ctrlUdata) {
        luaL_argerror(L, arg, "invalid channel object");
        return NULL;
    }
    return udata;
}

/* ============================================================================================ */

static const luaL_Reg ChannelMethods[] = 
{
    { NULL,       NULL } /* sentinel */
};

static const luaL_Reg ChannelMetaMethods[] = 
{
    { "__gc",       Channel_release  },
    { "__tostring", Channel_toString },

    { NULL,       NULL } /* sentinel */
};

/* ============================================================================================ */

static void setupChannelMeta(lua_State* L)
{                                                /* -> meta */
    lua_pushstring(L, LRTAUDIO_CHANNEL_CLASS_NAME);/* -> meta, className */
    lua_setfield(L, -2, "__metatable");          /* -> meta */

    luaL_setfuncs(L, ChannelMetaMethods, 0);       /* -> meta */
    
    lua_newtable(L);                             /* -> meta, ChannelClass */
    luaL_setfuncs(L, ChannelMethods, 0);           /* -> meta, ChannelClass */
    lua_setfield (L, -2, "__index");             /* -> meta */
//   auproc_set_capi(L, -1, &auproc_capi_impl);
}

/* ============================================================================================ */
} // extern "C"
/* ============================================================================================ */
