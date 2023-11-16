#ifndef LRTAUDIO_STREAM_HPP
#define LRTAUDIO_STREAM_HPP

#include "util.h"

/* ============================================================================================ */
extern "C" {
/* ============================================================================================ */

struct receiver_capi;
struct receiver_object;
struct receiver_writer;

/* ============================================================================================ */
} // extern "C"
/* ============================================================================================ */

/* ============================================================================================ */
namespace lrtaudio { 
/* ============================================================================================ */

struct ControllerUserData;

/* ============================================================================================ */
namespace stream {
/* ============================================================================================ */

struct Channels
{
    bool isInput;
    int  tableRef;
    int  min;
    int  max;
};

struct ConnectorInfo
{
    bool isChannel;
    bool isProcBuf;
    bool isInput;
    bool isOutput;
    
    ChannelUserData*  channelUdata;
    ProcBufUserData*  procBufUdata;
};

struct ProcReg
{
    void* processorData;
    int  (*processCallback)(uint32_t nframes, void* processorData);
    int  (*sampleRateCallback)(uint32_t nframes, void* processorData);
    int  (*bufferSizeCallback)(uint32_t nframes, void* processorData);
    void (*engineClosedCallback)(void* processorData);
    void (*engineReleasedCallback)(void* processorData);      
    char* processorName;
    uint32_t bufferFrames;
    uint32_t sampleRate;
    bool activated;
    bool outBuffersCleared;
    int  connectorTableRef;
    int  connectorCount;
    ConnectorInfo* connectorInfos;

};

/* ============================================================================================ */
} // namespace stream
/* ============================================================================================ */

struct Stream 
{
    int              streamNameRef;
    stream::Channels inputs;
    stream::Channels outputs;
    lua_Integer      inputDeviceId;
    lua_Integer      outputDeviceId;
    
    uint32_t       bufferFrames;
    uint32_t       sampleRate;
    unsigned int   numberOfBuffers; // TODO ???
    
    Mutex          processMutex;

    const receiver_capi* statusReceiverCapi;
    receiver_object*     statusReceiver;
    receiver_writer*     statusWriter;

    bool           isOpen;
    bool           isRunning;
    AtomicCounter  shutdownReceived;
    AtomicCounter  severeProcessingError;

    stream::ProcReg**  procRegList;
    int                procRegCount;
    stream::ProcReg**  activeProcRegList;
    stream::ProcReg**  confirmedProcRegList;

    uint32_t          processBeginFrameTime;
    void*             currentOutputBuffers;
    void*             currentInputBuffers;
    
    ChannelUserData*  firstChannelUserData;
    ProcBufUserData*  firstProcBufUserData;
};

/* ============================================================================================ */
namespace stream {
/* ============================================================================================ */

int open_stream(lua_State* L, ControllerUserData* udata, 
                uint32_t sampleRate, uint32_t bufferFrames,
                RtAudio::StreamOptions*    options,
                RtAudio::StreamParameters* outParams,
                RtAudio::StreamParameters* inpParams);
                    
void close_stream(ControllerUserData* udata);

void release_stream(lua_State* L, ControllerUserData* udata);

int rtaudio_callback(void* outputBuffer, void* inputBuffer, 
                     unsigned int nFrames, double streamTime, 
                     RtAudioStreamStatus status, void* voidData);

void handle_shutdown(ControllerUserData* udata);

void check_not_closed(lua_State* L, ControllerUserData* udata);

void activate_proc_list_LOCKED(Stream*    stream, 
                               ProcReg**  newList);
                               

/* ============================================================================================ */
} } // namespace lrtaudio::stream
/* ============================================================================================ */

#endif // LRTAUDIO_STREAM_HPP
