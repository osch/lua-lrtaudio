#include <vector>
#include <exception>

#include <rtaudio/RtAudio.h>

#include "main.hpp"
#include "controller.hpp"
#include "error.hpp"

#define RECEIVER_CAPI_IMPLEMENT_GET_CAPI 1
#include "receiver_capi.h"

/* ============================================================================================ */
namespace lrtaudio {
/* ============================================================================================ */

int handleException(lua_State* L)
{
    try {
        throw;
    }
    catch (RtAudioError& ex) {
        return luaL_error(L, "error: %s", ex.getMessage().c_str());
    }
    catch (std::exception& ex) {
        return luaL_error(L, "unexpected error: %s", ex.what());
    }
    catch (...) {
        return luaL_error(L, "unknown error");
    }
}

const char* quoteLString(lua_State* L, const char* s, size_t len)
{
    if (s) {
        luaL_Buffer tmp;
        luaL_buffinit(L, &tmp);
        luaL_addchar(&tmp, '"');
        int i;
        for (i = 0; i < len; ++i) {
            char c = s[i];
            if (c == 0) {
                luaL_addstring(&tmp, "\\0");
            } else if (c == '"') {
                luaL_addstring(&tmp, "\\\"");
            } else if (c == '\\') {
                luaL_addstring(&tmp, "\\\\");
            } else {
                luaL_addchar(&tmp, c);
            }
        }
        luaL_addchar(&tmp, '"');
        luaL_pushresult(&tmp);
        return lua_tostring(L, -1);
    } else {
        return lua_pushstring(L, "(nil)");
    }
}

const char* quoteString(lua_State* L, const char* s)
{
    return quoteLString(L, s, (s != NULL) ? strlen(s) : 0);
}

/* ============================================================================================ */

static void logSilent(const char* msg) 
{}
static void logStdOut(const char* msg) {
    fprintf(stdout, "%s\n", msg);
}
static void logStdErr(const char* msg) {
    fprintf(stderr, "%s\n", msg);
}

typedef void (*LogFunc)(const char* msg);

static const char *const builtin_log_options[] = {
    "SILENT",
    "STDOUT",
    "STDERR"
};
static LogFunc builtin_log_functions[] = {
    logSilent,
    logStdOut,
    logStdErr
};

/* ============================================================================================ */

#define LRTAUDIO_LOG_BUFFER_SIZE 2048

typedef struct {
    bool                 isError;
    Lock                 lock;
    AtomicCounter        initStage;
    LogFunc              logFunc;
    const receiver_capi* capi;
    receiver_object*     receiver;
    receiver_writer*     writer;
    char                 logBuffer[LRTAUDIO_LOG_BUFFER_SIZE];
} LogSetting;


static LogSetting errorLog = { true,  0 };
static LogSetting infoLog  = { false, 0 };

static void assureMutexInitialized(LogSetting* s)
{
    if (atomic_get(&s->initStage) != 2) {
        if (atomic_set_if_equal(&s->initStage, 0, 1)) {
            async_lock_init(&s->lock);
            if (s->isError) {
                s->logFunc = logStdErr;
            }
            atomic_set(&s->initStage, 2);
        } 
        else {
            while (atomic_get(&s->initStage) != 2) {
                Mutex waitMutex;
                async_mutex_init(&waitMutex);
                async_mutex_lock(&waitMutex);
                async_mutex_wait_millis(&waitMutex, 1);
                async_mutex_destruct(&waitMutex);
            }
        }
    }

}

/* ============================================================================================ */

static void handleLogError(void* ehdata, const char* msg, size_t msglen)
{
    error::handle_error((error::handler_data*)ehdata, msg, msglen);
}


static void logToReceiver_LOCKED(LogSetting* s, const char* msg)
{
    if (s->receiver) {
        s->capi->addStringToWriter(s->writer, msg, strlen(msg));
        error::handler_data ehdata = {0};
        int rc = s->capi->msgToReceiver(s->receiver, s->writer, false, false, handleLogError, &ehdata);
        if (rc == 1) {
            // receiver closed
            s->capi->freeWriter(s->writer);
            s->capi->releaseReceiver(s->receiver);
            s->writer = NULL;
            s->receiver = NULL;
            s->logFunc  = NULL;
        }
        else if (rc != 0) {
            // other error
            s->capi->clearWriter(s->writer);
        }
        if (ehdata.buffer) {
            fprintf(stderr, "Error while calling lrtaudio %s log callback: %s\n", 
                            s->isError ? "error" : "info", ehdata.buffer);
            free(ehdata.buffer);
        }
    } else if (s->logFunc) {
        s->logFunc(msg);
    }
}

static void logToReceiver1(LogSetting* s, const char* msg)
{
    async_lock_acquire(&s->lock);
    logToReceiver_LOCKED(s, msg);
    async_lock_release(&s->lock);
}

static bool logToReceiver2(LogSetting* s, const char* fmt, va_list args)
{
    async_lock_acquire(&s->lock);
    int size = vsnprintf(s->logBuffer, LRTAUDIO_LOG_BUFFER_SIZE, fmt, args);
    if (size >= LRTAUDIO_LOG_BUFFER_SIZE) {
        strcpy(s->logBuffer + LRTAUDIO_LOG_BUFFER_SIZE - 4, "...");
    }
    logToReceiver_LOCKED(s, s->logBuffer);
    async_lock_release(&s->lock);
    return true;
}

/* ============================================================================================ */

static void errorCallback(const char* msg)
{
    logToReceiver1(&errorLog, msg);
}

static void infoCallback(const char* msg)
{
    logToReceiver1(&infoLog, msg);
}


/* ============================================================================================ */

static int setLogFunction(lua_State* L, LogSetting* s)
{
    const receiver_capi* api = NULL;
    receiver_object*     rcv = NULL;
    LogFunc              logFunc = NULL;
    int arg = 1;
    if (lua_type(L, arg) == LUA_TSTRING) {
        int opt = luaL_checkoption(L, arg, NULL, builtin_log_options);
        logFunc = builtin_log_functions[opt];
    }
    else {
        int versErr = 0;
        api = receiver_get_capi(L, arg, &versErr);
        if (!api) {
            if (versErr) {
                return luaL_argerror(L, arg, "receiver api version mismatch");
            } else {
                return luaL_argerror(L, arg, "expected string or receiver object");
            }
        }
        rcv = api->toReceiver(L, arg);
        if (!rcv) {
            return luaL_argerror(L, arg, "invalid receiver object");
        }
    }
    receiver_writer* wrt = NULL;
    if (rcv) {
        wrt = api->newWriter(1024, 2);
        if (!wrt) {
            return luaL_error(L, "cannot create writer to log receiver");
        }
        api->retainReceiver(rcv);
        logFunc = s->isError ? errorCallback : infoCallback;
    }
    async_lock_acquire(&s->lock);
        if (s->receiver) {
            s->capi->releaseReceiver(s->receiver);
            s->capi->freeWriter(s->writer);
        }
        s->logFunc  = logFunc;
        s->capi     = api;
        s->receiver = rcv;
        s->writer   = wrt;
    async_lock_release(&s->lock);
    return 0;
}


/* ============================================================================================ */

bool log_errorV(const char* fmt, va_list args)
{
    assureMutexInitialized(&errorLog);
    return logToReceiver2(&errorLog, fmt, args);
}

void log_error(const char* fmt, ...)
{
    bool finished;
    do {
        va_list args;
        va_start(args, fmt);
            finished = log_errorV(fmt, args);
        va_end(args);
    } while (!finished);
}

/* ============================================================================================ */

bool log_infoV(const char* fmt, va_list args)
{
    assureMutexInitialized(&infoLog);
    return logToReceiver2(&infoLog, fmt, args);
}

void log_info(const char* fmt, ...)
{
    bool finished;
    do {
        va_list args;
        va_start(args, fmt);
            finished = log_infoV(fmt, args);
        va_end(args);
    } while (!finished);
}


/* ============================================================================================ */
} // namespace lrtaudio
/* ============================================================================================ */

