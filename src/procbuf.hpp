#ifndef LRTAUDIO_PROCBUF_HPP
#define LRTAUDIO_PROCBUF_HPP

#include "util.h"
#include "auproc_capi.h"

extern const char* const LRTAUDIO_PROCBUF_CLASS_NAME;

/* ============================================================================================ */
namespace lrtaudio {
/* ============================================================================================ */

struct ControllerUserData;

struct ProcBufUserData
{
    const char*          className;
    ControllerUserData*  ctrlUdata;
    bool                 isMidi;
    bool                 isAudio;

    int              procUsageCounter;
    
    int              inpUsageCounter;
    int              outUsageCounter;
    
    int              inpActiveCounter;
    int              outActiveCounter;
    
    unsigned char*       bufferData;
    size_t               bufferLength;

    uint32_t           midiEventCount;
    auproc_midi_event* midiEventsBegin;
    auproc_midi_event* midiEventsEnd;
    unsigned char*     midiDataBegin;
    unsigned char*     midiDataEnd;
    
    ProcBufUserData**    prevNextProcBufUserData;
    ProcBufUserData*     nextProcBufUserData;
};

/* ============================================================================================ */
namespace procbuf {
/* ============================================================================================ */

ProcBufUserData* push_new_procbuf(lua_State* L, ControllerUserData* ctrlUdata, bool isMidi);

void release_procbuf(lua_State* L, ProcBufUserData* udata);

void clear_midi_events(ProcBufUserData* udata);

/* ============================================================================================ */
} } // namespace lrtaudio::procbuf
/* ============================================================================================ */


/* ============================================================================================ */
extern "C" {
/* ============================================================================================ */

    void lrtaudio_procbuf_clear_midi_buffer(auproc_midibuf* midibuf);

    uint32_t lrtaudio_procbuf_get_midi_event_count(auproc_midibuf* midibuf);

    int lrtaudio_procbuf_get_midi_event(auproc_midi_event* event,
                                        auproc_midibuf*    midibuf,
                                        uint32_t           event_index);

    unsigned char* lrtaudio_procbuf_reserve_midi_event(auproc_midibuf*  midibuf,
                                                       uint32_t         time,
                                                       size_t           data_size);

/* ============================================================================================ */
} // extern "C"
/* ============================================================================================ */

#endif // LRTAUDIO_PROCBUF_HPP
