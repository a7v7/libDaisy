/** TinyUSB port for the Daisy Seed. See tinyusb_daisy.h.
 *
 *  This file is compiled into each executable that links daisy_tinyusb,
 *  not into libdaisy.a, so it sees the app's tusb_config.h.
 */
#include "tinyusb/tinyusb_daisy.h"

#include "stm32h7xx_hal.h"
#include "sys/irq_priority.h"
#include "sys/system.h"
#include "tusb.h"

#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT 0
#endif

#if BOARD_TUD_RHPORT != 0 && BOARD_TUD_RHPORT != 1
#error "BOARD_TUD_RHPORT must be 0 (micro-USB) or 1 (external pins D29/D30)"
#endif

namespace daisy
{
namespace tinyusb
{
bool Init()
{
    GPIO_InitTypeDef gpio = {};
    gpio.Mode             = GPIO_MODE_AF_PP;
    gpio.Pull             = GPIO_NOPULL;
    gpio.Speed            = GPIO_SPEED_FREQ_HIGH;

#if BOARD_TUD_RHPORT == 0
    // Micro-USB: DM PA11, DP PA12, VBUS sense PA9 (as libDaisy's ST stack)
    __HAL_RCC_GPIOA_CLK_ENABLE();
    gpio.Pin       = GPIO_PIN_11 | GPIO_PIN_12;
    gpio.Alternate = GPIO_AF10_OTG1_FS;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin  = GPIO_PIN_9;
    gpio.Mode = GPIO_MODE_INPUT;
    HAL_GPIO_Init(GPIOA, &gpio);

    __HAL_RCC_USB_OTG_FS_CLK_ENABLE();
    HAL_NVIC_SetPriority(OTG_FS_IRQn, DSY_IRQ_PRIO_USB, 0);
    const bool vbus_sensing = true;
#else
    // External pins D29/D30: DM PB14, DP PB15, no VBUS sense
    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Pin       = GPIO_PIN_14 | GPIO_PIN_15;
    gpio.Alternate = GPIO_AF12_OTG2_FS;
    HAL_GPIO_Init(GPIOB, &gpio);

    __HAL_RCC_USB_OTG_HS_CLK_ENABLE();
    HAL_NVIC_SetPriority(OTG_HS_IRQn, DSY_IRQ_PRIO_USB, 0);
    const bool vbus_sensing = false;
#endif

    tud_configure_dwc2_t cfg = CFG_TUD_CONFIGURE_DWC2_DEFAULT;
    cfg.vbus_sensing         = vbus_sensing;
    tud_configure(BOARD_TUD_RHPORT, TUD_CFGID_DWC2, &cfg);

    // TinyUSB enables the port's interrupt itself.
    const tusb_rhport_init_t init = {TUSB_ROLE_DEVICE, TUSB_SPEED_FULL};
    return tusb_init(BOARD_TUD_RHPORT, &init);
}

} // namespace tinyusb
} // namespace daisy

extern "C"
{
    // These replace libDaisy's (weak) ST stack handlers for this port.
#if BOARD_TUD_RHPORT == 0
    void OTG_FS_IRQHandler(void) { tud_int_handler(0); }
#else
    void OTG_HS_IRQHandler(void) { tud_int_handler(1); }
#endif

    // TinyUSB's millisecond clock, needed without an RTOS.
    uint32_t tusb_time_millis_api(void) { return daisy::System::GetNow(); }
}
