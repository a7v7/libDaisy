/** Interface numbers and the UAC2 descriptor for TinyUSB_Audio.
 *
 *  The audio function is a stereo line output (host to Daisy) and a stereo
 *  line input (Daisy to host), both on one internal 48 kHz clock:
 *
 *      USB OUT  -> [IT 0x01 USB streaming] -> [OT 0x03 line out]
 *      line in  -> [IT 0x11 line in]       -> [OT 0x13 USB streaming] -> USB IN
 *
 *  Based on TinyUSB's uac2_headset and uac2_speaker_fb examples (MIT
 *  License, Copyright (c) 2020 Ha Thach, tinyusb.org). Unlike the headset,
 *  playback here is asynchronous with a feedback endpoint, and there's no
 *  feature unit (volume/mute), so the host does volume in software.
 */
#pragma once
#ifndef USB_DESCRIPTORS_H_
#define USB_DESCRIPTORS_H_

enum
{
    ITF_NUM_AUDIO_CONTROL = 0,
    ITF_NUM_AUDIO_STREAMING_OUT, // host to Daisy
    ITF_NUM_AUDIO_STREAMING_IN,  // Daisy to host
    ITF_NUM_CDC,
    ITF_NUM_CDC_DATA,
    ITF_NUM_TOTAL
};

#define ITF_NUM_AUDIO_TOTAL 3

// Entity IDs
#define UAC2_ENTITY_CLOCK 0x04
#define UAC2_ENTITY_OUT_INPUT_TERMINAL 0x01  // USB streaming in
#define UAC2_ENTITY_OUT_OUTPUT_TERMINAL 0x03 // line out
#define UAC2_ENTITY_IN_INPUT_TERMINAL 0x11   // line in
#define UAC2_ENTITY_IN_OUTPUT_TERMINAL 0x13  // USB streaming out

// USB terminal type "Line connector" (External Terminal Types, 0x0603).
// Windows names the devices "Line".
#define DAISY_TERM_TYPE_LINE 0x0603

#define DAISY_CHANNEL_CONFIG_STEREO \
    (AUDIO20_CHANNEL_CONFIG_FRONT_LEFT | AUDIO20_CHANNEL_CONFIG_FRONT_RIGHT)

#define DAISY_UAC2_AC_TOTAL_LEN                                     \
    (TUD_AUDIO20_DESC_CLK_SRC_LEN + TUD_AUDIO20_DESC_INPUT_TERM_LEN \
     + TUD_AUDIO20_DESC_OUTPUT_TERM_LEN + TUD_AUDIO20_DESC_INPUT_TERM_LEN \
     + TUD_AUDIO20_DESC_OUTPUT_TERM_LEN)

#define DAISY_UAC2_DESC_LEN                                                   \
    (TUD_AUDIO20_DESC_IAD_LEN + TUD_AUDIO20_DESC_STD_AC_LEN                   \
     + TUD_AUDIO20_DESC_CS_AC_LEN                                             \
     + DAISY_UAC2_AC_TOTAL_LEN /* OUT streaming, alt 0 and 1 */               \
     + TUD_AUDIO20_DESC_STD_AS_LEN + TUD_AUDIO20_DESC_STD_AS_LEN              \
     + TUD_AUDIO20_DESC_CS_AS_INT_LEN + TUD_AUDIO20_DESC_TYPE_I_FORMAT_LEN    \
     + TUD_AUDIO20_DESC_STD_AS_ISO_EP_LEN + TUD_AUDIO20_DESC_CS_AS_ISO_EP_LEN \
     + TUD_AUDIO20_DESC_STD_AS_ISO_FB_EP_LEN /* IN streaming, alt 0 and 1 */  \
     + TUD_AUDIO20_DESC_STD_AS_LEN + TUD_AUDIO20_DESC_STD_AS_LEN              \
     + TUD_AUDIO20_DESC_CS_AS_INT_LEN + TUD_AUDIO20_DESC_TYPE_I_FORMAT_LEN    \
     + TUD_AUDIO20_DESC_STD_AS_ISO_EP_LEN + TUD_AUDIO20_DESC_CS_AS_ISO_EP_LEN)

