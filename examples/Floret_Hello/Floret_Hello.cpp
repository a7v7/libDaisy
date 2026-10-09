/** Floret_Hello: a first Floret application on the Daisy Seed.
 *
 *  Two tasks, highest priority first (row 0 is the highest):
 *
 *      blink - toggles the Seed's LED every 500 ms, from a periodic timer
 *      work  - every 10 ms does 200 us of busy work, standing in for a real
 *              job, so Flight Test has something to show
 *
 *  Audio passes from input to output. The audio callback also drives
 *  Floret's tick: 48 samples at 48 kHz is exactly 1 ms per tick, locked to
 *  the codec's sample clock.
 *
 *  Nothing runs in main() after floret_run(): every piece of work is a
 *  handler that Floret calls when its event arrives, and between events the
 *  CPU sleeps.
 */
#include "daisy_seed.h"
#include "floret/floret.h"

using namespace daisy;

static DaisySeed hw;

//------------------------------------------------------------------------------
//  Task ids: the row of each task in the table below
//------------------------------------------------------------------------------
enum : floret_id_t
{
    TASK_BLINK = 0,
    TASK_WORK  = 1,
};

//------------------------------------------------------------------------------
//  blink: a periodic timer signals BLINK_TICK every 500 ticks (500 ms)
//------------------------------------------------------------------------------
static constexpr uint32_t BLINK_TICK = 1u << 0;
static floret_timer_t     blink_timer;
static bool               led_on;

static void blink_sys(uint32_t events, void*)
{
    if(events & FLORET_SYS_START)
        floret_timer_start(&blink_timer, TASK_BLINK, BLINK_TICK, 500, 500);
}

static void blink_usr(uint32_t events, void*)
{
    if(events & BLINK_TICK)
    {
        led_on = !led_on;
        hw.SetLed(led_on);
    }
}

//------------------------------------------------------------------------------
//  work: every 10 ms, 200 us of busy work
//------------------------------------------------------------------------------
static constexpr uint32_t WORK_TICK = 1u << 0;
static floret_timer_t     work_timer;

static void work_sys(uint32_t events, void*)
{
    if(events & FLORET_SYS_START)
        floret_timer_start(&work_timer, TASK_WORK, WORK_TICK, 10, 10);
}

static void work_usr(uint32_t events, void*)
{
    if(events & WORK_TICK)
    {
        const uint32_t start = System::GetUs();
        while(System::GetUs() - start < 200) {}
    }
}

//------------------------------------------------------------------------------
//  The task table: name, init, isr, sys, usr, arg
//------------------------------------------------------------------------------
static const floret_task_t tasks[] = {
    {"blink", nullptr, nullptr, blink_sys, blink_usr, nullptr},
    {"work", nullptr, nullptr, work_sys, work_usr, nullptr},
};

//------------------------------------------------------------------------------
//  Audio: passthrough, plus Floret's tick
//------------------------------------------------------------------------------
static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
{
    for(size_t i = 0; i < size; i++)
    {
        out[0][i] = in[0][i];
        out[1][i] = in[1][i];
    }
    floret_tick();
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(48); // one tick per millisecond

    floret_init(tasks, sizeof(tasks) / sizeof(tasks[0]));
    floret_start();
    hw.StartAudio(AudioCallback);
    floret_run();
}
