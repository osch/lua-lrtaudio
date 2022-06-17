# lrtaudio Documentation

<!-- ---------------------------------------------------------------------------------------- -->
##   Contents
<!-- ---------------------------------------------------------------------------------------- -->

   * [Overview](#overview)
   * [Module Functions](#module-functions)
        * [lrtaudio.getCompiledApi()](#lrtaudio_getCompiledApi)
        * [lrtaudio.new()](#lrtaudio_new)
   * [Controller Methods](#controller-methods)
        * [controller:getCurrentApi()](#controller_getCurrentApi)
        * [controller:getDeviceCount()](#controller_getDeviceCount)
        * [controller:getDeviceInfo()](#controller_getDeviceInfo)
   * [Connector Objects](#connector-objects)
   * [Processor Objects](#processor-objects)

TODO

<!-- ---------------------------------------------------------------------------------------- -->
##   Overview
<!-- ---------------------------------------------------------------------------------------- -->
   
*lrtaudio* is a Lua binding for [RtAudio].

This binding enables [Lua] scripting code to open [RtAudio] streams and to connect them with Lua 
audio processor objects for realtime processing. 
Realtime audio processing of [Lua processor objects](./README.md#processor-objects) 
has to be implemented in native C code.

<!-- ---------------------------------------------------------------------------------------- -->
##   Module Functions
<!-- ---------------------------------------------------------------------------------------- -->

* <a id="lrtaudio_getCompiledApi">**`  lrtaudio.getCompiledApi()
  `**</a>
  
  Returns a string list of possible audio API names that can be used
  for creating a new controller object with [lrtaudio.new()](#lrtaudio_new). 
  
  Possible API names for [RtAudio] 5.2.0: *alsa*, *pulse*, *oss*, *jack*, *core*, 
  *wasapi*, *asio*, *ds*.

* <a id="lrtaudio_new">**`  lrtaudio.new([apiName])
  `**</a>
  
  Creates a new lrtaudio controller object.
  
  * *apiName* - optional string, name of the audio API that is used with 
                this controller. If not given, the default order of use is 
                *jack*, *pulse*, *alsa*, *oss* (Linux systems) and *asio*, *wasapi*, *ds* (Windows systems). 
                For possible values see: [lrtaudio.getCompiledApi()](#lrtaudio_getCompiledApi)
  
  TODO
  
<!-- ---------------------------------------------------------------------------------------- -->
##   Controller Methods
<!-- ---------------------------------------------------------------------------------------- -->

Controller objects are created by invoking the function [lrtaudio.new()](#lrtaudio_new).

* <a id="controller_getCurrentApi">**`      controller:getCurrentApi()
  `** </a>

  Returns the audio API name used with this controller object. For possible values 
  see [lrtaudio.getCompiledApi()](#lrtaudio_getCompiledApi).


* <a id="controller_getDeviceCount">**`     controller:getDeviceCount()
  `** </a>

  Returns the number of devices.

* <a id="controller_getDeviceInfo">**`      controller:getDeviceInfo([deviceId])
  `** </a>

  Returns device information. 
  
  * *deviceId* - optional integer. If not given, this method returns a list of device
                 information for all possible devices. If given it must be: 
                 1 <= *deviceId* <= [controller:getDeviceCount()](#controller_getDeviceCount)
  
  Device information for one device is given as Lua table with key value pairs. Possible entries
  are:
  
  * *deviceId*            - device ID
  * *probed*              - boolean value, true if the device capabilities were successfully probed. 
  * *name*                - device name string
  * *outputChannels*      - maximum number of   output channels supported by device
  * *inputChannels*       - maximum number of input channels supported by device
  * *duplexChannels*      - maximum number of simultaneous input/output channels supported by device. 
  * *isDefaultOutput*     - boolean value, true if this is the default output device.
  * *isDefaultInput*      - boolean value, true if this is the default input device
  * *sampleRates*         - list of supported sample rate integer values
  * *preferredSampleRate* - integer value
  * *nativeFormats*       - list of strings of supported data formats, 
                            possible values: *SINT8*, *SINT16*, *SINT24*, *SINT32*, *FLOAT32*, *FLOAT64*.

<!-- ---------------------------------------------------------------------------------------- -->
##   Connector Objects
<!-- ---------------------------------------------------------------------------------------- -->

TODO

<!-- ---------------------------------------------------------------------------------------- -->
##   Processor Objects
<!-- ---------------------------------------------------------------------------------------- -->

Processor objects are Lua objects for processing realtime audio data. They must be implemented
in C using the [Auproc C API].

Processor objects can be connected to audio or midi data streams using 
[connector objects](#connector-objects).

The [lrtaudio examples](../examples) are using procesor objects that are provided by the
[lua-auproc](https://github.com/osch/lua-auproc) package.

<!-- ---------------------------------------------------------------------------------------- -->

TODO

End of document.

<!-- ---------------------------------------------------------------------------------------- -->

[RtAudio]:         https://github.com/thestk/rtaudio
[Lua]:             https://www.lua.org
[Auproc C API]:    https://github.com/lua-capis/lua-auproc-capi

