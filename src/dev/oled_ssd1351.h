#pragma once

#include "per/spi.h"
#include "per/gpio.h"
#include "sys/system.h"

namespace daisy
{

/**
 * 4 Wire SPI Transport for SSD1351 OLED display devices
 */
class SSD13514WireSpiTransport
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
            // SSD1351 control pin config
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
    void SendData(uint8_t data)
    {
        dsy_gpio_write(&pin_dc_, 1);
        spi_.BlockingTransmit(&data, 1);
    };

  private:
    SpiHandle spi_;
    dsy_gpio  pin_reset_;
    dsy_gpio  pin_dc_;
};


/**
 * A driver implementation for the SSD1351
 */
template <size_t width, size_t height, typename Transport>
class SSD1351Driver
{
  public:
    struct Config
    {
        typename Transport::Config transport_config;
    };

    void Init(Config config)
    {
        fg_color_ = 0xffff;
        bg_color_ = 0x0000;
        transport_.Init(config.transport_config);

    	transport_.SendCommand(0xae);	// display off
    	transport_.SendCommand(0xa4);	// Normal Display mode

    	transport_.SendCommand(0x15);	// set column address
    	transport_.SendData(0x00);     	// column address start 00
    	transport_.SendData(0x7f);     	// column address end 95
    	transport_.SendCommand(0x75);	// set row address
    	transport_.SendData(0x00);     	// row address start 00
    	transport_.SendData(0x7f);     	// row address end 63

    	transport_.SendCommand(0xB3);
    	transport_.SendData(0xF1);

    	transport_.SendCommand(0xCA);
    	transport_.SendData(0x7F);

    	transport_.SendCommand(0xa0);  	// set re-map & data format
    	transport_.SendData(0x74);     	// Horizontal address increment

    	transport_.SendCommand(0xa1);  	// set display start line
    	transport_.SendData(0x00);     	// start 00 line

    	transport_.SendCommand(0xa2);  	// set display offset
    	transport_.SendData(0x00);

    	transport_.SendCommand(0xAB);
    	transport_.SendCommand(0x01);

    	transport_.SendCommand(0xB4);
    	transport_.SendData(0xA0);
    	transport_.SendData(0xB5);
    	transport_.SendData(0x55);

    	transport_.SendCommand(0xC1);
    	transport_.SendData(0xC8);
    	transport_.SendData(0x80);
    	transport_.SendData(0xC0);

    	transport_.SendCommand(0xC7);
    	transport_.SendData(0x0F);

    	transport_.SendCommand(0xB1);
    	transport_.SendData(0x32);

    	transport_.SendCommand(0xB2);
    	transport_.SendData(0xA4);
    	transport_.SendData(0x00);
    	transport_.SendData(0x00);

    	transport_.SendCommand(0xBB);
    	transport_.SendData(0x17);

    	transport_.SendCommand(0xB6);
    	transport_.SendData(0x01);

    	transport_.SendCommand(0xBE);
    	transport_.SendData(0x05);

    	transport_.SendCommand(0xA6);

    	System::Delay(200);				//	wait 200ms
        transport_.SendCommand(0xaf);	// turn on display
        Fill(false);
    };

    size_t Width() const { return width; };
    size_t Height() const { return height; };

    void DrawPixel(uint_fast8_t x, uint_fast8_t y, bool on)
    {
        if ((x >= width) || (y >= height))
            return;

        if (on) {
        	buffer_[(y * width) + x] = fg_color_;
        }
        else {
        	buffer_[(y * width) + x] = bg_color_;
        }
    };

    void Fill(bool on)
    {
        for(size_t i = 0; i < sizeof(buffer_)/2; i++)
        {
            buffer_[i] = on ? fg_color_ : bg_color_;
        }
    };

    /**
     * Update the display 
    */
    void Update()
    {
        transport_.SendCommand(0x15);	// column
        transport_.SendCommand(0x00);
        transport_.SendCommand(width-1);

        transport_.SendCommand(0x75);	// row
        transport_.SendCommand(0x00);
        transport_.SendCommand(height-1);

        //write data
        transport_.SendData((uint8_t*)buffer_, sizeof(buffer_));
    };

    void Set_FgColor(uint16_t in_col)
    {
    	fg_color_ = in_col;
    };

    void Set_BgColor(uint16_t in_col)
    {
    	bg_color_ = in_col;
    };

  protected:
    Transport transport_;
    uint16_t  buffer_[width * height];
    uint16_t  fg_color_;
    uint16_t  bg_color_;
};

/**
 * A driver for the SSD1351 128x128 OLED displays connected via 4 wire SPI
 */
using SSD13514WireSpi128x128Driver = daisy::SSD1351Driver<128, 128, SSD13514WireSpiTransport>;

}; // namespace daisy
