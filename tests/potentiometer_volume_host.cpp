#include "potentiometer_filter.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>

namespace {

void settle(PotentiometerFilter &filter, uint32_t raw)
{
    for (uint32_t i = 0; i < PotentiometerFilter::SampleCount; ++i)
        filter.addSample(raw);
}

void startupUsesOnlyAvailableSamples()
{
    PotentiometerFilter silent;
    assert(!silent.hasValidSample());
    assert(silent.volume() == 0);
    assert(silent.filteredRaw() == 0);
    assert(!silent.addSample(0));
    assert(silent.hasValidSample());
    assert(silent.volume() == 0);

    PotentiometerFilter high;
    assert(high.addSample(4095));
    assert(high.filteredRaw() == 4095);
    assert(high.volume() == 100);
    assert(!high.addSample(4095));

    // A real initial one-percent setting bypasses the runtime deadband.
    PotentiometerFilter low;
    assert(low.addSample(41));
    assert(low.volume() == 1);
    assert(low.filteredRaw() == 41);

    PotentiometerFilter startup;
    startup.addSample(1000);
    startup.addSample(1100);
    assert(startup.filteredRaw() == 1050);
    startup.addSample(1200);
    assert(startup.filteredRaw() == 1100);
    startup.addSample(1300);
    assert(startup.filteredRaw() == 1150);
}

void rollingWindowEvictsOldReadings()
{
    PotentiometerFilter filter;
    for (uint32_t sample : {1000u, 1100u, 1200u, 1300u})
        filter.addSample(sample);
    filter.addSample(1700);
    assert(filter.filteredRaw() == 1325);
    filter.addSample(1800);
    assert(filter.filteredRaw() == 1500);
    settle(filter, 4095);
    assert(filter.filteredRaw() == 4095);
    assert(filter.volume() == 100);
    settle(filter, 0);
    assert(filter.filteredRaw() == 0);
    assert(filter.volume() == 0);
}

void mappingRoundsAndClampsSafely()
{
    struct Mapping { uint32_t raw; uint8_t expected; };
    const Mapping cases[] = {
        {0, 0}, {20, 0}, {21, 1}, {2047, 50}, {2048, 50},
        {2860, 70}, {4074, 99}, {4075, 100}, {4095, 100},
        {4096, 100}, {65536, 100},
        {std::numeric_limits<uint32_t>::max(), 100}
    };
    for (const Mapping &entry : cases)
    {
        PotentiometerFilter filter;
        const bool changed = filter.addSample(entry.raw);
        assert(filter.volume() == entry.expected);
        assert(changed == (entry.expected != 0));
        assert(filter.filteredRaw() <= PotentiometerFilter::AdcMaximum);
    }

    // Clamping happens before narrowing and before adding to the history.
    PotentiometerFilter filter;
    settle(filter, std::numeric_limits<uint32_t>::max());
    assert(filter.filteredRaw() == 4095);
    assert(filter.volume() == 100);
}

void stationaryNoiseDoesNotEmitChanges()
{
    PotentiometerFilter filter;
    settle(filter, 2048);
    assert(filter.volume() == 50);

    // These readings straddle a rounded percentage boundary. Even a sustained
    // one-point shift is too small to reopen the UI overlay.
    for (unsigned repeat = 0; repeat < 100; ++repeat)
    {
        for (uint32_t sample : {2027u, 2069u, 2047u, 2049u})
            assert(!filter.addSample(sample));
    }
    for (unsigned i = 0; i < 20; ++i)
        assert(!filter.addSample(2090));
    assert(filter.volume() == 50);

    // Real movement across two percentage points emits an accepted change.
    unsigned changes = 0;
    for (unsigned i = 0; i < 4; ++i)
        changes += filter.addSample(2130) ? 1u : 0u;
    assert(changes == 1);
    assert(filter.volume() == 52);
    assert(!filter.addSample(2130));
}

void physicalEndpointsBypassOnePointGap()
{
    PotentiometerFilter low;
    settle(low, 41);
    assert(low.volume() == 1);
    unsigned lowChanges = 0;
    for (unsigned i = 0; i < 4; ++i)
        lowChanges += low.addSample(0) ? 1u : 0u;
    assert(lowChanges == 1);
    assert(low.volume() == 0);
    assert(!low.addSample(0));

    PotentiometerFilter high;
    settle(high, 4054);
    assert(high.volume() == 99);
    unsigned highChanges = 0;
    for (unsigned i = 0; i < 4; ++i)
        highChanges += high.addSample(4095) ? 1u : 0u;
    assert(highChanges == 1);
    assert(high.volume() == 100);
    assert(!high.addSample(4095));
}

void rapidMovementSettlesWithinFourValidSamples()
{
    PotentiometerFilter filter;
    settle(filter, 0);
    const uint8_t rising[] = {25, 50, 75, 100};
    for (uint8_t expected : rising)
    {
        assert(filter.addSample(4095));
        assert(filter.volume() == expected);
    }
    const uint8_t falling[] = {75, 50, 25, 0};
    for (uint8_t expected : falling)
    {
        assert(filter.addSample(0));
        assert(filter.volume() == expected);
    }
}

void missingConversionsPreserveAcceptedVolumeAndHistory()
{
    PotentiometerFilter filter;
    settle(filter, 2860);
    assert(filter.volume() == 70);
    const uint16_t previousRaw = filter.filteredRaw();
    const uint8_t previousVolume = filter.volume();

    // The acquisition contract omits addSample() on any HAL failure. Reads of
    // the retained state, including a long gap, cannot manufacture zero input.
    for (unsigned missedConversions = 0; missedConversions < 1000;
         ++missedConversions)
    {
        assert(filter.hasValidSample());
        assert(filter.filteredRaw() == previousRaw);
        assert(filter.volume() == previousVolume);
    }
    assert(!filter.addSample(2860));
    assert(filter.filteredRaw() == previousRaw);

    PotentiometerFilter neverSucceeded;
    assert(!neverSucceeded.hasValidSample());
    assert(neverSucceeded.volume() == 0);
    assert(neverSucceeded.addSample(4095));
    assert(neverSucceeded.volume() == 100);
}

void fullKnobSweepStaysMonotonicAndInRange()
{
    PotentiometerFilter filter;
    uint8_t previousVolume = 0;
    for (uint32_t raw = 0; raw <= 4095; ++raw)
    {
        const bool changed = filter.addSample(raw);
        assert(changed == (filter.volume() != previousVolume));
        assert(filter.volume() >= previousVolume);
        assert(filter.volume() <= 100);
        previousVolume = filter.volume();
    }
    settle(filter, 4095);
    assert(filter.volume() == 100);
    previousVolume = filter.volume();
    for (int raw = 4095; raw >= 0; --raw)
    {
        const bool changed = filter.addSample(static_cast<uint32_t>(raw));
        assert(changed == (filter.volume() != previousVolume));
        assert(filter.volume() <= previousVolume);
        previousVolume = filter.volume();
    }
    settle(filter, 0);
    assert(filter.volume() == 0);
}

} // namespace

int main()
{
    startupUsesOnlyAvailableSamples();
    rollingWindowEvictsOldReadings();
    mappingRoundsAndClampsSafely();
    stationaryNoiseDoesNotEmitChanges();
    physicalEndpointsBypassOnePointGap();
    rapidMovementSettlesWithinFourValidSamples();
    missingConversionsPreserveAcceptedVolumeAndHistory();
    fullKnobSweepStaysMonotonicAndInRange();
    std::cout << "Potentiometer filter: 8 groups passed\n";
}
