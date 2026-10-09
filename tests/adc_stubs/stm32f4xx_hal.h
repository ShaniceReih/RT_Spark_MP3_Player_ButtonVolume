#ifndef POTENTIOMETER_TEST_STM32F4XX_HAL_H
#define POTENTIOMETER_TEST_STM32F4XX_HAL_H

#include <cstdint>

// ADC-only host interface. It deliberately stays separate from the audio
// regression stub so changing one peripheral cannot weaken the other suite.
enum HAL_StatusTypeDef { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT };
struct GPIO_TypeDef {};
struct ADC_TypeDef {};
extern GPIO_TypeDef hostGPIOF;
extern ADC_TypeDef hostADC3;

#define GPIOF (&hostGPIOF)
#define ADC3 (&hostADC3)
#define GPIO_PIN_6 (1u << 6)
#define GPIO_MODE_ANALOG 3u
#define GPIO_NOPULL 0u
#define DISABLE 0u
#define ADC_CLOCK_SYNC_PCLK_DIV4 1u
#define ADC_RESOLUTION_12B 0u
#define ADC_DATAALIGN_RIGHT 0u
#define ADC_EXTERNALTRIGCONVEDGE_NONE 0u
#define ADC_SOFTWARE_START 0x0fu
#define ADC_EOC_SINGLE_CONV 1u
#define ADC_CHANNEL_4 4u
#define ADC_SAMPLETIME_480CYCLES 7u

struct GPIO_InitTypeDef
{
    uint32_t Pin, Mode, Pull, Speed, Alternate;
};

struct ADC_InitTypeDef
{
    uint32_t ClockPrescaler;
    uint32_t Resolution;
    uint32_t DataAlign;
    uint32_t ScanConvMode;
    uint32_t ContinuousConvMode;
    uint32_t DiscontinuousConvMode;
    uint32_t ExternalTrigConvEdge;
    uint32_t ExternalTrigConv;
    uint32_t NbrOfConversion;
    uint32_t DMAContinuousRequests;
    uint32_t EOCSelection;
};

struct ADC_HandleTypeDef
{
    ADC_TypeDef *Instance;
    ADC_InitTypeDef Init;
};

struct ADC_ChannelConfTypeDef
{
    uint32_t Channel, Rank, SamplingTime, Offset;
};

void HostEnableGPIOFClock();
void HostEnableADC3Clock();
#define __HAL_RCC_GPIOF_CLK_ENABLE() HostEnableGPIOFClock()
#define __HAL_RCC_ADC3_CLK_ENABLE() HostEnableADC3Clock()

uint32_t HAL_GetTick();
void HAL_GPIO_Init(GPIO_TypeDef *, GPIO_InitTypeDef *);
HAL_StatusTypeDef HAL_ADC_Init(ADC_HandleTypeDef *);
HAL_StatusTypeDef HAL_ADC_ConfigChannel(ADC_HandleTypeDef *, ADC_ChannelConfTypeDef *);
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *);
HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef *, uint32_t);
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *);
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef *);

#endif
