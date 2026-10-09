#ifndef POTENTIOMETER_FILTER_H
#define POTENTIOMETER_FILTER_H

#include <cstdint>

// ADC acquisition supplies successful conversions only. A failed conversion
// must not be added, so both the history and accepted volume remain intact.
class PotentiometerFilter
{
public:
    static constexpr uint32_t ResolutionBits = 12;
    static constexpr uint32_t AdcMaximum = (1u << ResolutionBits) - 1u;
    static constexpr uint32_t SampleCount = 4;
    static constexpr uint32_t Deadband = 2;

    // True means the single accepted logical volume changed. The first valid
    // reading is accepted without a deadband; an initial zero is still valid.
    bool addSample(uint32_t raw)
    {
        if (raw > AdcMaximum) raw = AdcMaximum;

        const bool firstSample = sampleCount == 0;
        sampleSum -= samples[nextSample];
        samples[nextSample] = static_cast<uint16_t>(raw);
        sampleSum += raw;
        nextSample = static_cast<uint8_t>((nextSample + 1u) % SampleCount);
        if (sampleCount < SampleCount) ++sampleCount;

        // At startup average only the populated entries, without zero padding.
        averageRaw = static_cast<uint16_t>(
            (sampleSum + sampleCount / 2u) / sampleCount);
        uint32_t mapped =
            (static_cast<uint32_t>(averageRaw) * 100u + AdcMaximum / 2u) /
            AdcMaximum;
        if (mapped > 100u) mapped = 100u;
        const uint8_t candidate = static_cast<uint8_t>(mapped);
        const uint32_t difference = candidate > acceptedVolume
                                        ? candidate - acceptedVolume
                                        : acceptedVolume - candidate;

        // Small movements/noise are suppressed, but an adjacent accepted 1 or
        // 99 must never prevent the physical endpoints reaching 0 or 100.
        const bool accept = firstSample || difference >= Deadband ||
                            candidate == 0 || candidate == 100;
        if (accept && candidate != acceptedVolume)
        {
            acceptedVolume = candidate;
            return true;
        }
        return false;
    }

    uint8_t volume() const { return acceptedVolume; }
    uint16_t filteredRaw() const { return averageRaw; }
    bool hasValidSample() const { return sampleCount != 0; }

private:
    uint16_t samples[SampleCount]{};
    uint32_t sampleSum = 0;
    uint16_t averageRaw = 0;
    uint8_t sampleCount = 0;
    uint8_t nextSample = 0;
    uint8_t acceptedVolume = 0;
};

#endif
