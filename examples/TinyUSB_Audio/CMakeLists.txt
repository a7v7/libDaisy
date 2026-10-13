set(FIRMWARE_NAME TinyUSB_Audio)
set(FIRMWARE_SOURCES TinyUSB_Audio.cpp usb_audio.cpp usb_descriptors.c)
include(DaisyProject)

# TinyUSB, configured by this folder's tusb_config.h
target_link_libraries(${FIRMWARE_NAME} PRIVATE daisy_tinyusb)
target_include_directories(${FIRMWARE_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
