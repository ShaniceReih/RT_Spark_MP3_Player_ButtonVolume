#pragma once
// Host-only HAL simulation. Included only by the native test compiler.
#include <cstdint>
#include <cassert>

enum HAL_StatusTypeDef { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT };
enum GPIO_PinState { GPIO_PIN_RESET, GPIO_PIN_SET };
struct GPIO_TypeDef {};
struct SPI_TypeDef { uint32_t CR2 = 0; };
struct DMA_Stream_TypeDef {};
inline GPIO_TypeDef hostGpio;
inline SPI_TypeDef hostSpi;
inline DMA_Stream_TypeDef hostDma;
#define GPIOA (&hostGpio)
#define GPIOB (&hostGpio)
#define GPIOC (&hostGpio)
#define GPIOF (&hostGpio)
#define SPI3 (&hostSpi)
#define DMA1_Stream5 (&hostDma)
#define DMA1_Stream5_IRQn 16
#define GPIO_PIN_0 1
#define GPIO_PIN_1 2
#define GPIO_PIN_3 8
#define GPIO_PIN_5 32
#define GPIO_PIN_7 128
#define GPIO_PIN_15 32768
#define GPIO_MODE_OUTPUT_OD 1
#define GPIO_MODE_AF_PP 2
#define GPIO_PULLUP 1
#define GPIO_NOPULL 0
#define GPIO_SPEED_FREQ_HIGH 2
#define GPIO_SPEED_FREQ_VERY_HIGH 3
#define GPIO_AF6_SPI3 6
#define RCC_PERIPHCLK_I2S 1
#define I2S_MODE_MASTER_TX 1
#define I2S_STANDARD_PHILIPS 0
#define I2S_DATAFORMAT_16B 0
#define I2S_MCLKOUTPUT_ENABLE 1
#define I2S_AUDIOFREQ_44K 44100
#define I2S_CPOL_LOW 0
#define I2S_CLOCK_PLL 0
#define I2S_FULLDUPLEXMODE_DISABLE 0
#define DMA_CHANNEL_0 0
#define DMA_MEMORY_TO_PERIPH 1
#define DMA_PINC_DISABLE 0
#define DMA_MINC_ENABLE 1
#define DMA_PDATAALIGN_HALFWORD 1
#define DMA_MDATAALIGN_HALFWORD 1
#define DMA_CIRCULAR 1
#define DMA_PRIORITY_HIGH 2
#define DMA_FIFOMODE_DISABLE 0
#define SPI_CR2_TXDMAEN 2
#define CLEAR_BIT(REG, BIT) ((REG) &= ~(BIT))
#define __HAL_RCC_GPIOA_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOC_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOF_CLK_ENABLE() ((void)0)
#define __HAL_RCC_SPI3_CLK_ENABLE() ((void)0)
#define __HAL_RCC_DMA1_CLK_ENABLE() ((void)0)
#define __NOP() ((void)0)
#define __DMB() ((void)0)
#define __HAL_LINKDMA(HANDLE, FIELD, DMA) ((HANDLE)->FIELD = &(DMA), (DMA).Parent = (HANDLE))

struct GPIO_InitTypeDef { uint32_t Pin, Mode, Pull, Speed, Alternate; };
struct RCC_PeriphCLKInitTypeDef {
    uint32_t PeriphClockSelection;
    struct { uint32_t PLLI2SN, PLLI2SR; } PLLI2S;
};
struct DMA_HandleTypeDef {
    DMA_Stream_TypeDef *Instance;
    struct {
        uint32_t Channel, Direction, PeriphInc, MemInc, PeriphDataAlignment;
        uint32_t MemDataAlignment, Mode, Priority, FIFOMode;
    } Init;
    void *Parent;
};
struct I2S_HandleTypeDef {
    SPI_TypeDef *Instance;
    struct {
        uint32_t Mode, Standard, DataFormat, MCLKOutput, AudioFreq, CPOL;
        uint32_t ClockSource, FullDuplexMode;
    } Init;
    DMA_HandleTypeDef *hdmatx;
};

