/** TinyUSB CDC echo: USB serial through TinyUSB instead of the ST USB stack.
 *
 *  Open the Seed's COM port in any serial terminal (the baud rate doesn't
 *  matter). It prints a greeting, then echoes back everything you type.
 *  The LED is on while a terminal has the port open.
 *
 *  This is stage 1 of USB Audio Class 2 support: it checks that TinyUSB
 *  runs on the Daisy (pins, clock, interrupt, descriptors) before audio is
 *  added.
 */
#include "daisy_seed.h"
#include "tinyusb/tinyusb_daisy.h"
#include "tusb.h"

using namespace daisy;

static DaisySeed hw;

int main(void)
{
    hw.Init();
    tinyusb::Init();

    bool was_connected = false;
    while(1)
    {
        tud_task(); // handles USB events queued by the interrupt

        const bool connected = tud_cdc_connected();
        if(connected != was_connected)
        {
            was_connected = connected;
            hw.SetLed(connected);
            if(connected)
            {
                tud_cdc_write_str("Daisy TinyUSB echo. Type something.\r\n");
                tud_cdc_write_flush();
            }
        }

        if(tud_cdc_available())
        {
            uint8_t        buf[64];
            const uint32_t n = tud_cdc_read(buf, sizeof(buf));
            tud_cdc_write(buf, n);
            tud_cdc_write_flush();
        }
    }
}
