/** USB descriptors for TinyUSB_Audio: UAC2 stereo out/in plus a CDC port.
 *
 *  Based on TinyUSB's cdc_uac2 example (MIT License,
 *  Copyright (c) 2019 Ha Thach, tinyusb.org).
 *
 *  VID 0xCafe is TinyUSB's VID for examples and testing. A product that
 *  ships needs a VID/PID of its own.
 */
#include <string.h>

#include "tusb.h"
#include "usb_descriptors.h"
#include "util/unique_id.h"

#define USB_VID 0xCafe
#define USB_PID 0x4021 // Audio + CDC. Each interface combination needs its
                       // own PID, because the host caches drivers by VID/PID.

//--------------------------------------------------------------------+
// Device descriptor
//--------------------------------------------------------------------+
static tusb_desc_device_t const desc_device = {
    .bLength         = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB          = 0x0201, // 2.01: has a BOS descriptor (see below)

    // IAD, so the host groups each function's interfaces
    .bDeviceClass    = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor  = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0110, // 1.1: adds the feature unit (volume, mute)

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
#define EPNUM_AUDIO_OUT 0x01
#define EPNUM_AUDIO_IN 0x81
#define EPNUM_AUDIO_FB 0x82
#define EPNUM_CDC_NOTIF 0x83
#define EPNUM_CDC_OUT 0x04
#define EPNUM_CDC_IN 0x84

#define CONFIG_TOTAL_LEN \
    (TUD_CONFIG_DESC_LEN + DAISY_UAC2_DESC_LEN + TUD_CDC_DESC_LEN)

static uint8_t const desc_configuration[] = {
    // config number, interface count, string index, total length,
    // attributes, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

    DAISY_UAC2_DESCRIPTOR(
        4, EPNUM_AUDIO_OUT, EPNUM_AUDIO_FB, EPNUM_AUDIO_IN, DAISY_UAC2_EP_SIZE),

    TUD_CDC_DESCRIPTOR(
        ITF_NUM_CDC, 5, EPNUM_CDC_NOTIF, 16, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
};

TU_VERIFY_STATIC(sizeof(desc_configuration) == CONFIG_TOTAL_LEN,
                 "Incorrect configuration descriptor size");

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return desc_configuration;
}

//--------------------------------------------------------------------+
// Recognizing a Windows host
//
// The playback feedback value must be sent in a different format for
// Windows: its UAC2 driver needs 16.16 in 4 bytes at full speed, while
// macOS needs the USB 2.0 spec's 10.14 in 3 bytes (Linux takes either).
//
// To tell them apart, the BOS descriptor advertises Microsoft OS 2.0
// descriptors. Windows (8.1 and later) then asks for the descriptor set with
// a vendor request while it enumerates the device; macOS and Linux never
// send it. The set returned is minimal: it changes nothing in Windows.
//--------------------------------------------------------------------+
#define VENDOR_REQUEST_MS_OS_20 0x01
#define MS_OS_20_DESC_LEN (0x0A + 0x06) // set header + vendor revision

static uint8_t const desc_bos[] = {
    TUD_BOS_DESCRIPTOR(TUD_BOS_DESC_LEN + TUD_BOS_MICROSOFT_OS_DESC_LEN, 1),
    TUD_BOS_MS_OS_20_DESCRIPTOR(MS_OS_20_DESC_LEN, VENDOR_REQUEST_MS_OS_20),
};

static uint8_t const desc_ms_os_20[] = {
    // Set header: length, type, Windows version (8.1), total length
    U16_TO_U8S_LE(0x000A),
    U16_TO_U8S_LE(MS_OS_20_SET_HEADER_DESCRIPTOR),
    U32_TO_U8S_LE(0x06030000),
    U16_TO_U8S_LE(MS_OS_20_DESC_LEN),
    // Vendor revision: length, type, revision
    U16_TO_U8S_LE(0x0006),
    U16_TO_U8S_LE(MS_OS_20_FEATURE_VENDOR_REVISION),
    U16_TO_U8S_LE(0x0001),
};

TU_VERIFY_STATIC(sizeof(desc_ms_os_20) == MS_OS_20_DESC_LEN,
                 "Incorrect MS OS 2.0 descriptor set size");

// Set from tud_task() (the vendor request), read in the USB interrupt (the
// feedback format callback).
static volatile bool host_is_windows = false;

uint8_t const *tud_descriptor_bos_cb(void)
{
    return desc_bos;
}

bool tud_vendor_control_xfer_cb(uint8_t                       rhport,
                                uint8_t                       stage,
                                tusb_control_request_t const *request)
{
    if(stage != CONTROL_STAGE_SETUP)
        return true;
    if(request->bRequest == VENDOR_REQUEST_MS_OS_20 && request->wIndex == 7)
    {
        host_is_windows = true;
        return tud_control_xfer(
            rhport, request, (void *)(uintptr_t)desc_ms_os_20, sizeof(desc_ms_os_20));
    }
    return false; // anything else: stall
}

bool usb_host_is_windows(void)
{
    return host_is_windows;
}

void usb_host_forget(void)
{
    host_is_windows = false;
}

// TinyUSB asks before each feedback transfer: the spec's 10.14 for every
// host except Windows.
bool tud_audio_feedback_format_10_14_cb(uint8_t func_id)
{
    (void)func_id;
    return !host_is_windows;
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
};

static char const *string_desc_arr[] = {
    NULL,           // 0: language, sent as English (0x0409) below
    "Daisy",        // 1: Manufacturer
    "Daisy Audio",  // 2: Product
    NULL,           // 3: Serial number, from the MCU's unique ID
    "Daisy Audio",  // 4: Audio function
    "Daisy Serial", // 5: CDC interface
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
