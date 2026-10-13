/** TinyUSB configuration for TinyUSB_Audio: a USB Audio Class 2 interface
 *  (stereo out and in, 48 kHz, 24-bit) plus a CDC serial port for status.
 */
#pragma once
#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

// Port: 0 = the Seed's micro-USB, 1 = external pins D29/D30
#define BOARD_TUD_RHPORT 0

#define CFG_TUSB_OS OPT_OS_NONE
#define CFG_TUSB_DEBUG 0

#define CFG_TUD_ENABLED 1
#define CFG_TUD_MAX_SPEED OPT_MODE_FULL_SPEED

#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN __attribute__((aligned(4)))

#define CFG_TUD_ENDPOINT0_SIZE 64

#define CFG_TUD_CDC 1
#define CFG_TUD_MSC 0
#define CFG_TUD_HID 0
#define CFG_TUD_MIDI 0
#define CFG_TUD_AUDIO 1
#define CFG_TUD_VENDOR 0

//--------------------------------------------------------------------+
// Audio format, the same in both directions
//--------------------------------------------------------------------+
#define DAISY_UAC2_SAMPLE_RATE 48000
#define DAISY_UAC2_N_CHANNELS 2
// 24-bit samples, each in a 32-bit slot (left-justified, low byte zero),
// so a sample is an int32_t on the Daisy side.
#define DAISY_UAC2_BYTES_PER_SAMPLE 4
#define DAISY_UAC2_RESOLUTION 24

// Largest packet: one frame (1 ms) of audio plus one extra sample, which
// asynchronous streams need room for. 49 x 2 x 4 = 392 bytes.
#define DAISY_UAC2_EP_SIZE                    \
    TUD_AUDIO_EP_SIZE(0,                      \
                      DAISY_UAC2_SAMPLE_RATE, \
                      DAISY_UAC2_BYTES_PER_SAMPLE, \
                      DAISY_UAC2_N_CHANNELS)

#define CFG_TUD_AUDIO_ENABLE_EP_OUT 1 // host to Daisy (playback)
#define CFG_TUD_AUDIO_ENABLE_EP_IN 1  // Daisy to host (recording)

// Playback is asynchronous: Daisy's codec clock sets the rate, and the
// feedback endpoint tells the host how fast to send.
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP 1

#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX DAISY_UAC2_EP_SIZE
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX DAISY_UAC2_EP_SIZE

// Software FIFOs between USB and the app: 4 packets each. The feedback
// method used here (FIFO count) needs at least 4.
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ (4 * DAISY_UAC2_EP_SIZE)
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ (4 * DAISY_UAC2_EP_SIZE)

#define CFG_TUD_CDC_RX_BUFSIZE 64
#define CFG_TUD_CDC_TX_BUFSIZE 256

#endif
