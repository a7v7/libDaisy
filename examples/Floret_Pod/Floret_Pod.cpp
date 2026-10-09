/** Floret_Pod: a Floret application to play with while Downlink watches.
 *
 *  Controls
 *      Encoder turn    work task load, 0..80 % in 2 % steps
 *      Encoder press   one deliberate 5 ms "hiccup" (a single long dispatch)
 *      Knob 1          extra work inside the audio callback, 0..500 us per
 *                      1 ms block (shows in Downlink's "irq + kernel" lane)
 *      Knob 2          how often the work task runs, every 5..50 ms
 *      LED 1           CPU load, green -> yellow -> red, computed here from
 *                      Floret's own telemetry
 *      LED 2           heartbeat
 *
 *  Tasks, highest priority first (row 0 is the highest):
 *      controls  1 kHz: reads the knobs and encoder, drives the LEDs' PWM
 *      lights    20 Hz: works out the CPU load, picks the LED colors
 *      work      the adjustable load
 *
 *  The run-to-completion lesson: a handler runs until it returns, and
 *  nothing but interrupts can cut in. So the work task does its job in
 *  250 us chunks, signaling itself between chunks; after every chunk
 *  Floret goes back to the top, and the controls task gets its turn on
 *  time even at 80 % load. The hiccup deliberately breaks that rule: for
 *  5 ms nothing else runs, and Downlink shows it as the work task's
 *  longest dispatch.
 *
 *  Audio passes from input to output; the audio callback drives Floret's
 *  tick (48 samples at 48 kHz = 1 ms).
 */
#include "daisy_pod.h"
#include "floret/floret.h"
#include "floret/floret_telemetry.h"

using namespace daisy;

static DaisyPod hw;

enum : floret_id_t
{
    TASK_CONTROLS = 0,
    TASK_LIGHTS   = 1,
    TASK_WORK     = 2,
};

//------------------------------------------------------------------------------
//  Settings, written by the controls task
//------------------------------------------------------------------------------
static uint32_t          work_percent  = 10; // encoder
static uint32_t          work_period   = 10; // ticks (ms), knob 2
static volatile uint32_t audio_load_us = 0;  // knob 1; read in the audio ISR

static void Busy(uint32_t us)
{
    const uint32_t start = System::GetUs();
    while(System::GetUs() - start < us) {}
}

//------------------------------------------------------------------------------
//  work: the adjustable load, done in chunks
//------------------------------------------------------------------------------
static constexpr uint32_t WORK_TICK   = 1u << 0; // timer: start a period's work
static constexpr uint32_t WORK_CHUNK  = 1u << 1; // self-signal: next chunk
static constexpr uint32_t WORK_HICCUP = 1u << 2; // encoder press
static constexpr uint32_t CHUNK_US    = 250;
static constexpr uint32_t HICCUP_US   = 5000;

static floret_timer_t work_timer;
static uint32_t       work_remaining_us;

static void work_sys(uint32_t events, void*)
{
    if(events & FLORET_SYS_START)
        floret_timer_start(
            &work_timer, TASK_WORK, WORK_TICK, work_period, work_period);
}

static void work_usr(uint32_t events, void*)
{
    if(events & WORK_HICCUP)
        Busy(HICCUP_US); // one long dispatch, on purpose

    if(events & WORK_TICK) // percent of the period, in microseconds
        work_remaining_us = work_percent * work_period * 10;

    if(work_remaining_us > 0)
    {
        const uint32_t chunk
            = work_remaining_us < CHUNK_US ? work_remaining_us : CHUNK_US;
        Busy(chunk);
        work_remaining_us -= chunk;
        if(work_remaining_us > 0) // let higher-priority tasks in, then continue
            floret_signal(TASK_WORK, FLORET_USR, WORK_CHUNK);
    }
}

