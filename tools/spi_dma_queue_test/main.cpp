/** SPI DMA queue test: is a queued transfer lost when a third one is
 *  queued behind it?
 *
 *  SpiHandle runs one DMA transfer at a time and has one queue slot per
 *  peripheral. Each run starts three transfers back to back:
 *      A  starts immediately (DMA idle)
 *      B  DMA busy, so B goes into the queue slot
 *      C  DMA busy and the slot is full, so QueueDmaTransfer() is supposed
 *         to wait until the DMA interrupt starts B and frees the slot
 *  Expected: three end callbacks, in the order A, B, C.
 *
 *  Stock libDaisy has two bugs here (see README.md):
 *   1. SpiDmaJob::IsValidJob() requires both an RX and a TX buffer, so a
 *      queued transmit-only job never counts as queued. The interrupt never
 *      starts B or C: only A's callback arrives.
 *   2. The wait loop in QueueDmaTransfer() polls a non-volatile array that
 *      only the DMA interrupt changes. GCC 10+ deletes it (-ffinite-loops),
 *      so C overwrites B in the slot. With bug 1 fixed, A then C arrive.
 *  With both fixed: A, B, C.
 *
 *  Results:
 *      LED     on after a run that passed, off after one that failed
 *      Serial  one line per run over USB (any serial terminal)
 *      Saleae  decode SPI on SCLK/MOSI/NSS. Each transfer is 64 bytes of
 *              one value: 0xAA (A), 0xBB (B), 0xCC (C).
 *
 *  Logic analyzer channels (Seed pins; on a Daisy Pod these are the SPI1
 *  header pins - D1-D6 are the Pod's SD card, so don't use those):
 *      NSS   D7   SPI1 chip select, low during each transfer
 *      SCLK  D8   SPI1 clock, about 390 kHz
 *      Done  D9   short pulse from each transfer's end callback
 *      MOSI  D10  SPI1 data
 */
#include "daisy_seed.h"

using namespace daisy;

static constexpr size_t   kTransferSize = 64; // about 1.3 ms each at PS_256
static constexpr uint32_t kRunPeriodMs  = 1000;
static constexpr uint32_t kSettleMs     = 100; // all three finish in ~4 ms

static DaisySeed hw;
static SpiHandle spi;
static GPIO      pin_done;

static uint8_t DMA_BUFFER_MEM_SECTION buf_a[kTransferSize];
static uint8_t DMA_BUFFER_MEM_SECTION buf_b[kTransferSize];
static uint8_t DMA_BUFFER_MEM_SECTION buf_c[kTransferSize];

/* Written by the end callback (DMA interrupt), read by main. volatile for
 * the same reason as the bug under test: otherwise GCC may read them once
 * and never see the interrupt's updates. */
static volatile size_t done_count;
static volatile char   done_order[4];

static void EndCallback(void* context, SpiHandle::Result result)
{
    pin_done.Write(true);
    const size_t n = done_count;
    if(n < sizeof(done_order))
        done_order[n] = (result == SpiHandle::Result::OK)
                            ? static_cast<char>(reinterpret_cast<uintptr_t>(
                                context))
                            : '!';
    done_count = n + 1;
    pin_done.Write(false);
}

static void* Tag(char id)
{
    return reinterpret_cast<void*>(static_cast<uintptr_t>(id));
}

int main(void)
{
    hw.Init();
    hw.StartLog(false);

    for(size_t i = 0; i < kTransferSize; i++)
    {
        buf_a[i] = 0xAA;
        buf_b[i] = 0xBB;
        buf_c[i] = 0xCC;
    }

    pin_done.Init(seed::D9, GPIO::Mode::OUTPUT);

    SpiHandle::Config cfg;
    cfg.periph            = SpiHandle::Config::Peripheral::SPI_1;
    cfg.mode              = SpiHandle::Config::Mode::MASTER;
    cfg.direction         = SpiHandle::Config::Direction::TWO_LINES_TX_ONLY;
    cfg.nss               = SpiHandle::Config::NSS::HARD_OUTPUT;
    cfg.baud_prescaler    = SpiHandle::Config::BaudPrescaler::PS_256;
    cfg.pin_config.nss    = seed::D7;
    cfg.pin_config.sclk   = seed::D8;
    cfg.pin_config.miso   = Pin();
    cfg.pin_config.mosi   = seed::D10;
    spi.Init(cfg);

    uint32_t run    = 0;
    uint32_t passed = 0;
    uint32_t failed = 0;
    while(1)
    {
        const uint32_t start = System::GetNow();

        done_count = 0;
        for(size_t i = 0; i < sizeof(done_order); i++)
            done_order[i] = '-';

        spi.DmaTransmit(buf_a, kTransferSize, nullptr, EndCallback, Tag('A'));
        spi.DmaTransmit(buf_b, kTransferSize, nullptr, EndCallback, Tag('B'));
        spi.DmaTransmit(buf_c, kTransferSize, nullptr, EndCallback, Tag('C'));

        System::Delay(kSettleMs);

        const size_t count = done_count;
        const bool   ok    = count == 3 && done_order[0] == 'A'
                        && done_order[1] == 'B' && done_order[2] == 'C';
        ok ? passed++ : failed++;
        hw.SetLed(ok);
        hw.PrintLine("run %lu: %u callbacks, order %c%c%c  %s  (pass %lu, fail %lu)",
                     run++,
                     static_cast<unsigned>(count),
                     done_order[0],
                     done_order[1],
                     done_order[2],
                     ok ? "PASS" : "FAIL",
                     passed,
                     failed);

        while(System::GetNow() - start < kRunPeriodMs) {}
    }
}
