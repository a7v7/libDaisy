/** @addtogroup system
    @{
*/

/** NVIC preemption priorities for every interrupt used by libDaisy.
 *
 *  HAL_Init() selects NVIC_PRIORITYGROUP_4: 16 preemption levels,
 *  0 is the highest, and there are no sub-priorities. An interrupt can only
 *  preempt one with a strictly lower priority (higher number), so the audio
 *  DMA interrupt sits alone at the top and storage sits near the bottom.
 *
 *  Each level can be overridden by defining the macro before this header is
 *  included (e.g. with -DDSY_IRQ_PRIO_USB=5 on the command line).
 *
 *  Rule: an interrupt must never wait on work done by an interrupt with a
 *  lower priority (higher number) - it will never complete.
 *
 *  SysTick is not listed here: it uses TICK_INT_PRIORITY (14) from
 *  stm32h7xx_hal_conf.h. The full map is in
 *  doc/md/_b2_Development-Interrupt-Priorities.md.
 */
#ifndef DSY_IRQ_PRIORITY_H
#define DSY_IRQ_PRIORITY_H

/** SAI DMA streams - the audio callback runs here. */
#ifndef DSY_IRQ_PRIO_AUDIO
#define DSY_IRQ_PRIO_AUDIO 0
#endif

/** DAC DMA streams and TIM6 (DAC trigger) - audio/CV rate output. */
#ifndef DSY_IRQ_PRIO_DAC
#define DSY_IRQ_PRIO_DAC 1
#endif

/** TimerHandle callbacks used for musical timing (clocks, gates).
 *  Opt in per timer with TimerHandle::Config::irq_priority.
 */
#ifndef DSY_IRQ_PRIO_TIMER_HIGH
#define DSY_IRQ_PRIO_TIMER_HIGH 2
#endif

/** UART interrupts and UART DMA streams - MIDI and serial. */
#ifndef DSY_IRQ_PRIO_UART
#define DSY_IRQ_PRIO_UART 3
#endif

/** ADC DMA stream - knobs and CV inputs. */
#ifndef DSY_IRQ_PRIO_ADC
#define DSY_IRQ_PRIO_ADC 4
#endif

/** USB OTG FS / HS, device and host. */
#ifndef DSY_IRQ_PRIO_USB
#define DSY_IRQ_PRIO_USB 6
#endif

/** SPI and I2C interrupts and their DMA streams - displays, peripherals. */
#ifndef DSY_IRQ_PRIO_SERIAL_BUS
#define DSY_IRQ_PRIO_SERIAL_BUS 8
#endif

/** SDMMC and QSPI - bulk storage. */
#ifndef DSY_IRQ_PRIO_STORAGE
#define DSY_IRQ_PRIO_STORAGE 10
#endif

/** Default for TimerHandle callbacks: the lowest level, below SysTick (14),
 *  so housekeeping callbacks (UI redraws, etc.) never delay other interrupts.
 */
#ifndef DSY_IRQ_PRIO_TIMER_LOW
#define DSY_IRQ_PRIO_TIMER_LOW 15
#endif

#endif
/** @} */
