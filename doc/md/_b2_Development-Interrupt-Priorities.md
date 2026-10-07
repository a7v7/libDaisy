# Development - Interrupt Priorities

libDaisy assigns every interrupt it uses a preemption priority from one header, `src/sys/irq_priority.h`. This page explains the levels, why they are ordered the way they are, and the one rule to follow when writing interrupt code.

## How priorities work on the STM32H7

- `HAL_Init()` selects `NVIC_PRIORITYGROUP_4`: 16 preemption levels, **0 is the highest**, and there are no sub-priorities.
- An interrupt can only preempt another one with a **strictly lower** priority (a higher number). Two interrupts at the same level never interrupt each other - whichever starts first runs to the end.

Before this header existed nearly every interrupt was at level 0, so nothing could preempt anything. In particular, an SD card, display or USB interrupt that was already running delayed the audio callback by its full length.

## The levels

| Level | Macro | Interrupts | Why |
|---|---|---|---|
| 0 | `DSY_IRQ_PRIO_AUDIO` | SAI DMA (DMA1 Stream 0, 1, 3, 4) | The audio callback. Hard real-time, locked to the codec clock |
| 1 | `DSY_IRQ_PRIO_DAC` | DAC DMA (DMA2 Stream 0, 1), TIM6_DAC | Audio/CV-rate output, also clock-locked |
| 2 | `DSY_IRQ_PRIO_TIMER_HIGH` | TimerHandle callbacks that opt in | Musical timing - clocks and gates |
| 3 | `DSY_IRQ_PRIO_UART` | USART/UART/LPUART, UART DMA (DMA1 Stream 5, DMA2 Stream 4) | MIDI input must not overrun the hardware FIFO |
| 4 | `DSY_IRQ_PRIO_ADC` | ADC DMA (DMA1 Stream 2) | Knobs and CV inputs |
| 6 | `DSY_IRQ_PRIO_USB` | OTG FS / OTG HS, device and host | USB has hardware buffering and tolerates some delay |
| 8 | `DSY_IRQ_PRIO_SERIAL_BUS` | SPI1-6, I2C1-3 events, their DMA (DMA2 Stream 2, 3, DMA1 Stream 6) | Displays and peripheral control |
| 10 | `DSY_IRQ_PRIO_STORAGE` | SDMMC1, QUADSPI | Bulk storage tolerates the most delay |
| 14 | `TICK_INT_PRIORITY` | SysTick | HAL tick; set in `stm32h7xx_hal_conf.h` |
| 15 | `DSY_IRQ_PRIO_TIMER_LOW` | TimerHandle callbacks (default) | Housekeeping (UI redraws, etc.) never delays other interrupts |

## Timer callbacks

A `TimerHandle` callback runs at `DSY_IRQ_PRIO_TIMER_LOW` unless you ask otherwise. This is the right choice for background work, such as the Desktop DevKit's `StartLowPriorityCallback()`, which redraws the UI over SPI.

For musical timing, raise the priority for that one timer:

```cpp
TimerHandle::Config cfg;
cfg.periph       = TimerHandle::Config::Peripheral::TIM_3;
cfg.period       = period_ticks;
cfg.enable_irq   = true;
cfg.irq_priority = DSY_IRQ_PRIO_TIMER_HIGH;
clock_timer.Init(cfg);
clock_timer.SetCallback(OnClockTick);
clock_timer.Start();
```

A high-priority timer callback must be short, and must follow the rule below.

## The rule

**An interrupt must never wait on work done by an interrupt with a lower priority.**

The lower-priority interrupt cannot run until the waiting one returns, so the wait never ends. For example, a `DSY_IRQ_PRIO_TIMER_HIGH` callback must not start an SPI DMA transfer and spin until it completes, because the SPI DMA interrupt is at level 8. The same applies to `System::Delay()` from any interrupt above SysTick: the tick never advances.

## Overriding a level

Every macro is wrapped in `#ifndef`, so an application can change a level without editing libDaisy. Define it for the whole build so the library and the application agree, for example in CMake:

```cmake
target_compile_definitions(daisy PUBLIC DSY_IRQ_PRIO_USB=5)
```

Keep the audio interrupt alone at the top. Anything placed at the same level as audio can delay the audio callback.
