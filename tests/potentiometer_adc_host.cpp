#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "adc_stubs/stm32f4xx_hal.h"

// Compile the actual acquisition implementation, allowing each scenario to
// start with fresh private state without adding test hooks to board firmware.
#include "../src/potentiometer_volume.cpp"

GPIO_TypeDef hostGPIOF;
ADC_TypeDef hostADC3;

namespace
{
struct FakeHAL
{
    uint32_t tick = 0;
    uint32_t raw = 0;
    HAL_StatusTypeDef initStatus = HAL_OK;
    HAL_StatusTypeDef configStatus = HAL_OK;
    HAL_StatusTypeDef startStatus = HAL_OK;
    HAL_StatusTypeDef pollStatus = HAL_OK;
    HAL_StatusTypeDef stopStatus = HAL_OK;
    int gpioClockCalls = 0;
    int adcClockCalls = 0;
    int gpioCalls = 0;
    int initCalls = 0;
    int configCalls = 0;
    int startCalls = 0;
    int pollCalls = 0;
    int getValueCalls = 0;
    int stopCalls = 0;
    uint32_t pollTimeout = 0;
    GPIO_TypeDef *gpioPort = nullptr;
    GPIO_InitTypeDef gpioConfiguration = {};
    ADC_HandleTypeDef adcConfiguration = {};
    ADC_ChannelConfTypeDef channelConfiguration = {};
    std::vector<std::string> events;
};

FakeHAL fake;
int checks = 0;
int scenarios = 0;

void require(bool condition, const char *message, int line)
{
    ++checks;
    if (!condition)
    {
        std::cerr << "FAIL line " << line << ": " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

#define CHECK(condition) require((condition), #condition, __LINE__)

void resetModule(uint32_t tick = 0)
{
    fake = FakeHAL{};
    fake.tick = tick;
    adc = ADC_HandleTypeDef{};
    filter = PotentiometerFilter{};
    initialized = false;
    initializationAttempted = false;
    stopNeedsRetry = false;
    samplingStarted = false;
    lastInitializationAttempt = 0;
    lastSample = 0;
    lastRaw = 0;
    observedMinimum = 0;
    observedMaximum = 0;
    ++scenarios;
}

PotentiometerResult updateAt(uint32_t tick, bool force = false)
{
    fake.tick = tick;
    return PotentiometerVolume_Update(tick, force);
}

void sameReading(const PotentiometerReading &a, const PotentiometerReading &b)
{
    CHECK(a.raw == b.raw);
    CHECK(a.filteredRaw == b.filteredRaw);
    CHECK(a.observedMinimum == b.observedMinimum);
    CHECK(a.observedMaximum == b.observedMaximum);
    CHECK(a.volume == b.volume);
    CHECK(a.valid == b.valid);
}

void acceptFirst(uint32_t value)
{
    CHECK(PotentiometerVolume_Init());
    fake.raw = value;
    CHECK(updateAt(fake.tick, true) == PotentiometerResult::VolumeChanged);
    CHECK(PotentiometerVolume_GetReading().valid);
}

void testPinAndAdcConfiguration()
{
    resetModule();
    CHECK(PotentiometerVolume_Init());
    CHECK(fake.gpioClockCalls == 1);
    CHECK(fake.adcClockCalls == 1);
    CHECK(fake.gpioCalls == 1);
    CHECK(fake.gpioPort == GPIOF);
    // The exact mask also guards every other working pin on GPIOF.
    CHECK(fake.gpioConfiguration.Pin == GPIO_PIN_6);
    CHECK(fake.gpioConfiguration.Mode == GPIO_MODE_ANALOG);
    CHECK(fake.gpioConfiguration.Pull == GPIO_NOPULL);
    CHECK(fake.initCalls == 1);
    CHECK(fake.adcConfiguration.Instance == ADC3);
    const ADC_InitTypeDef &configuration = fake.adcConfiguration.Init;
    CHECK(configuration.ClockPrescaler == ADC_CLOCK_SYNC_PCLK_DIV4);
    CHECK(configuration.Resolution == ADC_RESOLUTION_12B);
    CHECK(configuration.DataAlign == ADC_DATAALIGN_RIGHT);
    CHECK(configuration.ScanConvMode == DISABLE);
    CHECK(configuration.ContinuousConvMode == DISABLE);
    CHECK(configuration.DiscontinuousConvMode == DISABLE);
    CHECK(configuration.ExternalTrigConvEdge == ADC_EXTERNALTRIGCONVEDGE_NONE);
    CHECK(configuration.ExternalTrigConv == ADC_SOFTWARE_START);
    CHECK(configuration.NbrOfConversion == 1);
    CHECK(configuration.DMAContinuousRequests == DISABLE);
    CHECK(configuration.EOCSelection == ADC_EOC_SINGLE_CONV);
    CHECK(fake.configCalls == 1);
    CHECK(fake.channelConfiguration.Channel == ADC_CHANNEL_4);
    CHECK(fake.channelConfiguration.Rank == 1);
    CHECK(fake.channelConfiguration.SamplingTime == ADC_SAMPLETIME_480CYCLES);
    CHECK(fake.startCalls == 0);
    CHECK(!PotentiometerVolume_GetReading().valid);
}

void testValidZeroAndBoundedPolling()
{
    resetModule();
    CHECK(PotentiometerVolume_Init());
    CHECK(updateAt(0, true) == PotentiometerResult::Sampled);
    const PotentiometerReading reading = PotentiometerVolume_GetReading();
    CHECK(reading.valid);
    CHECK(reading.raw == 0);
    CHECK(reading.filteredRaw == 0);
    CHECK(reading.volume == 0);
    CHECK(reading.observedMinimum == 0);
    CHECK(reading.observedMaximum == 0);
    CHECK(fake.startCalls == 1);
    CHECK(fake.pollCalls == 1);
    CHECK(fake.pollTimeout == 1);
    CHECK(fake.getValueCalls == 1);
    CHECK(fake.stopCalls == 1);
    CHECK(fake.events.size() == 7);
    CHECK(fake.events[3] == "start");
    CHECK(fake.events[4] == "poll");
    CHECK(fake.events[5] == "value");
    CHECK(fake.events[6] == "stop");
}

void testFourForcedStartupSamples()
{
    resetModule(123);
    CHECK(PotentiometerVolume_Init());
    for (uint32_t value = 1000; value <= 4000; value += 1000)
    {
        fake.raw = value;
        CHECK(updateAt(123, true) == PotentiometerResult::VolumeChanged);
    }
    const PotentiometerReading reading = PotentiometerVolume_GetReading();
    CHECK(reading.raw == 4000);
    CHECK(reading.filteredRaw == 2500);
    CHECK(reading.volume == 61);
    CHECK(reading.observedMinimum == 1000);
    CHECK(reading.observedMaximum == 4000);
    CHECK(fake.startCalls == 4);
    CHECK(fake.stopCalls == 4);
    CHECK(updateAt(123) == PotentiometerResult::NotDue);
    CHECK(updateAt(142) == PotentiometerResult::NotDue);
    CHECK(fake.startCalls == 4);
    CHECK(updateAt(143) != PotentiometerResult::NotDue);
    CHECK(fake.startCalls == 5);
}

void testSamplingCadenceAndForce()
{
    resetModule();
    CHECK(PotentiometerVolume_Init());
    CHECK(updateAt(0) == PotentiometerResult::Sampled);
    CHECK(updateAt(19) == PotentiometerResult::NotDue);
    CHECK(updateAt(20) == PotentiometerResult::Sampled);
    CHECK(updateAt(39) == PotentiometerResult::NotDue);
    CHECK(updateAt(40) == PotentiometerResult::Sampled);
    CHECK(fake.startCalls == 3);
    CHECK(updateAt(40, true) == PotentiometerResult::Sampled);
    CHECK(updateAt(40, true) == PotentiometerResult::Sampled);
    CHECK(fake.startCalls == 5);
}

void testSamplingAcrossTickWrap()
{
    const uint32_t beforeWrap = std::numeric_limits<uint32_t>::max() - 10u;
    resetModule(beforeWrap);
    CHECK(PotentiometerVolume_Init());
    CHECK(updateAt(beforeWrap) == PotentiometerResult::Sampled);
    CHECK(updateAt(8) == PotentiometerResult::NotDue);
    CHECK(updateAt(9) == PotentiometerResult::Sampled);
    CHECK(fake.startCalls == 2);
}

void testInitFailureAndDelayedRecovery()
{
    resetModule();
    fake.initStatus = HAL_ERROR;
    CHECK(!PotentiometerVolume_Init());
    CHECK(fake.initCalls == 1);
    CHECK(fake.configCalls == 0);
    const PotentiometerReading empty = PotentiometerVolume_GetReading();
    CHECK(!empty.valid && empty.volume == 0);
    // Even forced startup reads must not flood a failing peripheral.
    for (uint32_t tick : {0u, 20u, 500u, 999u})
        CHECK(updateAt(tick, true) == PotentiometerResult::NotDue);
    CHECK(fake.initCalls == 1);
    CHECK(fake.startCalls == 0);
    CHECK(updateAt(1000, true) == PotentiometerResult::InitializationFailed);
    CHECK(fake.initCalls == 2);
    sameReading(empty, PotentiometerVolume_GetReading());
    fake.initStatus = HAL_OK;
    fake.raw = 2048;
    CHECK(updateAt(1999, true) == PotentiometerResult::NotDue);
    CHECK(updateAt(2000, true) == PotentiometerResult::VolumeChanged);
    CHECK(fake.initCalls == 3);
    CHECK(fake.configCalls == 1);
    CHECK(fake.startCalls == 1);
    CHECK(PotentiometerVolume_GetReading().volume == 50);
    CHECK(PotentiometerVolume_GetReading().valid);
}

void testConfigFailureAndDelayedRecovery()
{
    resetModule();
    fake.configStatus = HAL_ERROR;
    CHECK(!PotentiometerVolume_Init());
    CHECK(fake.initCalls == 1 && fake.configCalls == 1);
    CHECK(updateAt(999, true) == PotentiometerResult::NotDue);
    CHECK(updateAt(1000, true) == PotentiometerResult::InitializationFailed);
    CHECK(fake.initCalls == 2 && fake.configCalls == 2);
    CHECK(fake.startCalls == 0);
    CHECK(!PotentiometerVolume_GetReading().valid);
    fake.configStatus = HAL_OK;
    fake.raw = 4095;
    CHECK(updateAt(2000, true) == PotentiometerResult::VolumeChanged);
    CHECK(PotentiometerVolume_GetReading().volume == 100);
}

void testStartFailureKeepsAllAcceptedData()
{
    resetModule();
    acceptFirst(1500);
    const PotentiometerReading before = PotentiometerVolume_GetReading();
    fake.startStatus = HAL_BUSY;
    fake.raw = 0;
    CHECK(updateAt(20) == PotentiometerResult::ConversionFailed);
    CHECK(fake.startCalls == 2);
    CHECK(fake.pollCalls == 1);
    CHECK(fake.getValueCalls == 1);
    CHECK(fake.stopCalls == 2);
    sameReading(before, PotentiometerVolume_GetReading());
    fake.startStatus = HAL_OK;
    fake.raw = 1500;
    CHECK(updateAt(40) == PotentiometerResult::Sampled);
    CHECK(fake.initCalls == 1);
    CHECK(PotentiometerVolume_GetReading().filteredRaw == 1500);
}

void testPollFailureKeepsAllAcceptedData()
{
    resetModule();
    acceptFirst(1500);
    const PotentiometerReading before = PotentiometerVolume_GetReading();
    fake.pollStatus = HAL_TIMEOUT;
    fake.raw = 4095;
    CHECK(updateAt(20) == PotentiometerResult::ConversionFailed);
    CHECK(fake.startCalls == 2);
    CHECK(fake.pollCalls == 2);
    CHECK(fake.getValueCalls == 1);
    CHECK(fake.stopCalls == 2);
    sameReading(before, PotentiometerVolume_GetReading());
    fake.pollStatus = HAL_OK;
    fake.raw = 1500;
    CHECK(updateAt(40) == PotentiometerResult::Sampled);
    CHECK(PotentiometerVolume_GetReading().filteredRaw == 1500);
}

void testStopFailureAndCheckedRecovery()
{
    resetModule();
    acceptFirst(1000);
    const PotentiometerReading before = PotentiometerVolume_GetReading();
    fake.stopStatus = HAL_ERROR;
    fake.raw = 4095;
    CHECK(updateAt(20) == PotentiometerResult::ConversionFailed);
    CHECK(fake.getValueCalls == 2);
    sameReading(before, PotentiometerVolume_GetReading());
    CHECK(updateAt(21, true) == PotentiometerResult::NotDue);
    CHECK(updateAt(1019, true) == PotentiometerResult::NotDue);
    CHECK(fake.stopCalls == 2 && fake.initCalls == 1);
    // Retrying stop is itself checked; failure must prevent initialization.
    CHECK(updateAt(1020, true) == PotentiometerResult::InitializationFailed);
    CHECK(fake.stopCalls == 3 && fake.initCalls == 1);
    sameReading(before, PotentiometerVolume_GetReading());
    fake.stopStatus = HAL_OK;
    fake.raw = 2000;
    CHECK(updateAt(2019, true) == PotentiometerResult::NotDue);
    fake.events.clear();
    CHECK(updateAt(2020, true) == PotentiometerResult::VolumeChanged);
    CHECK(fake.stopCalls == 5);
    CHECK(fake.initCalls == 2);
    CHECK(fake.events.size() == 8);
    CHECK(fake.events[0] == "stop");
    CHECK(fake.events[1] == "gpio");
    CHECK(fake.events[2] == "init");
    CHECK(fake.events[3] == "config");
    CHECK(PotentiometerVolume_GetReading().raw == 2000);
    // The failed 4095 conversion never entered the rolling history.
    CHECK(PotentiometerVolume_GetReading().filteredRaw == 1500);
    CHECK(PotentiometerVolume_GetReading().observedMinimum == 1000);
    CHECK(PotentiometerVolume_GetReading().observedMaximum == 2000);
}

void testBothStartAndStopFailure()
{
    resetModule();
    acceptFirst(2000);
    const PotentiometerReading before = PotentiometerVolume_GetReading();
    fake.startStatus = HAL_ERROR;
    fake.stopStatus = HAL_ERROR;
    CHECK(updateAt(20) == PotentiometerResult::ConversionFailed);
    CHECK(fake.pollCalls == 1 && fake.getValueCalls == 1);
    CHECK(fake.stopCalls == 2);
    sameReading(before, PotentiometerVolume_GetReading());
    fake.startStatus = fake.stopStatus = HAL_OK;
    fake.raw = 2000;
    CHECK(updateAt(1019, true) == PotentiometerResult::NotDue);
    CHECK(updateAt(1020, true) == PotentiometerResult::Sampled);
    CHECK(fake.initCalls == 2);
    CHECK(fake.stopCalls == 4);
}

void testExtremaUseSuccessfulSamplesOnly()
{
    resetModule();
    acceptFirst(2000);
    fake.raw = 2500;
    CHECK(updateAt(20) == PotentiometerResult::VolumeChanged);
    const PotentiometerReading before = PotentiometerVolume_GetReading();
    fake.raw = 0;
    fake.pollStatus = HAL_TIMEOUT;
    CHECK(updateAt(40) == PotentiometerResult::ConversionFailed);
    sameReading(before, PotentiometerVolume_GetReading());
    fake.raw = 4095;
    fake.startStatus = HAL_BUSY;
    CHECK(updateAt(60) == PotentiometerResult::ConversionFailed);
    sameReading(before, PotentiometerVolume_GetReading());
    fake.startStatus = fake.pollStatus = HAL_OK;
    fake.raw = 1000;
    CHECK(updateAt(80) == PotentiometerResult::VolumeChanged);
    fake.raw = 3000;
    CHECK(updateAt(100) == PotentiometerResult::VolumeChanged);
    const PotentiometerReading reading = PotentiometerVolume_GetReading();
    CHECK(reading.raw == 3000);
    CHECK(reading.filteredRaw == 2125);
    CHECK(reading.observedMinimum == 1000);
    CHECK(reading.observedMaximum == 3000);
}

void testInitialConversionFailureRemainsZero()
{
    resetModule();
    CHECK(PotentiometerVolume_Init());
    fake.pollStatus = HAL_TIMEOUT;
    fake.raw = 4095;
    CHECK(updateAt(0, true) == PotentiometerResult::ConversionFailed);
    const PotentiometerReading empty = PotentiometerVolume_GetReading();
    CHECK(!empty.valid);
    CHECK(empty.raw == 0 && empty.filteredRaw == 0 && empty.volume == 0);
    CHECK(empty.observedMinimum == 0 && empty.observedMaximum == 0);
    fake.pollStatus = HAL_OK;
    fake.raw = 1000;
    CHECK(updateAt(20) == PotentiometerResult::VolumeChanged);
    CHECK(PotentiometerVolume_GetReading().filteredRaw == 1000);
    CHECK(PotentiometerVolume_GetReading().observedMinimum == 1000);
    CHECK(PotentiometerVolume_GetReading().valid);
}

void testOutOfRangeIsClamped()
{
    resetModule();
    acceptFirst(std::numeric_limits<uint32_t>::max());
    const PotentiometerReading reading = PotentiometerVolume_GetReading();
    CHECK(reading.raw == 4095);
    CHECK(reading.filteredRaw == 4095);
    CHECK(reading.observedMinimum == 4095 && reading.observedMaximum == 4095);
    CHECK(reading.volume == 100);
}

void testInitializationRetryAcrossTickWrap()
{
    const uint32_t beforeWrap = std::numeric_limits<uint32_t>::max() - 99u;
    resetModule(beforeWrap);
    fake.initStatus = HAL_ERROR;
    CHECK(!PotentiometerVolume_Init());
    CHECK(updateAt(899, true) == PotentiometerResult::NotDue);
    fake.initStatus = HAL_OK;
    fake.raw = 0;
    CHECK(updateAt(900, true) == PotentiometerResult::Sampled);
    CHECK(fake.initCalls == 2);
    CHECK(PotentiometerVolume_GetReading().valid);
}
}

void HostEnableGPIOFClock() { ++fake.gpioClockCalls; }
void HostEnableADC3Clock() { ++fake.adcClockCalls; }
uint32_t HAL_GetTick() { return fake.tick; }

void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *configuration)
{
    ++fake.gpioCalls;
    fake.gpioPort = port;
    fake.gpioConfiguration = *configuration;
    fake.events.push_back("gpio");
}

HAL_StatusTypeDef HAL_ADC_Init(ADC_HandleTypeDef *handle)
{
    ++fake.initCalls;
    fake.adcConfiguration = *handle;
    fake.events.push_back("init");
    return fake.initStatus;
}

HAL_StatusTypeDef HAL_ADC_ConfigChannel(ADC_HandleTypeDef *handle,
                                       ADC_ChannelConfTypeDef *configuration)
{
    CHECK(handle == &adc);
    ++fake.configCalls;
    fake.channelConfiguration = *configuration;
    fake.events.push_back("config");
    return fake.configStatus;
}

HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *handle)
{
    CHECK(handle == &adc);
    ++fake.startCalls;
    fake.events.push_back("start");
    return fake.startStatus;
}

HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef *handle,
                                          uint32_t timeout)
{
    CHECK(handle == &adc);
    ++fake.pollCalls;
    fake.pollTimeout = timeout;
    fake.events.push_back("poll");
    return fake.pollStatus;
}

uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *handle)
{
    CHECK(handle == &adc);
    ++fake.getValueCalls;
    fake.events.push_back("value");
    return fake.raw;
}

HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef *handle)
{
    CHECK(handle == &adc);
    ++fake.stopCalls;
    fake.events.push_back("stop");
    return fake.stopStatus;
}

int main()
{
    testPinAndAdcConfiguration();
    testValidZeroAndBoundedPolling();
    testFourForcedStartupSamples();
    testSamplingCadenceAndForce();
    testSamplingAcrossTickWrap();
    testInitFailureAndDelayedRecovery();
    testConfigFailureAndDelayedRecovery();
    testStartFailureKeepsAllAcceptedData();
    testPollFailureKeepsAllAcceptedData();
    testStopFailureAndCheckedRecovery();
    testBothStartAndStopFailure();
    testExtremaUseSuccessfulSamplesOnly();
    testInitialConversionFailureRemainsZero();
    testOutOfRangeIsClamped();
    testInitializationRetryAcrossTickWrap();
    std::cout << "PASS: " << scenarios << " ADC hardware/error/timing scenarios, "
              << checks << " checks\n";
}
