/** TinyUSB Audio: the Daisy as a USB Audio Class 2 device (stage 2).
 *
 *  The Seed shows up on the computer as "Daisy Audio", with a stereo line
 *  output and a stereo line input, 48 kHz, 24-bit, plus a serial port.
 *  No driver is needed on Windows 10+, macOS or Linux.
 *
 *  This stage doesn't touch the codec yet: it loops USB audio back. Whatever
 *  the computer plays to the Daisy comes back on the Daisy's input, so you
 *  can play a file and record it in a DAW (Audacity, Reaper, ...).
 *
 *  The serial port prints a status line once a second:
 *
 *      out: on  fifo  768  short 0 | in: on  fifo  776
 *
 *  - out/in: whether the host is streaming in that direction
 *  - fifo:   bytes waiting in TinyUSB's buffer (one ms of audio is 384)
 *  - short:  ms in the last second where less than 1 ms of playback audio
 *            was waiting. While streaming steadily this should stay 0; each
 *            one is a gap you'd hear.
 *
 *  The LED is on while either direction is streaming.
 *
 *  Everything here runs in the main loop: TinyUSB calls the tud_audio_*_cb
 *  callbacks from tud_task(), never from the interrupt.
 */
#include <stdio.h>

#include "daisy_seed.h"
#include "tinyusb/tinyusb_daisy.h"
#include "tusb.h"
#include "usb_descriptors.h"

using namespace daisy;

static DaisySeed hw;

static constexpr uint32_t kSampleRate = DAISY_UAC2_SAMPLE_RATE;
static constexpr size_t   kFrameBytes
    = DAISY_UAC2_N_CHANNELS * DAISY_UAC2_BYTES_PER_SAMPLE;      // 8
static constexpr size_t kFramesPerMs = kSampleRate / 1000;     // 48
static constexpr size_t kBytesPerMs  = kFramesPerMs * kFrameBytes; // 384

static bool     out_streaming = false; // host is sending playback audio
static bool     in_streaming  = false; // host is reading recording audio
static uint32_t short_reads   = 0;

//--------------------------------------------------------------------+
// Audio loopback, run once per millisecond
//--------------------------------------------------------------------+
static void LoopbackOneMs()
{
    static int32_t buf[kFramesPerMs * DAISY_UAC2_N_CHANNELS];
    size_t         n = 0;

    if(out_streaming)
    {
        // Read whole frames only, at most 1 ms
        n = tud_audio_available();
        n = (n > kBytesPerMs ? kBytesPerMs : n) / kFrameBytes * kFrameBytes;
        if(n < kBytesPerMs)
            short_reads++;
        tud_audio_read(buf, (uint16_t)n);
    }
    memset(reinterpret_cast<uint8_t*>(buf) + n, 0, kBytesPerMs - n);

    if(in_streaming)
        tud_audio_write(buf, kBytesPerMs);
}

static void PrintStatus()
{
    if(!tud_cdc_connected())
        return;
    char line[96];
    snprintf(line,
             sizeof(line),
             "out: %s fifo %4u  short %lu | in: %s fifo %4u\r\n",
             out_streaming ? "on " : "off",
             (unsigned)tud_audio_available(),
             (unsigned long)short_reads,
             in_streaming ? "on " : "off",
             (unsigned)tu_fifo_count(tud_audio_get_ep_in_ff()));
    tud_cdc_write_str(line);
    tud_cdc_write_flush();
}

int main(void)
{
    hw.Init();
    tinyusb::Init();

    uint32_t last_ms     = System::GetNow();
    uint32_t last_status = last_ms;
    while(1)
    {
        tud_task();

        // One loopback step per elapsed millisecond (of the Daisy's clock)
        const uint32_t now = System::GetNow();
        while(last_ms != now)
        {
            last_ms++;
            LoopbackOneMs();
        }

        if(now - last_status >= 1000)
        {
            last_status += 1000;
            PrintStatus();
            short_reads = 0;
        }

        hw.SetLed(out_streaming || in_streaming);
    }
}