inline bool hostInIrq = false;
inline bool hostAck = true;
inline bool hostIrqEnabled = false;
inline bool hostTransportRunning = false;
inline bool hostRequestsEnabled = false;
inline bool hostI2sEnabled = false;
inline HAL_StatusTypeDef hostStartResult = HAL_OK;
inline HAL_StatusTypeDef hostPauseResult = HAL_OK;
inline HAL_StatusTypeDef hostResumeResult = HAL_OK;
inline HAL_StatusTypeDef hostStopResult = HAL_OK;
inline int16_t *hostMemory = nullptr;
inline uint32_t hostTransferWords = 0;
inline uint32_t hostNdtr = 0;
inline unsigned hostStopCalls = 0;
inline unsigned hostGpioCalls = 0;
inline unsigned hostPendingClears = 0;
inline uint32_t hostTick = 0;
inline void (*hostOnGpioWrite)() = nullptr;

inline void HAL_GPIO_WritePin(GPIO_TypeDef *, uint32_t, GPIO_PinState) {
    assert(!hostInIrq && "IRQ called software I2C");
    ++hostGpioCalls;
    if (hostOnGpioWrite) {
        auto callback = hostOnGpioWrite;
        hostOnGpioWrite = nullptr;
        callback();
    }
}
inline GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *, uint32_t) {
    assert(!hostInIrq);
    return hostAck ? GPIO_PIN_RESET : GPIO_PIN_SET;
}
inline void HAL_GPIO_Init(GPIO_TypeDef *, GPIO_InitTypeDef *) {}
inline HAL_StatusTypeDef HAL_RCCEx_PeriphCLKConfig(RCC_PeriphCLKInitTypeDef *) { return HAL_OK; }
inline HAL_StatusTypeDef HAL_I2S_Init(I2S_HandleTypeDef *) { return HAL_OK; }
inline HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef *) { return HAL_OK; }
inline void HAL_NVIC_SetPriority(int, int, int) {}
inline void HAL_NVIC_DisableIRQ(int) { hostIrqEnabled = false; }
inline void HAL_NVIC_EnableIRQ(int) { hostIrqEnabled = true; }
inline void HAL_NVIC_ClearPendingIRQ(int) { ++hostPendingClears; }
inline void HAL_DMA_IRQHandler(DMA_HandleTypeDef *) {}
inline void HAL_Delay(uint32_t ms) { assert(!hostInIrq); hostTick += ms; }
inline uint32_t HAL_GetTick() { assert(!hostInIrq); return ++hostTick; }
inline HAL_StatusTypeDef HAL_I2S_Transmit(I2S_HandleTypeDef *, uint16_t *, uint16_t, uint32_t) {
    assert(!hostInIrq);
    return HAL_OK;
}
inline HAL_StatusTypeDef HAL_I2S_Transmit_DMA(I2S_HandleTypeDef *handle, uint16_t *memory, uint16_t size) {
    assert(!hostInIrq);
    if (hostStartResult == HAL_OK) {
        hostMemory = reinterpret_cast<int16_t *>(memory);
        hostTransferWords = hostNdtr = size;
        hostTransportRunning = hostRequestsEnabled = hostI2sEnabled = true;
        handle->Instance->CR2 |= SPI_CR2_TXDMAEN;
    }
    return hostStartResult;
}
inline HAL_StatusTypeDef HAL_I2S_DMAPause(I2S_HandleTypeDef *handle) {
    assert(!hostInIrq);
    if (hostPauseResult == HAL_OK) {
        hostRequestsEnabled = false;
        handle->Instance->CR2 &= ~SPI_CR2_TXDMAEN;
    }
    return hostPauseResult;
}
inline HAL_StatusTypeDef HAL_I2S_DMAResume(I2S_HandleTypeDef *handle) {
    assert(!hostInIrq);
    if (hostResumeResult == HAL_OK) {
        hostRequestsEnabled = true;
        handle->Instance->CR2 |= SPI_CR2_TXDMAEN;
    }
    return hostResumeResult;
}
inline HAL_StatusTypeDef HAL_I2S_DMAStop(I2S_HandleTypeDef *handle) {
    assert(!hostInIrq && "IRQ called blocking DMAStop");
    ++hostStopCalls;
    if (hostStopResult == HAL_OK) hostTransportRunning = hostRequestsEnabled = false;
    handle->Instance->CR2 &= ~SPI_CR2_TXDMAEN;
    return hostStopResult;
}
#define __HAL_I2S_DISABLE(HANDLE) ((void)(HANDLE), hostI2sEnabled = false)

