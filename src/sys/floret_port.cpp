/** Floret port for the Daisy (STM32H750, Cortex-M7).
 *
 *  The five functions Floret needs from the hardware (floret/floret_port.h):
 *
 *  - now():    TIM2's counter. libDaisy runs TIM2 free (no prescaler, full
 *              32-bit period) for System::GetTick(), and unlike the CPU's
 *              cycle counter it keeps counting while the CPU sleeps.
 *  - lock() / unlock() / sleep(): PRIMASK and WFI. Floret checks for work
 *              with PRIMASK set, then sleeps with WFI. A pending interrupt
 *              still wakes WFI, but its handler runs only after unlock(), so
 *              an event can't slip in between the check and the sleep, and
 *              interrupt time is never counted as idle. Interrupts are held
 *              off only while the CPU is asleep anyway, plus a few cycles to
 *              read the clock after waking.
 *
 *  The tick is not here: call floret_tick() from your audio callback, so
 *  Floret's timers are locked to the codec's sample clock. With a block size
 *  of 48 at 48 kHz, one tick is exactly 1 ms.
 *
 *  Debugging a sleeping Floret: by default the STM32 stops the debug port's
 *  clocks during sleep, so a debugger or Downlink can't reach the chip. Set
 *  the "debug in sleep" bits in DBGMCU_CR (CubeProgrammer's "Debug in Low
 *  Power mode" option does this) when you need to.
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

extern "C" uint32_t floret_port_lock(void)
{
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

extern "C" void floret_port_unlock(uint32_t state)
{
    __set_PRIMASK(state);
}

extern "C" void floret_port_sleep(void)
{
    __DSB();
    __WFI();
}
