#ifndef LRTAUDIO_ASYNC_DEFINES_H
#define LRTAUDIO_ASYNC_DEFINES_H

/* -------------------------------------------------------------------------------------------- */

#if    defined(LRTAUDIO_ASYNC_USE_WIN32) \
    && (   defined(LRTAUDIO_ASYNC_USE_STDATOMIC) \
        || defined(LRTAUDIO_ASYNC_USE_GNU))
  #error "LRTAUDIO_ASYNC: Invalid compile flag combination"
#endif
#if    defined(LRTAUDIO_ASYNC_USE_STDATOMIC) \
    && (   defined(LRTAUDIO_ASYNC_USE_WIN32) \
        || defined(LRTAUDIO_ASYNC_USE_GNU))
  #error "LRTAUDIO_ASYNC: Invalid compile flag combination"
#endif
#if    defined(LRTAUDIO_ASYNC_USE_GNU) \
    && (   defined(LRTAUDIO_ASYNC_USE_WIN32) \
        || defined(LRTAUDIO_ASYNC_USE_STDATOMIC))
  #error "LRTAUDIO_ASYNC: Invalid compile flag combination"
#endif
 
/* -------------------------------------------------------------------------------------------- */

#if    !defined(LRTAUDIO_ASYNC_USE_WIN32) \
    && !defined(LRTAUDIO_ASYNC_USE_STDATOMIC) \
    && !defined(LRTAUDIO_ASYNC_USE_GNU)

    #if defined(WIN32) || defined(_WIN32)
        #define LRTAUDIO_ASYNC_USE_WIN32
    #elif defined(__GNUC__)
        #define LRTAUDIO_ASYNC_USE_GNU
    #elif __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
        #define LRTAUDIO_ASYNC_USE_STDATOMIC
    #else
        #error "LRTAUDIO_ASYNC: unknown platform"
    #endif
#endif

/* -------------------------------------------------------------------------------------------- */

#if defined(__unix__) || defined(__unix) || (defined (__APPLE__) && defined (__MACH__))
    #include <unistd.h>
#endif

#if    !defined(LRTAUDIO_ASYNC_USE_WINTHREAD) \
    && !defined(LRTAUDIO_ASYNC_USE_PTHREAD) \
    && !defined(LRTAUDIO_ASYNC_USE_STDTHREAD)
    
    #ifdef LRTAUDIO_ASYNC_USE_WIN32
        #define LRTAUDIO_ASYNC_USE_WINTHREAD
    #elif _XOPEN_VERSION >= 600
        #define LRTAUDIO_ASYNC_USE_PTHREAD
    #elif __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
        #define LRTAUDIO_ASYNC_USE_STDTHREAD
    #else
        #define LRTAUDIO_ASYNC_USE_PTHREAD
    #endif
#endif

/* -------------------------------------------------------------------------------------------- */

#if defined(LRTAUDIO_ASYNC_USE_PTHREAD)
    #ifndef _XOPEN_SOURCE
        #define _XOPEN_SOURCE 600 /* must be defined before any other include */
    #endif
    #include <errno.h>
    #include <sys/time.h>
    #include <pthread.h>
#endif
#if defined(LRTAUDIO_ASYNC_USE_WIN32) || defined(LRTAUDIO_ASYNC_USE_WINTHREAD)
    #include <windows.h>
#endif
#if defined(LRTAUDIO_ASYNC_USE_STDATOMIC)
    #include <stdint.h>
    #include <stdatomic.h>
#endif
#if defined(LRTAUDIO_ASYNC_USE_STDTHREAD)
    #include <sys/time.h>
    #include <threads.h>
#endif

/* -------------------------------------------------------------------------------------------- */

#endif /* LRTAUDIO_ASYNC_DEFINES_H */
