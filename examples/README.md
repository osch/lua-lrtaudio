# lrtaudio Examples

<!-- ---------------------------------------------------------------------------------------- -->

Most of the examples are using procesor objects that are provided by the
[lua-auproc](https://github.com/osch/lua-auproc) package.

<!-- ---------------------------------------------------------------------------------------- -->

   * [`example01.lua`](./example01.lua)
     
     This example lists all compiled [RtAudio] APIs and for each API it displays infos for 
     all available devices. 
       
<!-- ---------------------------------------------------------------------------------------- -->

   * [`example02.lua`](./example02.lua)
     
     This examples plays a sinus tone for 4 seconds using
     an [Auproc audio sender object](https://github.com/osch/lua-auproc/blob/master/doc/README.md#auproc_new_audio_sender).
       
<!-- ---------------------------------------------------------------------------------------- -->

   * [`example03.lua`](./example03.lua)
     
     This example generates audio samples from Lua script code on the main thread and lets the
     user interactively modify oscillator parameters.
     This example demonstrates the usage of 
     [Auproc audio sender object](https://github.com/osch/lua-auproc/blob/master/doc/README.md#auproc_new_audio_sender)
     and
     [Auproc audio mixer object](https://github.com/osch/lua-auproc/blob/master/doc/README.md#auproc_new_audio_mixer).
       
<!-- ---------------------------------------------------------------------------------------- -->

[RtAudio]:  https://github.com/thestk/rtaudio
