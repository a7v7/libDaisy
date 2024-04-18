#pragma once

#include "per/spi.h"
#include "per/gpio.h"
#include "sys/system.h"

namespace daisy
{

/**
 * 4 Wire SPI Transport for SSD1327 OLED display devices
 */
class SSD13274WireSpiTransport
{
  public:
    struct Config
    {
        Config()
        {
            // Initialize using defaults
            Defaults();
        }
        SpiHandle::Config spi_config;
        struct
        {
            dsy_gpio_pin dc;    /**< & */
            dsy_gpio_pin reset; /**< & */
        } pin_config;
        void Defaults()
        {
            // SPI peripheral config
            spi_config.periph = SpiHandle::Config::Peripheral::SPI_1;
            spi_config.mode   = SpiHandle::Config::Mode::MASTER;
            spi_config.direction
                = SpiHandle::Config::Direction::TWO_LINES_TX_ONLY;
            spi_config.datasize       = 8;
            spi_config.clock_polarity = SpiHandle::Config::ClockPolarity::LOW;
            spi_config.clock_phase    = SpiHandle::Config::ClockPhase::ONE_EDGE;
            spi_config.nss            = SpiHandle::Config::NSS::HARD_OUTPUT;
            spi_config.baud_prescaler = SpiHandle::Config::BaudPrescaler::PS_8;
            // SPI pin config
            spi_config.pin_config.sclk = {DSY_GPIOG, 11};
            spi_config.pin_config.miso = {DSY_GPIOX, 0};
            spi_config.pin_config.mosi = {DSY_GPIOB, 5};
            spi_config.pin_config.nss  = {DSY_GPIOG, 10};
            // SSD1327 control pin config
            pin_config.dc    = {DSY_GPIOB, 4};
            pin_config.reset = {DSY_GPIOB, 15};
        }
    };
    void Init(const Config& config)
    {
        // Initialize both GPIO
        pin_dc_.mode = DSY_GPIO_MODE_OUTPUT_PP;
        pin_dc_.pin  = config.pin_config.dc;
        dsy_gpio_init(&pin_dc_);
        pin_reset_.mode = DSY_GPIO_MODE_OUTPUT_PP;
        pin_reset_.pin  = config.pin_config.reset;
        dsy_gpio_init(&pin_reset_);

        // Initialize SPI
        spi_.Init(config.spi_config);

        // Reset and Configure OLED.
        dsy_gpio_write(&pin_reset_, 0);
        System::Delay(10);
        dsy_gpio_write(&pin_reset_, 1);
        System::Delay(10);
    };
    void SendCommand(uint8_t cmd)
    {
        dsy_gpio_write(&pin_dc_, 0);
        spi_.BlockingTransmit(&cmd, 1);
    };

    void SendData(uint8_t* buff, size_t size)
    {
        dsy_gpio_write(&pin_dc_, 1);
        spi_.BlockingTransmit(buff, size);
    };

  private:
    SpiHandle spi_;
    dsy_gpio  pin_reset_;
    dsy_gpio  pin_dc_;
};


/**
 * A driver implementation for the SSD1327
 */
template <size_t width, size_t height, typename Transport>
class SSD1327Driver
{
  public:
    struct Config
    {
        typename Transport::Config transport_config;
    };

    void Init(Config config)
    {
        transport_.Init(config.transport_config);

        transport_.SendCommand(0xae);	// turn off oled panel

        transport_.SendCommand(0x15);   // set column address
        transport_.SendCommand(0x00);   // start column   0
        transport_.SendCommand(0x7f);   // end column   127

        transport_.SendCommand(0x75);   // set row address
        transport_.SendCommand(0x00);   // start row   0
        transport_.SendCommand(0x7f);   // end row   127

        transport_.SendCommand(0x81);  	// set contrast control
        transport_.SendCommand(0x80);

        transport_.SendCommand(0xa0);   // gment remap
        transport_.SendCommand(0x51);	// 51

        transport_.SendCommand(0xa1);  	// start line
        transport_.SendCommand(0x00);

        transport_.SendCommand(0xa2);  	// display offset
        transport_.SendCommand(0x00);

        transport_.SendCommand(0xa4);   // rmal display
        transport_.SendCommand(0xa8);   // set multiplex ratio
        transport_.SendCommand(0x7f);

        transport_.SendCommand(0xb1);  	// set phase leghth
        transport_.SendCommand(0xf1);

        transport_.SendCommand(0xb3);  	// set dclk
        transport_.SendCommand(0x00);  	// 80Hz:0xc1 / 90Hz:0xe1 / 100Hz:0x00 / 110Hz:0x30 / 120Hz:0x50 / 130Hz:0x70

        transport_.SendCommand(0xab);
        transport_.SendCommand(0x01);

        transport_.SendCommand(0xb6);  	// set phase length
        transport_.SendCommand(0x0f);

        transport_.SendCommand(0xbe);
        transport_.SendCommand(0x0f);

        transport_.SendCommand(0xbc);
        transport_.SendCommand(0x08);

        transport_.SendCommand(0xd5);
        transport_.SendCommand(0x62);

        transport_.SendCommand(0xfd);
        transport_.SendCommand(0x12);

        System::Delay(200);				//	wait 200ms

        transport_.SendCommand(0xaf);	// turn on display
    };

    size_t Width() const { return width; };
    size_t Height() const { return height; };

    void DrawPixel(uint_fast8_t x, uint_fast8_t y, bool on)
    {
        if ((x >= width) || (y >= height))
            return;

        if (on)
            buffer_[x + (y / 2) * width] |= color_;
        else
            buffer_[x + (y / 2) * width] = 0;
    };

    void Fill(bool on)
    {
        for(size_t i = 0; i < sizeof(buffer_); i++)
        {
            buffer_[i] = on ? 0xff : 0x00;
        }
    };

    /**
     * Update the display 
    */
    void Update()
    {
        uint8_t *pBuf = buffer_;

        transport_.SendCommand(0x15);	// column
        transport_.SendCommand(0x00);
        transport_.SendCommand(width-1);

        transport_.SendCommand(0x75);	// row
        transport_.SendCommand(0x00);
        transport_.SendCommand(height-1);

        //write data
        for (uint32_t line = 0; line < height; line++) {
        	transport_.SendData(pBuf, width/2);
            pBuf+=width/2;
        }
    };

    void Set_Color(uint8_t in_col)
    {
    	color_ = in_col & 0x0f;
    };

  protected:
    Transport transport_;
    uint8_t   buffer_[width * height];
    uint8_t   color_;
};

/**
 * A driver for the SSD1327 128x128 OLED displays connected via 4 wire SPI
 */
using SSD13274WireSpi128x128Driver = daisy::SSD1327Driver<128, 128, SSD13274WireSpiTransport>;

}; // namespace daisy
