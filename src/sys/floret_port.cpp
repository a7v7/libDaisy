/** Floret port for the Daisy (STM32H750, Cortex-M7).
 *
 *  The four functions Floret needs from the hardware (floret/floret_port.h):
 *
 *  - now():    TIM2's counter. libDaisy runs TIM2 free (no prescaler, full
 *              32-bit period) for System::GetTick(), and unlike the CPU's
 *              cycle counter it keeps counting while the CPU sleeps.
 *  - idle_wait() / notify(): WFE / SEV. floret_signal() calls notify(),
 *              which latches the event register, so a WFE that follows
 *              returns at once: an event signaled just before the sleep
 *              can't be missed, and no interrupt is ever masked.
 *
 *  The tick is not here: call floret_tick() from your audio callback, so
 *  Floret's timers are locked to the codec's sample clock. With a block size
 *  of 48 at 48 kHz, one tick is exactly 1 ms.
 */
#include "floret/floret_port.h"
#include "stm32h7xx.h"
#include "sys/system.h"

extern "C" uint32_t floret_port_now(void)
{
    return TIM2->CNT; // read directly: this runs twice per dispatch
}

extern "C" uint32_t floret_port_now_hz(void)
{
    return daisy::System::GetTickFreq();
}

extern "C" void floret_port_idle_wait(void)
{
    __WFE();
}

extern "C" void floret_port_notify(void)
{
    __SEV();
}
