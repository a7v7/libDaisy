/** usb_audio: moves audio between the Daisy's codec and USB (UAC2).
 *  See usb_audio.h.
 */
#include "usb_audio.h"

#include <stdio.h>

#include "tusb.h"
#include "usb_descriptors.h"

using namespace daisy;

namespace
{
constexpr uint32_t kSampleRate = DAISY_UAC2_SAMPLE_RATE;
constexpr size_t   kChannels   = DAISY_UAC2_N_CHANNELS;
constexpr size_t   kFrameBytes = kChannels * DAISY_UAC2_BYTES_PER_SAMPLE; // 8

// One audio block is one USB frame: 48 frames, 1 ms.
constexpr size_t kBlockFrames = kSampleRate / 1000;
constexpr size_t kBlockBytes  = kBlockFrames * kFrameBytes; // 384

// Playback starts once this much is buffered, about 2 ms. It matches the
// level TinyUSB's feedback aims for (half the FIFO).
constexpr size_t kPlaybackStartBytes
    = CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ / 2 / kFrameBytes * kFrameBytes;

// Set in thread context (TinyUSB callbacks), read in the audio interrupt.
volatile bool out_streaming = false; // host is sending playback
volatile bool in_streaming  = false; // host is reading recording

// Running totals, written only in the audio interrupt.
volatile uint32_t underruns = 0;
volatile uint32_t overruns  = 0;

//--------------------------------------------------------------------+
// Sample conversion. USB carries 24-bit samples left-justified in 32 bits.
//--------------------------------------------------------------------+
inline float UsbToFloat(int32_t s)
{
    return (float)(s >> 8) * (1.0f / 8388608.0f);
}

inline int32_t FloatToUsb(float x)
{
    if(x > 1.0f)
        x = 1.0f;
    else if(x < -1.0f)
        x = -1.0f;
    return (int32_t)(x * 8388607.0f) * 256;
}
} // namespace

namespace usb_audio
{
void Process(AudioHandle::InputBuffer  in,
             AudioHandle::OutputBuffer out,
             size_t                    size)
{
    static int32_t usb[kBlockFrames * kChannels];
    static bool    playing = false; // enough buffered to play from

    if(size > kBlockFrames)
        size = kBlockFrames;

    // Playback: USB -> codec
    size_t frames = 0;
    if(out_streaming)
    {
        const size_t available = tud_audio_available();
        if(!playing && available >= kPlaybackStartBytes)
            playing = true;
        if(playing)
        {
            if(available >= size * kFrameBytes)
                frames = size;
            else
            {
                // Ran dry: play what's there, then wait to refill to the
                // start level rather than glitching on every block.
                frames    = available / kFrameBytes;
                playing   = false;
                underruns = underruns + 1;
            }
            tud_audio_read(usb, (uint16_t)(frames * kFrameBytes));
        }
    }
    else
    {
        playing = false;
    }

    for(size_t i = 0; i < size; i++)
    {
        if(i < frames)
        {
            out[0][i] = UsbToFloat(usb[2 * i]);
            out[1][i] = UsbToFloat(usb[2 * i + 1]);
        }
        else
        {
            out[0][i] = 0.0f;
            out[1][i] = 0.0f;
        }
    }

    // Recording: codec -> USB
    if(in_streaming)
    {
        for(size_t i = 0; i < size; i++)
        {
            usb[2 * i]     = FloatToUsb(in[0][i]);
            usb[2 * i + 1] = FloatToUsb(in[1][i]);
        }
        const uint16_t bytes = (uint16_t)(size * kFrameBytes);
        if(tud_audio_write(usb, bytes) < bytes)
            overruns = overruns + 1;
    }
}

bool OutStreaming()
{
    return out_streaming;
}

bool InStreaming()
{
    return in_streaming;
}

uint32_t Underruns()
{
    return underruns;
}

uint32_t Overruns()
{
    return overruns;
}

void PrintStatus()
{
    static uint32_t last_under = 0;
    static uint32_t last_over  = 0;
    const uint32_t  under      = underruns;
    const uint32_t  over       = overruns;

    if(tud_cdc_connected())
    {
        char line[100];
        snprintf(line,
                 sizeof(line),
                 "out: %s fifo %4u  under %lu | in: %s fifo %4u  over %lu\r\n",
                 out_streaming ? "on " : "off",
                 (unsigned)tud_audio_available(),
                 (unsigned long)(under - last_under),
                 in_streaming ? "on " : "off",
                 (unsigned)tu_fifo_count(tud_audio_get_ep_in_ff()),
                 (unsigned long)(over - last_over));
        tud_cdc_write_str(line);
        tud_cdc_write_flush();
    }
    last_under = under;
    last_over  = over;
}

} // namespace usb_audio

//--------------------------------------------------------------------+
// UAC2 class callbacks (called from tud_task())
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

    // The host starts a stream (alt 1). TinyUSB has already cleared the
    // stream's FIFO, so the audio interrupt starts from empty.
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

    // The host stops a stream (alt 0). TinyUSB clears the FIFO just before
    // this, so an audio interrupt landing in between can play one block of
    // stale data as the stream ends. That's inaudible, so it's left as is.
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

    // Playback feedback: TinyUSB tells the host to speed up or slow down so
    // that the playback FIFO stays half full. Process() drains it at the
    // codec's rate, so the host ends up sending at the codec's rate.
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
