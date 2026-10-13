/** Floret_UsbAudio: the Daisy as a USB audio interface, run by Floret.
 *
 *  The same USB Audio Class 2 device as TinyUSB_Audio ("Daisy Audio",
 *  stereo line out and line in, 48 kHz, 24-bit, plus a serial port), but
 *  nothing is polled: TinyUSB's interrupt signals a Floret task when it has
 *  work, and the CPU sleeps the rest of the time. Downlink can watch it.
 *
 *  Tasks, highest priority first (row 0 is the highest):
 *      usb     runs tud_task() when the USB interrupt queues an event.
 *              While audio streams, that's a few times every millisecond:
 *              each USB frame completes a playback, recording and feedback
 *              transfer.
 *      status  every 100 ms: the LED (on while streaming); every second,
 *              the status line on the serial port (see usb_audio.h)
 *
 *  The audio callback moves the audio (usb_audio::Process) and drives
 *  Floret's tick (48 samples at 48 kHz = 1 ms).
 */
#include "daisy_seed.h"
#include "floret/floret.h"
#include "tinyusb/tinyusb_daisy.h"
#include "tusb.h"
#include "usb_audio.h"

using namespace daisy;

static DaisySeed hw;

enum : floret_id_t
{
    TASK_USB    = 0,
    TASK_STATUS = 1,
};

//------------------------------------------------------------------------------
//  usb: TinyUSB's device task, run only when there's a USB event
//------------------------------------------------------------------------------
static constexpr uint32_t USB_EVENT = 1u << 0;

static void usb_isr(uint32_t events, void*)
{
    if(events & USB_EVENT)
        tud_task(); // handles every queued event, then returns
}

// TinyUSB calls this whenever it queues an event for tud_task(), from the
// USB interrupt or from thread context. floret_signal() is safe in both.
extern "C" void tud_event_hook_cb(uint8_t rhport, uint32_t eventid, bool in_isr)
{
    (void)rhport;
    (void)eventid;
    (void)in_isr;
    floret_signal(TASK_USB, FLORET_ISR, USB_EVENT);
}

//------------------------------------------------------------------------------
//  status: LED every 100 ms, status line every second
//------------------------------------------------------------------------------
static constexpr uint32_t STATUS_TICK = 1u << 0;
static floret_timer_t     status_timer;
static uint32_t           status_count;

static void status_sys(uint32_t events, void*)
{
    if(events & FLORET_SYS_START)
        floret_timer_start(&status_timer, TASK_STATUS, STATUS_TICK, 100, 100);
}

static void status_usr(uint32_t events, void*)
{
    if(!(events & STATUS_TICK))
        return;
    hw.SetLed(usb_audio::OutStreaming() || usb_audio::InStreaming());
    if(++status_count % 10 == 0)
        usb_audio::PrintStatus();
}

//------------------------------------------------------------------------------
//  Task table: name, init, isr, sys, usr, arg
//------------------------------------------------------------------------------
static const floret_task_t tasks[] = {
    {"usb", nullptr, usb_isr, nullptr, nullptr, nullptr},
    {"status", nullptr, nullptr, status_sys, status_usr, nullptr},
};

//------------------------------------------------------------------------------
//  Audio: USB audio in and out, and Floret's tick
//------------------------------------------------------------------------------
static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
{
    usb_audio::Process(in, out, size);
    floret_tick();
}

int main(void)
{
    hw.Init();
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    hw.SetAudioBlockSize(48); // one tick and one USB frame per millisecond

    // The task table first: TinyUSB's interrupt may signal the usb task as
    // soon as tinyusb::Init() enables it. Events wait until floret_run().
    floret_init(tasks, sizeof(tasks) / sizeof(tasks[0]));
    tinyusb::Init();
    floret_start();
    hw.StartAudio(AudioCallback);
    floret_run();
}
