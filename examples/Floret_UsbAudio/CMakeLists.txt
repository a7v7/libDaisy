set(FIRMWARE_NAME Floret_UsbAudio)

# The USB audio device itself is shared with TinyUSB_Audio: its config,
# descriptors and the usb_audio module.
set(USB_AUDIO_DIR ${CMAKE_CURRENT_LIST_DIR}/../TinyUSB_Audio)
set(FIRMWARE_SOURCES
    Floret_UsbAudio.cpp
    ${USB_AUDIO_DIR}/usb_audio.cpp
    ${USB_AUDIO_DIR}/usb_descriptors.c
)
include(DaisyProject)

target_link_libraries(${FIRMWARE_NAME} PRIVATE daisy_tinyusb)
target_include_directories(${FIRMWARE_NAME} PRIVATE ${USB_AUDIO_DIR})