#define DAISY_UAC2_DESCRIPTOR(_stridx, _epout, _epfb, _epin, _epsize)                                        \
    /* Interface Association: the three audio interfaces form one function */                                \
    TUD_AUDIO20_DESC_IAD(ITF_NUM_AUDIO_CONTROL, ITF_NUM_AUDIO_TOTAL, 0x00),                                  \
    /* Standard AC Interface (4.7.1) */                                                                      \
    TUD_AUDIO20_DESC_STD_AC(ITF_NUM_AUDIO_CONTROL, /*_nEPs*/ 0x00, _stridx),                                 \
    /* Class-Specific AC Interface Header (4.7.2) */                                                         \
    TUD_AUDIO20_DESC_CS_AC(0x0200,                                                                           \
                           AUDIO20_FUNC_IO_BOX,                                                              \
                           DAISY_UAC2_AC_TOTAL_LEN,                                                          \
                           AUDIO20_CS_AS_INTERFACE_CTRL_LATENCY_POS),                                        \
    /* Clock Source (4.7.2.1): internal, fixed 48 kHz; the host can read */                                  \
    /* the frequency and whether the clock is valid */                                                       \
    TUD_AUDIO20_DESC_CLK_SRC(UAC2_ENTITY_CLOCK,                                                              \
                             AUDIO20_CLOCK_SOURCE_ATT_INT_FIX_CLK,                                           \
                             (AUDIO20_CTRL_R << AUDIO20_CLOCK_SOURCE_CTRL_CLK_FRQ_POS)                       \
                                 | (AUDIO20_CTRL_R << AUDIO20_CLOCK_SOURCE_CTRL_CLK_VAL_POS),                \
                             0x00,                                                                           \
                             0x00),                                                                          \
    /* Playback path: USB streaming -> line out */                                                           \
    TUD_AUDIO20_DESC_INPUT_TERM(UAC2_ENTITY_OUT_INPUT_TERMINAL,                                              \
                                AUDIO_TERM_TYPE_USB_STREAMING,                                               \
                                0x00,                                                                        \
                                UAC2_ENTITY_CLOCK,                                                           \
                                DAISY_UAC2_N_CHANNELS,                                                       \
                                DAISY_CHANNEL_CONFIG_STEREO,                                                 \
                                0x00,                                                                        \
                                0x0000,                                                                      \
                                0x00),                                                                       \
    TUD_AUDIO20_DESC_OUTPUT_TERM(UAC2_ENTITY_OUT_OUTPUT_TERMINAL,                                            \
                                 DAISY_TERM_TYPE_LINE,                                                       \
                                 0x00,                                                                       \
                                 UAC2_ENTITY_OUT_INPUT_TERMINAL,                                             \
                                 UAC2_ENTITY_CLOCK,                                                          \
                                 0x0000,                                                                     \
                                 0x00),                                                                      \
    /* Recording path: line in -> USB streaming */                                                           \
    TUD_AUDIO20_DESC_INPUT_TERM(UAC2_ENTITY_IN_INPUT_TERMINAL,                                               \
                                DAISY_TERM_TYPE_LINE,                                                        \
                                0x00,                                                                        \
                                UAC2_ENTITY_CLOCK,                                                           \
                                DAISY_UAC2_N_CHANNELS,                                                       \
                                DAISY_CHANNEL_CONFIG_STEREO,                                                 \
                                0x00,                                                                        \
                                0x0000,                                                                      \
                                0x00),                                                                       \
    TUD_AUDIO20_DESC_OUTPUT_TERM(UAC2_ENTITY_IN_OUTPUT_TERMINAL,                                             \
                                 AUDIO_TERM_TYPE_USB_STREAMING,                                              \
                                 0x00,                                                                       \
                                 UAC2_ENTITY_IN_INPUT_TERMINAL,                                              \
                                 UAC2_ENTITY_CLOCK,                                                          \
                                 0x0000,                                                                     \
                                 0x00),                                                                      \
    /* Playback streaming interface. Alt 0: no bandwidth (idle) */                                           \
    TUD_AUDIO20_DESC_STD_AS_INT(ITF_NUM_AUDIO_STREAMING_OUT, 0x00, 0x00, 0x00),                              \
    /* Alt 1: streaming, with the data and feedback endpoints */                                             \
    TUD_AUDIO20_DESC_STD_AS_INT(ITF_NUM_AUDIO_STREAMING_OUT, 0x01, 0x02, 0x00),                              \
    TUD_AUDIO20_DESC_CS_AS_INT(UAC2_ENTITY_OUT_INPUT_TERMINAL,                                               \
                               AUDIO20_CTRL_NONE,                                                            \
                               AUDIO20_FORMAT_TYPE_I,                                                        \
                               AUDIO20_DATA_FORMAT_TYPE_I_PCM,                                               \
                               DAISY_UAC2_N_CHANNELS,                                                        \
                               DAISY_CHANNEL_CONFIG_STEREO,                                                  \
                               0x00),                                                                        \
    TUD_AUDIO20_DESC_TYPE_I_FORMAT(DAISY_UAC2_BYTES_PER_SAMPLE, DAISY_UAC2_RESOLUTION),                      \
    TUD_AUDIO20_DESC_STD_AS_ISO_EP(_epout,                                                                   \
                                   (uint8_t)((uint8_t)TUSB_XFER_ISOCHRONOUS                                  \
                                             | (uint8_t)TUSB_ISO_EP_ATT_ASYNCHRONOUS                         \
                                             | (uint8_t)TUSB_ISO_EP_ATT_DATA),                               \
                                   _epsize,                                                                  \
                                   0x01),                                                                    \
    TUD_AUDIO20_DESC_CS_AS_ISO_EP(AUDIO20_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK,                          \
                                  AUDIO20_CTRL_NONE,                                                         \
                                  AUDIO20_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC,                        \
                                  0x0001),                                                                   \
    /* Feedback: 4 bytes in 16.16 format, every 1 ms */                                                      \
    TUD_AUDIO20_DESC_STD_AS_ISO_FB_EP(_epfb, 4, 0x01),                                                       \
    /* Recording streaming interface. Alt 0: no bandwidth (idle) */                                          \
    TUD_AUDIO20_DESC_STD_AS_INT(ITF_NUM_AUDIO_STREAMING_IN, 0x00, 0x00, 0x00),                               \
    /* Alt 1: streaming, with the data endpoint */                                                           \
    TUD_AUDIO20_DESC_STD_AS_INT(ITF_NUM_AUDIO_STREAMING_IN, 0x01, 0x01, 0x00),                               \
    TUD_AUDIO20_DESC_CS_AS_INT(UAC2_ENTITY_IN_OUTPUT_TERMINAL,                                               \
                               AUDIO20_CTRL_NONE,                                                            \
                               AUDIO20_FORMAT_TYPE_I,                                                        \
                               AUDIO20_DATA_FORMAT_TYPE_I_PCM,                                               \
                               DAISY_UAC2_N_CHANNELS,                                                        \
                               DAISY_CHANNEL_CONFIG_STEREO,                                                  \
                               0x00),                                                                        \
    TUD_AUDIO20_DESC_TYPE_I_FORMAT(DAISY_UAC2_BYTES_PER_SAMPLE, DAISY_UAC2_RESOLUTION),                      \
    TUD_AUDIO20_DESC_STD_AS_ISO_EP(_epin,                                                                    \
                                   (uint8_t)((uint8_t)TUSB_XFER_ISOCHRONOUS                                  \
                                             | (uint8_t)TUSB_ISO_EP_ATT_ASYNCHRONOUS                         \
                                             | (uint8_t)TUSB_ISO_EP_ATT_DATA),                               \
                                   _epsize,                                                                  \
                                   0x01),                                                                    \
    TUD_AUDIO20_DESC_CS_AS_ISO_EP(AUDIO20_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK,                          \
                                  AUDIO20_CTRL_NONE,                                                         \
                                  AUDIO20_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED,                       \
                                  0x0000)

#endif
