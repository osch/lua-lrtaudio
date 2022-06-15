#ifndef LRTAUDIO_CHANNEL_HPP
#define LRTAUDIO_CHANNEL_HPP

/* ============================================================================================ */
extern "C" {
/* ============================================================================================ */

#include "util.h"
#include "auproc_capi.h"

/* ============================================================================================ */
} // extern "C"
/* ============================================================================================ */

extern const char* const LRTAUDIO_CHANNEL_CLASS_NAME;

/* ============================================================================================ */
namespace lrtaudio {
/* ============================================================================================ */

struct ControllerUserData;

struct ChannelUserData
{
    const char*          className;
    ControllerUserData*  ctrlUdata;
    bool                 isInput;
    int                  index;

    int                  procUsageCounter;

    ChannelUserData**  prevNextChannelUserData;
    ChannelUserData*   nextChannelUserData;
};

/* ============================================================================================ */
namespace channel {
/* ============================================================================================ */

ChannelUserData* push_new_channel(lua_State* L, ControllerUserData* ctrlUdata, int index, bool isInput);

void release_channel(lua_State* L, ChannelUserData* udata);

extern const auproc_audiometh audio_methods;

/* ============================================================================================ */
} } // namespace lrtaudio::channel
/* ============================================================================================ */

#endif // LRTAUDIO_CHANNEL_HPP