//------------------------------------------------------------------------------
//  controls: 1 kHz
//------------------------------------------------------------------------------
static constexpr uint32_t CONTROLS_TICK = 1u << 0;
static floret_timer_t     controls_timer;

static void controls_sys(uint32_t events, void*)
{
    if(events & FLORET_SYS_START)
        floret_timer_start(&controls_timer, TASK_CONTROLS, CONTROLS_TICK, 1, 1);
}

static void controls_usr(uint32_t events, void*)
{
    if(!(events & CONTROLS_TICK))
        return;
    hw.ProcessAllControls();

    const int32_t steps = hw.encoder.Increment();
    if(steps != 0)
    {
        int32_t pct  = (int32_t)work_percent + 2 * steps;
        work_percent = (uint32_t)(pct < 0 ? 0 : (pct > 80 ? 80 : pct));
    }
    if(hw.encoder.RisingEdge())
        floret_signal(TASK_WORK, FLORET_USR, WORK_HICCUP);

    audio_load_us = (uint32_t)(hw.GetKnobValue(DaisyPod::KNOB_1) * 500.f);

    // Knob 2 sets the period. A little hysteresis keeps knob noise from
    // restarting the timer all the time.
    const uint32_t period
        = 5 + (uint32_t)(hw.GetKnobValue(DaisyPod::KNOB_2) * 45.f);
    if(period + 1 < work_period || period > work_period + 1)
    {
        work_period = period;
        floret_timer_start(
            &work_timer, TASK_WORK, WORK_TICK, work_period, work_period);
    }

    hw.UpdateLeds(); // the LEDs are software PWM: update every millisecond
}

//------------------------------------------------------------------------------
//  lights: 20 Hz, CPU load from Floret's telemetry
//------------------------------------------------------------------------------
static constexpr uint32_t LIGHTS_TICK = 1u << 0;
static floret_timer_t     lights_timer;
static uint32_t           last_now, last_idle, beats;

static void lights_sys(uint32_t events, void*)
{
    if(events & FLORET_SYS_START)
        floret_timer_start(&lights_timer, TASK_LIGHTS, LIGHTS_TICK, 50, 50);
}

static void lights_usr(uint32_t events, void*)
{
    if(!(events & LIGHTS_TICK))
        return;
    const volatile floret_telemetry_t* t    = floret_telemetry_get();
    const uint32_t                     now  = t->now;
    const uint32_t                     idle = t->idle_time;
    const uint32_t                     span = now - last_now;
    float cpu = span ? 1.f - (float)(idle - last_idle) / (float)span : 0.f;
    last_now  = now;
    last_idle = idle;
    cpu       = cpu < 0.f ? 0.f : (cpu > 1.f ? 1.f : cpu);

    const float red   = cpu < 0.5f ? 2.f * cpu : 1.f; // green -> yellow -> red
    const float green = cpu < 0.5f ? 1.f : 2.f * (1.f - cpu);
    hw.led1.Set(red, green, 0.f);

    beats++;
    hw.led2.Set(0.f, 0.f, (beats / 10) % 2 ? 0.6f : 0.f); // 1 Hz heartbeat
}

//------------------------------------------------------------------------------
//  Task table: name, init, isr, sys, usr, arg
//------------------------------------------------------------------------------
static const floret_task_t tasks[] = {
    {"controls", nullptr, nullptr, controls_sys, controls_usr, nullptr},
    {"lights", nullptr, nullptr, lights_sys, lights_usr, nullptr},
    {"work", nullptr, nullptr, work_sys, work_usr, nullptr},
};

//------------------------------------------------------------------------------
//  Audio: passthrough, the knob-1 load, and Floret's tick
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
    Busy(audio_load_us);
    floret_tick();
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(48); // one tick per millisecond

    floret_init(tasks, sizeof(tasks) / sizeof(tasks[0]));
    floret_start();
    hw.StartAdc();
    hw.StartAudio(AudioCallback);
    floret_run();
}
