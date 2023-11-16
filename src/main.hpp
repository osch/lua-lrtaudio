#ifndef LRTAUDIO_MAIN_HPP
#define LRTAUDIO_MAIN_HPP

#include "util.h"

/* ============================================================================================ */
extern "C" {
/* ============================================================================================ */

DLL_PUBLIC int luaopen_lrtaudio(lua_State* L);

/* ============================================================================================ */
} // extern "C"
/* ============================================================================================ */

/* ============================================================================================ */
namespace lrtaudio {
/* ============================================================================================ */

int handleException(lua_State* L);

const char* quoteString(lua_State* L, const char* s);
const char* quoteLString(lua_State* L, const char* s, size_t len);

bool log_errorV(const char* fmt, va_list args);
bool log_infoV(const char* fmt, va_list args);

void log_error(const char* fmt, ...);
void log_info(const char* fmt, ...);

/* ============================================================================================ */
} // namespace lrtaudio
/* ============================================================================================ */

#endif // LRTAUDIO_MAIN_HPP
