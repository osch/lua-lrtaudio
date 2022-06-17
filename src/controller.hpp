#ifndef LRTAUDIO_CONTROLLER_HPP
#define LRTAUDIO_CONTROLLER_HPP

/* ============================================================================================ */
extern "C" {
/* ============================================================================================ */

#include "util.h"

int lrtaudio_controller_init_module(lua_State* L, int module);

struct receiver_capi;
struct receiver_object;

/* ============================================================================================ */
} // extern "C"
/* ============================================================================================ */

class RtAudio;

extern const char* const LRTAUDIO_CONTROLLER_CLASS_NAME;

/* ============================================================================================ */
namespace lrtaudio {
/* ============================================================================================ */

struct ChannelUserData;
struct ProcBufUserData;
struct Stream;

struct ControllerUserData
{
    const char*          className;
    RtAudio*             api;
    const receiver_capi* statusReceiverCapi;
    receiver_object*     statusReceiver;
    
    Stream*              stream;
    bool                 isStreamOpen;
};

/* ============================================================================================ */
} // namespace lrtaudio
/* ============================================================================================ */

#endif // LRTAUDIO_CONTROLLER_HPP
