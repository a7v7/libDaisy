#include "daisy_dsp.h"

using namespace daisy;

// Hardware Definitions
constexpr Pin PIN_ENC_CLICK  = seed::D0;
constexpr Pin PIN_ENC_B      = seed::D11;
constexpr Pin PIN_ENC_A      = seed::D12;
constexpr Pin PIN_OLED_DC    = seed::D9;
constexpr Pin PIN_OLED_RESET = seed::D30;
constexpr Pin PIN_UART_OUT   = seed::D13;
constexpr Pin PIN_UART_IN    = seed::D14;

constexpr Pin PIN_CTRL_1 = seed::D15;
constexpr Pin PIN_CTRL_2 = seed::D16;
constexpr Pin PIN_CTRL_3 = seed::D21;
constexpr Pin PIN_CTRL_4 = seed::D18;


void DaisyDSP::Init(bool boost)
{
    // Configure Seed first
    seed.Configure();
    seed.Init(boost);
    InitEncoder();
    InitDisplay();
    InitControls();
    // Set Screen update vars
    screen_update_period_ = 17; // roughly 60Hz
    screen_update_last_   = seed.system.GetNow();
}

void DaisyDSP::DelayMs(size_t del)
{
    seed.DelayMs(del);
}

void DaisyDSP::SetHidUpdateRates()
{
    for(size_t i = 0; i < CTRL_LAST; i++)
    {
        controls[i].SetSampleRate(AudioCallbackRate());
    }
}

void DaisyDSP::StartAudio(AudioHandle::AudioCallback cb)
{
    seed.StartAudio(cb);
}

void DaisyDSP::ChangeAudioCallback(AudioHandle::AudioCallback cb)
{
    seed.ChangeAudioCallback(cb);
}

void DaisyDSP::StopAudio()
{
    seed.StopAudio();
}

void DaisyDSP::SetAudioSampleRate(SaiHandle::Config::SampleRate samplerate)
{
    seed.SetAudioSampleRate(samplerate);
    SetHidUpdateRates();
}

float DaisyDSP::AudioSampleRate()
{
    return seed.AudioSampleRate();
}

void DaisyDSP::SetAudioBlockSize(size_t size)
{
    seed.SetAudioBlockSize(size);
    SetHidUpdateRates();
}

size_t DaisyDSP::AudioBlockSize()
{
    return seed.AudioBlockSize();
}

float DaisyDSP::AudioCallbackRate()
{
    return seed.AudioCallbackRate();
}

void DaisyDSP::StartAdc()
{
    seed.adc.Start();
}

/** Stops Transfering data from the ADC */
void DaisyDSP::StopAdc()
{
    seed.adc.Stop();
}


void DaisyDSP::ProcessAnalogControls()
{
    for(size_t i = 0; i < CTRL_LAST; i++)
    {
        controls[i].Process();
    }
}
float DaisyDSP::GetKnobValue(Ctrl k)
{
    return (controls[k].Value());
}

void DaisyDSP::ProcessDigitalControls()
{
    encoder.Debounce();
}

// This will render the display with the controls as vertical bars
void DaisyDSP::DisplayControls(bool invert)
{
    bool on, off;
    on  = invert ? false : true;
    off = invert ? true : false;
    if(seed.system.GetNow() - screen_update_last_ > screen_update_period_)
    {
        // Graph Knobs
        size_t barwidth, barspacing;
        size_t curx, cury;
        screen_update_last_ = seed.system.GetNow();
        barwidth            = 15;
        barspacing          = 20;
        display.Fill(off);
        // Bars for all four knobs.
        for(size_t i = 0; i < DaisyDSP::CTRL_LAST; i++)
        {
            float  v;
            size_t dest;
            curx = (barspacing * i + 1) + (barwidth * i);
            cury = display.Height();
            v    = GetKnobValue(static_cast<DaisyDSP::Ctrl>(i));
            dest = (v * display.Height());
            for(size_t j = dest; j > 0; j--)
            {
                for(size_t k = 0; k < barwidth; k++)
                {
                    display.DrawPixel(curx + k, cury - j, on);
                }
            }
        }
        display.Update();
    }
}

void DaisyDSP::InitControls()
{
    AdcChannelConfig cfg[CTRL_LAST];

    // Init ADC channels with Pins
    cfg[CTRL_1].InitSingle(PIN_CTRL_1);
    cfg[CTRL_2].InitSingle(PIN_CTRL_2);
    cfg[CTRL_3].InitSingle(PIN_CTRL_3);
    cfg[CTRL_4].InitSingle(PIN_CTRL_4);

    // Initialize ADC
    seed.adc.Init(cfg, CTRL_LAST);

    // Initialize AnalogControls, with flip set to true
    for(size_t i = 0; i < CTRL_LAST; i++)
    {
        controls[i].Init(seed.adc.GetPtr(i), AudioCallbackRate(), true);
    }
}

void DaisyDSP::InitDisplay()
{
    OledDisplay<SSD130x4WireSpi128x64Driver>::Config display_config;

    display_config.driver_config.transport_config.pin_config.dc = PIN_OLED_DC;
    display_config.driver_config.transport_config.pin_config.reset
        = PIN_OLED_RESET;

    display.Init(display_config);
}

void DaisyDSP::InitEncoder()
{
    encoder.Init(PIN_ENC_A, PIN_ENC_B, PIN_ENC_CLICK);
}