/* ============================================================================================ */
extern "C" {
/* ============================================================================================ */

#ifndef LRTAUDIO_VERSION
    #error LRTAUDIO_VERSION is not defined
#endif 

#define LRTAUDIO_STRINGIFY(x) #x
#define LRTAUDIO_TOSTRING(x) LRTAUDIO_STRINGIFY(x)
#define LRTAUDIO_VERSION_STRING LRTAUDIO_TOSTRING(LRTAUDIO_VERSION)

const char* const LRTAUDIO_MODULE_NAME = "rtaudio";


/* ============================================================================================ */

static int Lrtaudio_getVersion(lua_State* L)
{
    lua_pushstring(L, RtAudio::getVersion().c_str());
    return 1;
}

/* ============================================================================================ */

static int Lrtaudio_getCompiledApi(lua_State* L)
{
    try {
        lua_newtable(L);                          // -> list
        std::vector<RtAudio::Api> apis;
        RtAudio::getCompiledApi(apis);
        for (int i = 0; i < apis.size(); ++i) {
            lua_pushstring(L, RtAudio::getApiName(apis[i]).c_str());  // -> list, apiname
            lua_rawseti(L, -2, i+1);               // -> list
        }
        return 1;
    } catch (...) {
        return lrtaudio::handleException(L);
    }
}

/* ============================================================================================ */

static int Lrtaudio_set_error_log(lua_State* L)
{
    assureMutexInitialized(&lrtaudio::errorLog);
    return setLogFunction(L, &lrtaudio::errorLog);
}

/* ============================================================================================ */

static int Lrtaudio_set_info_log(lua_State* L)
{
    assureMutexInitialized(&lrtaudio::infoLog);
    return setLogFunction(L, &lrtaudio::infoLog);
}

/* ============================================================================================ */

static const luaL_Reg ModuleFunctions[] = 
{
    { "set_error_log",     Lrtaudio_set_error_log  },
    { "set_info_log",      Lrtaudio_set_info_log   },
    { "getRtAudioVersion", Lrtaudio_getVersion  },
    { "getCompiledApi",    Lrtaudio_getCompiledApi  },
    { NULL,                NULL } /* sentinel */
};

/* ============================================================================================ */

DLL_PUBLIC int luaopen_lrtaudio(lua_State* L)
{
    luaL_checkversion(L); /* does nothing if compiled for Lua 5.1 */

    int n      = lua_gettop(L);
    int module = ++n; lua_newtable(L);
    
    lua_pushvalue(L, module);
        luaL_setfuncs(L, ModuleFunctions, 0);
    lua_pop(L, 1);
    
    lua_pushliteral(L, LRTAUDIO_VERSION_STRING);
    lua_setfield(L, module, "_VERSION");
    
    lua_checkstack(L, LUA_MINSTACK);
    
    lrtaudio_controller_init_module(L, module);

    lua_settop(L, module);
    return 1;
}

/* ============================================================================================ */
} // extern "C"
/* ============================================================================================ */
