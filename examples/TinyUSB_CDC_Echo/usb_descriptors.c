/** USB descriptors for TinyUSB_CDC_Echo: a single CDC (USB serial) port.
 *
 *  Based on TinyUSB's cdc_dual_ports example (MIT License,
 *  Copyright (c) 2019 Ha Thach, tinyusb.org).
 *
 *  VID 0xCafe is TinyUSB's VID for examples and testing. A product that
 *  ships needs a VID/PID of its own.
 */
#include <string.h>

#include "tusb.h"
#include "util/unique_id.h"

#define USB_VID 0xCafe
#define USB_PID 0x4001 // CDC only. Each interface combination needs its own PID,
                       // because the host caches drivers by VID/PID.

//--------------------------------------------------------------------+
// Device descriptor
//--------------------------------------------------------------------+
static tusb_desc_device_t const desc_device = {
    .bLength         = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB          = 0x0200,

    // IAD, so the host groups the CDC interfaces into one function
    .bDeviceClass    = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor  = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0100,

    .iManufacturer = 0x01,
    .iProduct      = 0x02,
    .iSerialNumber = 0x03,

    .bNumConfigurations = 0x01};

uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&desc_device;
}

//--------------------------------------------------------------------+
// Configuration descriptor
//--------------------------------------------------------------------+
enum
{
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_TOTAL
};

#define EPNUM_CDC_NOTIF 0x81
#define EPNUM_CDC_OUT 0x02
#define EPNUM_CDC_IN 0x82

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)

static uint8_t const desc_configuration[] = {
    // config number, interface count, string index, total length,
    // attributes, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

    // interface number, string index, notification EP and size,
    // data OUT and IN EPs, data EP size
    TUD_CDC_DESCRIPTOR(
        ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 16, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return desc_configuration;
}

//--------------------------------------------------------------------+
// String descriptors
//--------------------------------------------------------------------+
enum
{
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
    STRID_CDC,
};

static char const *string_desc_arr[] = {
    NULL,               // 0: language, sent as English (0x0409) below
    "Daisy",            // 1: Manufacturer
    "Daisy TinyUSB CDC", // 2: Product
    NULL,               // 3: Serial number, from the MCU's unique ID
    "Daisy Serial",     // 4: CDC interface
};

static uint16_t _desc_str[32 + 1];

// Writes the 96-bit unique ID as 24 hex digits.
static size_t get_serial(uint16_t *out)
{
    uint32_t id[3];
    dsy_get_unique_id(&id[0], &id[1], &id[2]);
    static char const hex[] = "0123456789ABCDEF";
    size_t            n     = 0;
    for(int w = 0; w < 3; w++)
        for(int shift = 28; shift >= 0; shift -= 4)
            out[n++] = hex[(id[w] >> shift) & 0xF];
    return n;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    size_t chr_count;

    switch(index)
    {
        case STRID_LANGID:
            _desc_str[1] = 0x0409;
            chr_count    = 1;
            break;

        case STRID_SERIAL: chr_count = get_serial(&_desc_str[1]); break;

        default:
        {
            if(index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))
                return NULL;

            const char  *str       = string_desc_arr[index];
            size_t const max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1;
            chr_count              = strlen(str);
            if(chr_count > max_count)
                chr_count = max_count;

            // ASCII to UTF-16
            for(size_t i = 0; i < chr_count; i++)
                _desc_str[1 + i] = str[i];
            break;
        }
    }

    // First byte: length including the header. Second: descriptor type.
    _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return _desc_str;
}
