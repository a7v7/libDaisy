#pragma once
#include "daisy_seed.h"
#include "dev/oled_ssd130x.h"

namespace daisy
{
/**
    @brief Class that handles initializing all of the hardware specific to the Daisy DSP Board. \n
    Helper funtions are also in place to provide easy access to built-in controls and peripherals.
    @author A.C. Verbeck
    @date April 2024
    @ingroup boards
*/
class DaisyDSP
{
  public:
    /** Enum of Ctrls to represent the four CV/Knob combos on the DSP
     */
    enum Ctrl
    {
        CTRL_1,    /**< */
        CTRL_2,    /**< */
        CTRL_3,    /**< */
        CTRL_4,    /**< */
        CTRL_LAST, /**< */
    };

    /** Constructor */
    DaisyDSP() {}
    /** Destructor */
    ~DaisyDSP() {}

    /** Initializes the daisy seed, and DSP hardware.*/
    void Init(bool boost = false);

    /** 
    Wait some ms before going on.
    \param del Delay time in ms.
    */
    void DelayMs(size_t del);


    /** Starts the callback
    \param cb multichannel callback function
    */
    void StartAudio(AudioHandle::AudioCallback cb);

    /**
       Switch callback functions
       \param cb New multichannel callback function.
    */
    void ChangeAudioCallback(AudioHandle::AudioCallback cb);

    /** Stops the audio */
    void StopAudio();

    /** Set the sample rate for the audio */
    void SetAudioSampleRate(SaiHandle::Config::SampleRate samplerate);

    /** Get sample rate */
    float AudioSampleRate();

    /** Audio Block size defaults to 48.
    Change it using this function before StartingAudio
    \param size Audio block size.
    */
    void SetAudioBlockSize(size_t size);

    /** Returns the number of samples per channel in a block of audio. */
    size_t AudioBlockSize();

    /** Returns the rate in Hz that the Audio callback is called */
    float AudioCallbackRate();

    /** Start analog to digital conversion.*/
    void StartAdc();

    /** Stops Transfering data from the ADC */
    void StopAdc();


    /** Call at same rate as reading controls for good reads. */
    void ProcessAnalogControls();

    /** Process Analog and Digital Controls */
    inline void ProcessAllControls()
    {
        ProcessAnalogControls();
        ProcessDigitalControls();
    }

    /**
       Get value for a particular control
       \param k Which control to get
     */
    float GetKnobValue(Ctrl k);

    /**  Process the digital controls */
    void ProcessDigitalControls();

    /**  Control the display */
    void DisplayControls(bool invert = true);

    /* These are exposed for the user to access and manipulate directly
       Helper functions above provide easier access to much of what they are capable of.
    */
    DaisySeed       seed;                             /**< Seed object */
    Encoder         encoder;                          /**< Encoder object */
    AnalogControl   controls[CTRL_LAST];              /**< Array of controls*/
    OledDisplay<SSD130x4WireSpi128x64Driver> display; /**< & */

  private:
    void SetHidUpdateRates();
    void InitControls();
    void InitDisplay();
    void InitEncoder();

    uint32_t screen_update_last_, screen_update_period_;
};

} // namespace daisy
