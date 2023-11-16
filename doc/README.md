# lrtaudio Documentation

<!-- ---------------------------------------------------------------------------------------- -->
##   Contents
<!-- ---------------------------------------------------------------------------------------- -->

   * [Overview](#overview)
   * [Module Functions](#module-functions)
        * [lrtaudio.getRtAudioVersion()](#lrtaudio_getRtAudioVersion)
        * [lrtaudio.getCompiledApi()](#lrtaudio_getCompiledApi)
        * [lrtaudio.new()](#lrtaudio_new)
   * [Controller Methods](#controller-methods)
        * [controller:getCurrentApi()](#controller_getCurrentApi)
        * [controller:getDeviceCount()](#controller_getDeviceCount)
        * [controller:getDeviceInfo()](#controller_getDeviceInfo)
        * [controller:openStream()](#controller_openStream)
        * [controller:closeStream()](#controller_closeStream)
        * [controller:startStream()](#controller_startStream)
        * [controller:stopStream()](#controller_stopStream)
        * [controller:getStreamSampleRate()](#controller_getStreamSampleRate)
        * [controller:getStreamBufferFrames()](#controller_getStreamBufferFrames)
        * [controller:getStreamLatency()](#controller_getStreamLatency)
        * [controller:getStreamInput()](#controller_getStreamInput)
        * [controller:getStreamOutput()](#controller_getStreamOutput)
        * [controller:newStreamBuffer()](#controller_newStreamBuffer)
   * [Connector Objects](#connector-objects)
   * [Processor Objects](#processor-objects)


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

* <span id="lrtaudio_getRtAudioVersion">**`lrtaudio.getRtAudioVersion()
  `**</span>
  
  Returns the RtAudio version as string.
  
* <span id="lrtaudio_getCompiledApi">**`lrtaudio.getCompiledApi()
  `**</span>
  
  Returns a string list of possible audio API names that can be used
  for creating a new controller object with [lrtaudio.new()](#lrtaudio_new). 
  
  Possible API names for [RtAudio] 5.2.0: *alsa*, *pulse*, *oss*, *jack*, *core*, 
  *wasapi*, *asio*, *ds*.

<!-- ---------------------------------------------------------------------------------------- -->

* <span id="lrtaudio_new">**`lrtaudio.new([apiName])
  `**</span>
  
  Creates a new lrtaudio [controller object](#controller-methods).
  
  * *apiName* - optional string, name of the audio API that is used with 
                this controller. If not given, the default order of use is 
                *jack*, *pulse*, *alsa*, *oss* (Linux systems) and *asio*, *wasapi*, *ds* (Windows systems). 
                For possible values see: [lrtaudio.getCompiledApi()](#lrtaudio_getCompiledApi)
  
<!-- ---------------------------------------------------------------------------------------- -->
##   Controller Methods
<!-- ---------------------------------------------------------------------------------------- -->

Controller objects are created by invoking the function
[lrtaudio.new()](#lrtaudio_new). A controller object can be used to create and
control an audio processing stream of input / output channels on specified devices.

<!-- ---------------------------------------------------------------------------------------- -->

* <span id="controller_getCurrentApi">**`controller:getCurrentApi()
  `** </span>

  Returns the audio API name used with this controller object. For possible values 
  see [lrtaudio.getCompiledApi()](#lrtaudio_getCompiledApi).

<!-- ---------------------------------------------------------------------------------------- -->

* <span id="controller_getDeviceCount">**`controller:getDeviceCount()
  `** </span>

  Returns the number of devices.

<!-- ---------------------------------------------------------------------------------------- -->

* <span id="controller_getDeviceInfo">**`controller:getDeviceInfo([deviceId])
  `** </span>

  Returns device information. 
  
  * *deviceId* - optional integer. If not given, this method returns a list of device
                 information for all possible devices. If given it must be: 
                 1 <= *deviceId* <= [controller:getDeviceCount()](#controller_getDeviceCount)
  
  Device information for one device is given as Lua table with key value pairs. Possible entries
  are:
  
  * *deviceId*            - device ID
  * *probed*              - boolean value, true if the device capabilities were successfully probed. 
  * *name*                - device name string
  * *outputChannels*      - maximum number of output channels supported by device
  * *inputChannels*       - maximum number of input channels supported by device
  * *duplexChannels*      - maximum number of simultaneous input/output channels supported by device. 
  * *isDefaultOutput*     - boolean value, true if this is the default output device.
  * *isDefaultInput*      - boolean value, true if this is the default input device
  * *sampleRates*         - list of supported sample rate integer values
  * *preferredSampleRate* - integer value
  * *nativeFormats*       - list of strings of supported data formats, 
                            possible values: *SINT8*, *SINT16*, *SINT24*, *SINT32*, *FLOAT32*, *FLOAT64*.

<!-- ---------------------------------------------------------------------------------------- -->

* <span id="controller_openStream">**`controller:openStream(params)
  `** </span>

  Opens the audio stream for this controller object with the specified parameters.

  * *params* - lua table with stream parameters.
  
  The parameter *params* may contain the following stream parameters as key value pairs:
  
  * <span id="openStream_streamName">*`streamName`*</span> -  optional string. 
    The streamName parameter can be used to set the client name when using the Jack API. 
    By default, the client name is set to "RtApiJack". However, if you wish to create multiple 
    instances of RtAudio with Jack, each instance must have a unique client name.
    
  * <span id="openStream_inputChannels">*`inputChannels`*</span> -  optional integer. 
    Number of channels used for audio input.
    
  * <span id="openStream_outputChannels">*`outputChannels`*</span> -  optional integer. 
    Number of channels used for audio output.

  * <span id="openStream_inputDevice">*`inputDevice`*</span> -  optional integer. 
    Device ID of the device used for audio input.
    
  * <span id="openStream_outputDevice">*`outputDevice`*</span> -  optional integer. 
    Device ID of the device used for audio output.
    
  * <span id="openStream_sampleRate">*`sampleRate`*</span> -  optional integer. 
    Sample rate to be used for audio input and output. The effective value chosen can be
    obtained by the method [controller:getStreamSampleRate()](#controller_getStreamSampleRate).
    
  * <span id="openStream_bufferFrames">*`bufferFrames`*</span> -  optional integer. 
    Buffer size for an audio processing cylce in number of samples. If not given, a default
    value of 256 is used. If set to 0, [RtAudio] tries to figure out the lowest possible 
    value, but this does not work in all cases. The effective value chosen can be
    obtained by the method [controller:getStreamBufferFrames()](#controller_getStreamBufferFrames).
    
  * <span id="openStream_minimizeLatency">*`minimizeLatency`*</span> -  optional boolean flag. 
    If set to true, [RtAudio] attempts to set stream parameters for lowest possible latency.
    
  * <span id="openStream_hogDevice">*`hogDevice`*</span> -  optional boolean flag. 
    If set to true, [RtAudio] attempts to grab device for exclusive use.
    
  * <span id="openStream_scheduleRealtime">*`scheduleRealtime`*</span> -  optional boolean flag. 
    If set to true, [RtAudio] attempts to select realtime scheduling for audio processing 
    thread.
    
  * <span id="openStream_alsaUseDefault">*`alsaUseDefault`*</span> -  optional boolean flag. 
    If set to true, [RtAudio] uses the "default" PCM device (ALSA only).
    
  At least one input or output channel has to be specified.

<!-- ---------------------------------------------------------------------------------------- -->

* <span id="controller_closeStream">**`controller:closeStream()
  `** </span>
    
  Closes the controller's stream and frees any associated stream memory.  

<!-- ---------------------------------------------------------------------------------------- -->

* <span id="controller_startStream">**`controller:startStream()
  `** </span>
    
  Starts the stream's audio processing.  

<!-- ---------------------------------------------------------------------------------------- -->

* <span id="controller_stopStream">**`controller:stopStream()
  `** </span>
    
  Stops the stream's audio processing.  

<!-- ---------------------------------------------------------------------------------------- -->

* <span id="controller_getStreamSampleRate">**`controller:getStreamSampleRate()
  `** </span>
    
  Returns the actual sample rate in samples per second in use by the stream. On some systems, 
  the sample rate used may be slightly different than that specified in the stream parameter
  [sampleRate](#openStream_sampleRate) set in the call to
  [controller:openStream()](#controller_openStream).

<!-- ---------------------------------------------------------------------------------------- -->

* <span id="controller_getStreamBufferFrames">**`controller:getStreamBufferFrames()
  `** </span>
    
  Returns the buffer size for an audio processing cylce in number of samples. This value may 
  differ from the value of [bufferFrames](#openStream_bufferFrames) set in the call to
  [controller:openStream()](#controller_openStream).

<!-- ---------------------------------------------------------------------------------------- -->

* <span id="controller_getStreamLatency">**`controller:getStreamLatency()
  `** </span>
    
  Returns the internal stream latency in sample frames.

  The stream latency refers to delay in audio input and/or output caused by internal buffering 
  by the audio system and/or hardware. For duplex streams, the returned value will represent 
  the sum of the input and output latencies. If the API does not report latency, the return 
  value will be zero. 

<!-- ---------------------------------------------------------------------------------------- -->

* <span id="controller_getStreamInput">**`controller:getStreamInput([id1[, id2]])
  `** </span>
  
  Returns one or more input channels of the current audio processing stream.
  
  * *id1* - optional integer channel ID of the first channel to be returned,
            default value is 1.
  * *id2* - optional integer channel ID of the last channel to be returned, default
            value is *id1*.
  
  If *id1* and *id2* are not given, all input channels are returned.
  If *id1* is given and *id2* is not given, the input channel with
  *id1* is returned.
  If both *id1* and *id2* are given, all channels between *id1* 
  and *id2* are returned.
  
  The returned channel objects can be used as [connector objects](#connector-objects). They
  become invalid if the audio processing stream is closed.
  
<!-- ---------------------------------------------------------------------------------------- -->

* <span id="controller_getStreamOutput">**`controller:getStreamOutput([id1[, id2]])
  `** </span>
  
  Returns one or more output channels of the current audio processing stream.
  
  * *id1* - optional integer channel ID of the first channel to be returned,
            default value is 1.
  * *id2* - optional integer channel ID of the last channel to be returned, default
            value is *id1*.
  
  If *id1* and *id2* are not given, all output channels are returned.
  If *id1* is given and *id2* is not given, the output channel with
  *id1* is returned.
  If both *id1* and *id2* are given, all channels between *id1* 
  and *id2* are returned.

  The returned channel objects can be used as [connector objects](#connector-objects). They
  become invalid if the audio processing stream is closed.

<!-- ---------------------------------------------------------------------------------------- -->

* <span id="controller_newStreamBuffer">**`controller:newStreamBuffer([type])
  `** </span>
  
  Creates a new stream buffer object which can be used as [connector object](#connector-objects). 

  * *type* - optional string value, must be "AUDIO" or "MIDI". Default value is "AUDIO" 
             if this parameter is not given.

  The returned stream buffer becomes invalid if the audio processing stream is closed.  

<!-- ---------------------------------------------------------------------------------------- -->
##   Connector Objects
<!-- ---------------------------------------------------------------------------------------- -->

Connector objects are either *input* or *output channels* or *stream buffer objects*.
They are used to specify input or output connections of [processor objects](#processor-objects).

  * *Input channel objects* can be obtained by the method 
    [controller:getStreamInput()](#controller_getStreamInput). 
    The number of  input channels has to specified by the parameter
    [inputChannels](#openStream_inputChannels) in the call to [controller:openStream()](#controller_openStream)

  * *Output channel objects* can be obtained by the method 
    [controller:getStreamOutput()](#controller_getStreamOutput). 
    The number of  output channels has to specified by the parameter
    [outputChannels](#openStream_outputChannels) in the call to [controller:openStream()](#controller_openStream)

  * *Stream buffer objects* are only visible inside your Lua application. They can be created
    by the method [controller:newStreamBuffer()](#controller_newStreamBuffer) and are used
    to connect [processor objects](#processor-objects) with each other.

<!-- ---------------------------------------------------------------------------------------- -->
##   Processor Objects
<!-- ---------------------------------------------------------------------------------------- -->

Processor objects are Lua objects for processing realtime audio data. They must be implemented
in C using the [Auproc C API].

Processor objects can be connected to audio or midi data streams using 
[connector objects](#connector-objects).

The [lrtaudio examples](../examples/README.md) are using procesor objects that are provided by the
[lua-auproc](https://github.com/osch/lua-auproc) package.

<!-- ---------------------------------------------------------------------------------------- -->

End of document.

<!-- ---------------------------------------------------------------------------------------- -->

[RtAudio]:         https://github.com/thestk/rtaudio
[Lua]:             https://www.lua.org
[Auproc C API]:    https://github.com/lua-capis/lua-auproc-capi

