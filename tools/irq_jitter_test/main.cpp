/** IRQ jitter test: how much does a competing interrupt delay the audio
 *  callback?
 *
 *  A GPIO goes high for the duration of each audio callback. A second
 *  interrupt, the DAC DMA callback, busy-waits LOAD_US microseconds standing
 *  in for a slow driver interrupt (SD card, display, USB). With libDaisy's
 *  stock priorities both interrupts are at level 0, so audio has to wait for
 *  the DAC callback to finish. With the irq-priorities branch the DAC is at
 *  level 1 and the audio interrupt preempts it.
 *
 *  The DAC callback period (DAC_BLOCK / DAC_RATE = 0.8 ms) is deliberately
 *  not a multiple of the 1 ms audio period, so the two drift past each other
 *  and every overlap phase shows up within a few seconds of capture.
 *
 *  Logic analyzer channels (Seed pins; on a Daisy Pod these are the SPI1
 *  header pins - D1-D6 are the Pod's SD card, so don't use those):
 *      Audio  D7  high while the audio callback runs
 *      Load   D8  high while the DAC DMA callback runs
 *      Mark   D9  toggles once per second from the main loop
 *
 *  The DAC output (D22) carries nothing useful; leave it unconnected.
 */
#include "daisy_seed.h"

using namespace daisy;

static constexpr uint32_t LOAD_US   = 300;   // busy time per DAC callback
static constexpr uint32_t DAC_RATE  = 40000; // DAC sample rate in Hz
static constexpr size_t   DAC_BLOCK = 32;    // samples per DAC callback

static DaisySeed hw;
static GPIO      pin_audio;
static GPIO      pin_load;
static GPIO      pin_mark;

static uint16_t DMA_BUFFER_MEM_SECTION dac_buffer[DAC_BLOCK * 2];

static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
{
    pin_audio.Write(true);
    for(size_t i = 0; i < size; i++)
    {
        out[0][i] = in[0][i];
        out[1][i] = in[1][i];
    }
    pin_audio.Write(false);
}

/* The DAC is only here to generate interrupts, so its buffer is never
 * written. (Also: with a single DAC channel 2, DacHandle hands the callback
 * a null buffer pointer, so writing to out[0] would write near address 0.) */
static void DacCallback(uint16_t** out, size_t size)
{
    (void)out;
    (void)size;
    pin_load.Write(true);
    const uint32_t start = System::GetUs();
    while(System::GetUs() - start < LOAD_US) {}
    pin_load.Write(false);
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(48);

    pin_audio.Init(seed::D7, GPIO::Mode::OUTPUT);
    pin_load.Init(seed::D8, GPIO::Mode::OUTPUT);
    pin_mark.Init(seed::D9, GPIO::Mode::OUTPUT);

    DacHandle::Config dac_cfg;
    dac_cfg.target_samplerate = DAC_RATE;
    dac_cfg.chn               = DacHandle::Channel::TWO;
    dac_cfg.mode              = DacHandle::Mode::DMA;
    dac_cfg.bitdepth          = DacHandle::BitDepth::BITS_12;
    dac_cfg.buff_state        = DacHandle::BufferState::ENABLED;
    hw.dac.Init(dac_cfg);
    hw.dac.Start(dac_buffer, DAC_BLOCK * 2, DacCallback);

    hw.StartAudio(AudioCallback);

    bool     mark      = false;
    uint32_t last_mark = System::GetNow();
    while(1)
    {
        /* Compiler barrier. Without it, a CMake/LTO build of stock libDaisy
         * can drop the audio callback entirely: GCC sees that this loop
         * never reads AudioHandle's (non-volatile) callback pointer and
         * deletes the store in StartAudio(). Fixed on the
         * audio-callback-volatile branch; the barrier keeps this test
         * working on unpatched libDaisy too. */
        __asm volatile("" ::: "memory");

        if(System::GetNow() - last_mark >= 1000)
        {
            last_mark += 1000;
            mark = !mark;
            pin_mark.Write(mark);
        }
    }
}
