#ifndef POTENTIOMETER_VOLUME_H
#define POTENTIOMETER_VOLUME_H

#include <cstdint>

enum class PotentiometerResult
{
    NotDue,
    Sampled,
    VolumeChanged,
    InitializationFailed,
    ConversionFailed
};

struct PotentiometerReading
{
    uint16_t raw = 0;
    uint16_t filteredRaw = 0;
    uint16_t observedMinimum = 0;
    uint16_t observedMaximum = 0;
    uint8_t volume = 0;
    bool valid = false;
};

// PF6 = ADC3_IN4 (STM32F407 datasheet, Table 7). No ADC DMA or IRQs.
// Initialization/retries never discard a previously accepted volume.
bool PotentiometerVolume_Init();

// Normal calls sample at most once per 20 ms. forceSample is for the four
// startup conversions; initialization retry delays still apply.
PotentiometerResult PotentiometerVolume_Update(uint32_t now,
                                             bool forceSample = false);
PotentiometerReading PotentiometerVolume_GetReading();

#endif
