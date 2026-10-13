/** usb_audio: moves audio between the Daisy's codec and USB (UAC2).
 *
 *  Works with this folder's tusb_config.h and usb_descriptors.{h,c}: a
 *  stereo line output and line input, 48 kHz, 24-bit. It provides
 *  TinyUSB's audio class callbacks (sample rate, stream start/stop,
 *  feedback), so the app doesn't need to.
 *
 *  Use:
 *    - audio block size 48 at 48 kHz (one block per USB frame, 1 ms)
 *    - call usb_audio::Process() from the audio callback
 *    - keep tud_task() running (main loop or a Floret task)
 *
 *  Clocks: the codec's sample clock sets the pace in both directions.
 *  Playback is asynchronous: TinyUSB's feedback endpoint tells the host to
 *  speed up or slow down so its FIFO here stays about half full. Recording
 *  uses TinyUSB's flow control, which sends 47, 48 or 49 frames per USB
 *  frame as needed.
 *
 *  Contexts: Process() runs in the audio interrupt and moves audio between
 *  the codec and TinyUSB's FIFOs. Each FIFO has one writer and one reader
 *  (the USB interrupt on one side, the audio interrupt on the other), which
 *  TinyUSB's FIFO supports without locking. The other functions, and the
 *  tud_audio_*_cb callbacks (called from tud_task()), are thread context.
 */
#pragma once
#ifndef USB_AUDIO_H
#define USB_AUDIO_H

#include "daisy_seed.h"

namespace usb_audio
{
/** Plays one block of USB audio to `out` and sends one block of `in` to
 *  USB. Call from the audio callback with size 48 (1 ms at 48 kHz). */
void Process(daisy::AudioHandle::InputBuffer  in,
             daisy::AudioHandle::OutputBuffer out,
             size_t                           size);

/** Whether the host is sending playback audio / reading recording audio. */
bool OutStreaming();
bool InStreaming();

/** Running totals, counted in the audio interrupt:
 *  underruns - blocks that found too little playback audio waiting (gaps)
 *  overruns  - blocks of recording audio that didn't fit because the host
 *              wasn't reading fast enough */
uint32_t Underruns();
uint32_t Overruns();

/** Prints a status line to the CDC port, if a terminal has it open:
 *      out: on  fifo  784  under 0 | in: on  fifo  776  over 0
 *  fifo is bytes waiting in TinyUSB's FIFO (1 ms of audio is 384); under
 *  and over count since the previous call. Call about once a second. */
void PrintStatus();

} // namespace usb_audio

#endif
