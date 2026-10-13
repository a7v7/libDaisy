/** TinyUSB Audio: the Daisy as a USB audio interface (USB Audio Class 2).
 *
 *  The Seed shows up on the computer as "Daisy Audio", with a stereo line
 *  output and a stereo line input, 48 kHz, 24-bit, plus a serial port.
 *  No driver is needed on Windows 10+, macOS or Linux.
 *
 *  - Playback: audio the computer plays to "Daisy Audio" comes out of the
 *    Daisy's audio outputs.
 *  - Recording: the Daisy's audio inputs are recorded by the computer.
 *
 *  The serial port prints a status line once a second (see usb_audio.h):
 *
 *      out: on  fifo  784  under 0  vol -12 dB | in: on  fifo  776  over 0 | fb 10.14
 *
 *  While streaming steadily, under and over should both stay 0. The LED is
 *  on while either direction is streaming.
 *
 *  This is the plain version: tud_task() is polled from the main loop.
 *  Floret_UsbAudio is the same device run by Floret, so Downlink can watch.
 */
#include "daisy_seed.h"
#include "tinyusb/tinyusb_daisy.h"
#include "tusb.h"
#include "usb_audio.h"

using namespace daisy;

static DaisySeed hw;

static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
{
    usb_audio::Process(in, out, size);
}

int main(void)
{
    hw.Init();
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    hw.SetAudioBlockSize(48); // one block per USB frame (1 ms)

    tinyusb::Init();
    hw.StartAudio(AudioCallback);

    uint32_t last_status = System::GetNow();
    while(1)
    {
        tud_task();

        if(System::GetNow() - last_status >= 1000)
        {
            last_status += 1000;
            usb_audio::PrintStatus();
        }

        hw.SetLed(usb_audio::OutStreaming() || usb_audio::InStreaming());
    }
}