//--------------------------------------------------------------------+
// UAC2 class callbacks
//--------------------------------------------------------------------+
extern "C"
{
    // The host reads the clock's sample rate, its range, and whether it's
    // valid. The clock is fixed at 48 kHz.
    bool tud_audio_get_req_entity_cb(uint8_t                       rhport,
                                     tusb_control_request_t const* p_request)
    {
        const uint8_t entity = TU_U16_HIGH(p_request->wIndex);
        const uint8_t ctrl   = TU_U16_HIGH(p_request->wValue);
        if(entity != UAC2_ENTITY_CLOCK)
            return false;

        if(ctrl == AUDIO20_CS_CTRL_SAM_FREQ)
        {
            if(p_request->bRequest == AUDIO20_CS_REQ_CUR)
            {
                audio20_control_cur_4_t cur = {(int32_t)kSampleRate};
                return tud_audio_buffer_and_schedule_control_xfer(
                    rhport, p_request, &cur, sizeof(cur));
            }
            if(p_request->bRequest == AUDIO20_CS_REQ_RANGE)
            {
                audio20_control_range_4_n_t(1) range = {};
                range.wNumSubRanges                  = 1;
                range.subrange[0].bMin               = (int32_t)kSampleRate;
                range.subrange[0].bMax               = (int32_t)kSampleRate;
                range.subrange[0].bRes               = 0;
                return tud_audio_buffer_and_schedule_control_xfer(
                    rhport, p_request, &range, sizeof(range));
            }
        }
        else if(ctrl == AUDIO20_CS_CTRL_CLK_VALID
                && p_request->bRequest == AUDIO20_CS_REQ_CUR)
        {
            audio20_control_cur_1_t valid = {1};
            return tud_audio_buffer_and_schedule_control_xfer(
                rhport, p_request, &valid, sizeof(valid));
        }
        return false;
    }

    // The host may set the sample rate. Only 48 kHz is accepted.
    bool tud_audio_set_req_entity_cb(uint8_t                       rhport,
                                     tusb_control_request_t const* p_request,
                                     uint8_t*                      buf)
    {
        (void)rhport;
        const uint8_t entity = TU_U16_HIGH(p_request->wIndex);
        const uint8_t ctrl   = TU_U16_HIGH(p_request->wValue);
        if(entity == UAC2_ENTITY_CLOCK && ctrl == AUDIO20_CS_CTRL_SAM_FREQ
           && p_request->bRequest == AUDIO20_CS_REQ_CUR
           && p_request->wLength == sizeof(audio20_control_cur_4_t))
        {
            const int32_t rate
                = ((audio20_control_cur_4_t const*)buf)->bCur;
            return rate == (int32_t)kSampleRate;
        }
        return false;
    }

    // The host starts (alt 1) or stops (alt 0) a stream.
    bool tud_audio_set_itf_cb(uint8_t                       rhport,
                              tusb_control_request_t const* p_request)
    {
        (void)rhport;
        const uint8_t itf = tu_u16_low(p_request->wIndex);
        const uint8_t alt = tu_u16_low(p_request->wValue);
        if(itf == ITF_NUM_AUDIO_STREAMING_OUT)
            out_streaming = alt != 0;
        else if(itf == ITF_NUM_AUDIO_STREAMING_IN)
            in_streaming = alt != 0;
        return true;
    }

    bool tud_audio_set_itf_close_ep_cb(uint8_t                       rhport,
                                       tusb_control_request_t const* p_request)
    {
        (void)rhport;
        const uint8_t itf = tu_u16_low(p_request->wIndex);
        if(itf == ITF_NUM_AUDIO_STREAMING_OUT)
            out_streaming = false;
        else if(itf == ITF_NUM_AUDIO_STREAMING_IN)
            in_streaming = false;
        return true;
    }

    // Playback feedback: TinyUSB tells the host to speed up or slow down
    // so that its playback FIFO stays half full. The app reads exactly
    // 1 ms of audio per ms of the Daisy's clock, so the host ends up
    // sending at the Daisy's rate.
    void tud_audio_feedback_params_cb(uint8_t                  func_id,
                                      uint8_t                  alt_itf,
                                      audio_feedback_params_t* feedback_param)
    {
        (void)func_id;
        (void)alt_itf;
        feedback_param->method      = AUDIO_FEEDBACK_METHOD_FIFO_COUNT;
        feedback_param->sample_freq = kSampleRate;
    }

    void tud_umount_cb(void)
    {
        out_streaming = false;
        in_streaming  = false;
    }
}
