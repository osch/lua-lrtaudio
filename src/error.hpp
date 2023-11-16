#ifndef LRTAUDIO_ERROR_HPP
#define LRTAUDIO_ERROR_HPP

#include <stdexcept>
	
#include "util.h"

/* ============================================================================================ */
namespace lrtaudio {
namespace error {
/* ============================================================================================ */

#if LRTAUDIO_NEW_RTAUDIO
    #define LRTAUDIO_CHECK(api, action) { \
        RtAudioErrorType errTp = action; \
        if (errTp != RTAUDIO_NO_ERROR) { \
            throw std::runtime_error(api->getErrorText()); \
        } \
    }
#else
    #define LRTAUDIO_CHECK(api, action) { \
        action; \
    }
#endif

struct handler_data {
    char*  buffer;
    size_t len;
};

void handle_error(handler_data* error_handler_data, const char* msg, size_t msglen);

/* ============================================================================================ */
} } // namespace lrtaudio::error
/* ============================================================================================ */


#endif // LRTAUDIO_ERROR_HPP
