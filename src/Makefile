.PHONY: default lrtaudio
default: lrtaudio

BUILD_DATE  := $(shell date "+%Y-%m-%dT%H:%M:%S")

LNX_GCC_RUN := g++ -shared -fPIC -O2 -g  -Werror=return-type
WIN_GCC_RUN := gcc -shared -fPIC -O2
MAC_GCC_RUN := g++ -O2 -bundle -undefined dynamic_lookup -all_load

LNX_COPTS   := 
WIN_COPTS   := -I/mingw64/include/lua5.1 
MAC_COPTS   := -std=c++11 -I/usr/local/include/lua

LNX_LOPTS   := -lrtaudio -g
WIN_LOPTS   := -lkernel32
MAC_LOPTS   := -lrtaudio

LNX_SO_EXT  := so
WIN_SO_EXT  := dll
MAC_SO_EXT  := so

GCC_RUN     :=
SO_EXT      :=
COPTS       :=
LOPTS       :=

# platforms: LNX, WIN, MAC
# (may be set in sandbox.mk)

PLATFORM    := LNX
LUA_VERSION := 5.4

-include sandbox.mk

GCC_RUN       := $(or $(GCC_RUN),       $($(PLATFORM)_GCC_RUN))
SO_EXT        := $(or $(SO_EXT),        $($(PLATFORM)_SO_EXT))
COPTS         := $(or $(COPTS),         $($(PLATFORM)_COPTS))
LOPTS         := $(or $(LOPTS),         $($(PLATFORM)_LOPTS))

lrtaudio:
	@mkdir -p build/lua$(LUA_VERSION)/
	$(GCC_RUN) $(COPTS) \
	    -D LRTAUDIO_VERSION=Makefile"-$(BUILD_DATE)" \
	    main.cpp controller.cpp channel.cpp stream.cpp \
	    procbuf.cpp auproc_capi_impl.cpp \
	    async_util.cpp error.cpp \
	    lrtaudio_compat.c \
	    $(LOPTS) \
	    -o build/lua$(LUA_VERSION)/lrtaudio.$(SO_EXT)
	    

