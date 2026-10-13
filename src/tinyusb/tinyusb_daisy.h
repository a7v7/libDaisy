/** TinyUSB on the Daisy Seed.
 *
 *  TinyUSB (Middlewares/tinyusb) is an alternative to libDaisy's ST USB
 *  stack. Among its device classes is USB Audio Class 2. An app that uses it:
 *
 *  - links the daisy_tinyusb CMake target:
 *        target_link_libraries(${FIRMWARE_NAME} PRIVATE daisy_tinyusb)
 *  - provides tusb_config.h on its include path, and its USB descriptors,
 *    as for any TinyUSB app (see examples/TinyUSB_CDC_Echo),
 *  - calls daisy::tinyusb::Init() after the board's Init(), then calls
 *    tud_task() regularly, for example from the main loop.
 *
 *  The port is chosen at compile time with BOARD_TUD_RHPORT in
 *  tusb_config.h:
 *
 *      0  the Seed's micro-USB connector (USB2_OTG_FS on PA11/PA12)
 *      1  the external USB pins D29/D30 (USB1_OTG_HS with its full-speed
 *         PHY on PB14/PB15). Not yet tested.
 *
 *  Only that port's interrupt handler is taken over, so libDaisy's ST stack
 *  can still drive the other port. Both ports are full speed (12 Mbit/s).
 */
#pragma once
#ifndef DSY_TINYUSB_DAISY_H
#define DSY_TINYUSB_DAISY_H

namespace daisy
{
namespace tinyusb
{
/** Sets up the port's pins, peripheral clock and interrupt priority, then
 *  starts TinyUSB's device stack on it. System::Init() (called by the
 *  board's Init()) has already set up the 48 MHz USB clock.
 *  \return false if TinyUSB failed to start.
 */
bool Init();

} // namespace tinyusb
} // namespace daisy

#endif
