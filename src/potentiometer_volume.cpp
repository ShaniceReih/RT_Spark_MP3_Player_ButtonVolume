#include "potentiometer_volume.h"
#include "potentiometer_filter.h"
#include "stm32f4xx_hal.h"

namespace
{
constexpr uint32_t SAMPLE_INTERVAL_MS = 20;
constexpr uint32_t INITIALIZATION_RETRY_MS = 1000;
constexpr uint32_t CONVERSION_TIMEOUT_MS = 1;

ADC_HandleTypeDef adc = {};
PotentiometerFilter filter;
bool initialized = false;
bool initializationAttempted = false;
bool stopNeedsRetry = false;
bool samplingStarted = false;
uint32_t lastInitializationAttempt = 0;
uint32_t lastSample = 0;
uint16_t lastRaw = 0;
uint16_t observedMinimum = 0;
uint16_t observedMaximum = 0;

bool readSingle(uint16_t &raw)
{
    const HAL_StatusTypeDef started = HAL_ADC_Start(&adc);
    HAL_StatusTypeDef converted = HAL_ERROR;
    uint32_t result = 0;
    if (started == HAL_OK)
    {
        converted = HAL_ADC_PollForConversion(&adc, CONVERSION_TIMEOUT_MS);
        if (converted == HAL_OK) result = HAL_ADC_GetValue(&adc);
    }

    // Stop also runs on failed start/poll paths. Never accept a partial read.
    const HAL_StatusTypeDef stopped = HAL_ADC_Stop(&adc);
    if (stopped != HAL_OK)
    {
        initialized = false;
        stopNeedsRetry = true;
        lastInitializationAttempt = HAL_GetTick();
    }
    if (started != HAL_OK || converted != HAL_OK || stopped != HAL_OK)
        return false;

    if (result > PotentiometerFilter::AdcMaximum)
        result = PotentiometerFilter::AdcMaximum;
    raw = static_cast<uint16_t>(result);
    return true;
}
}

bool PotentiometerVolume_Init()
{
    initialized = false;
    initializationAttempted = true;
    lastInitializationAttempt = HAL_GetTick();

    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_ADC3_CLK_ENABLE();

    if (stopNeedsRetry)
    {
        if (HAL_ADC_Stop(&adc) != HAL_OK) return false;
        stopNeedsRetry = false;
    }

    GPIO_InitTypeDef gpio = {};
    gpio.Pin = GPIO_PIN_6;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOF, &gpio);

    adc.Instance = ADC3;
    adc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    adc.Init.Resolution = ADC_RESOLUTION_12B;
    adc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    adc.Init.ScanConvMode = DISABLE;
    adc.Init.ContinuousConvMode = DISABLE;
    adc.Init.DiscontinuousConvMode = DISABLE;
    adc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    adc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    adc.Init.NbrOfConversion = 1;
    adc.Init.DMAContinuousRequests = DISABLE;
    adc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    if (HAL_ADC_Init(&adc) != HAL_OK) return false;

    ADC_ChannelConfTypeDef channel = {};
    channel.Channel = ADC_CHANNEL_4;
    channel.Rank = 1;
    channel.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    if (HAL_ADC_ConfigChannel(&adc, &channel) != HAL_OK) return false;

    initialized = true;
    return true;
}

PotentiometerResult PotentiometerVolume_Update(uint32_t now, bool forceSample)
{
    if (!forceSample && samplingStarted && now - lastSample < SAMPLE_INTERVAL_MS)
        return PotentiometerResult::NotDue;
    samplingStarted = true;
    lastSample = now;

    if (!initialized)
    {
        if (initializationAttempted &&
            now - lastInitializationAttempt < INITIALIZATION_RETRY_MS)
            return PotentiometerResult::NotDue;
        if (!PotentiometerVolume_Init())
            return PotentiometerResult::InitializationFailed;
    }

    uint16_t raw = 0;
    if (!readSingle(raw)) return PotentiometerResult::ConversionFailed;

    if (!filter.hasValidSample()) observedMinimum = observedMaximum = raw;
    else
    {
        if (raw < observedMinimum) observedMinimum = raw;
        if (raw > observedMaximum) observedMaximum = raw;
    }
    lastRaw = raw;
    return filter.addSample(raw) ? PotentiometerResult::VolumeChanged
                                 : PotentiometerResult::Sampled;
}

PotentiometerReading PotentiometerVolume_GetReading()
{
    PotentiometerReading reading;
    reading.raw = lastRaw;
    reading.filteredRaw = filter.filteredRaw();
    reading.observedMinimum = observedMinimum;
    reading.observedMaximum = observedMaximum;
    reading.volume = filter.volume();
    reading.valid = filter.hasValidSample();
    return reading;
}
